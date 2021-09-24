/*
 * Wayland window surface implementation
 *
 * Copyright 1993, 1994, 1995, 1996, 2001, 2013-2017 Alexandre Julliard
 * Copyright 1993 David Metcalfe
 * Copyright 1995, 1996 Alex Korobka
 * Copyright 2020 Alexandros Frantzis for Collabora Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include "waylanddrv.h"

#include <assert.h>
#include <limits.h>
#include <stdlib.h>

#include "ntgdi.h"
#include "ntuser.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

/* Change to 1 to dump flushed surface buffer contents to disk */
#define DEBUG_DUMP_FLUSH_SURFACE_BUFFER 0

/* Change to 1 to dump front buffer contents to disk when performing front
 * buffer rendering. */
#define DEBUG_DUMP_FRONT_BUFFER 0

struct wayland_window_surface
{
    struct window_surface header;
    HWND                  hwnd;
    struct wayland_surface *wayland_surface; /* Not owned by us */
    struct wayland_buffer_queue *wayland_buffer_queue;
    RECT                  bounds;
    HRGN                  region; /* region set through window_surface funcs */
    HRGN                  total_region; /* Total region (surface->region AND window_region) */
    COLORREF              color_key;
    BYTE                  alpha;
    BOOL                  src_alpha;
    void                 *bits;
    struct wayland_mutex  mutex;
    BOOL                  last_flush_failed;
    void                 *front_bits; /* Front buffer pixels, stored bottom to top */
    BOOL                  front_bits_dirty;
    BITMAPINFO            info;
};

struct last_flushed
{
    struct wl_list link;
    HWND hwnd;
    struct wayland_shm_buffer *buffer;
    BOOL owned;
};

static struct wayland_mutex last_flushed_mutex =
{
    PTHREAD_MUTEX_INITIALIZER, 0, 0, __FILE__ ": last_flushed_mutex"
};

static struct wl_list last_flushed_list = {&last_flushed_list, &last_flushed_list};

static struct last_flushed *last_flushed_get(HWND hwnd)
{
    struct last_flushed *last_flushed;

    wayland_mutex_lock(&last_flushed_mutex);

    wl_list_for_each(last_flushed, &last_flushed_list, link)
        if (last_flushed->hwnd == hwnd) return last_flushed;

    wayland_mutex_unlock(&last_flushed_mutex);

    return NULL;
}

static void last_flushed_release(struct last_flushed *last_flushed)
{
    if (last_flushed) wayland_mutex_unlock(&last_flushed_mutex);
}

static struct wayland_shm_buffer *get_last_flushed_buffer(HWND hwnd)
{
    struct last_flushed *last_flushed = last_flushed_get(hwnd);
    struct wayland_shm_buffer *prev_flushed = last_flushed ? last_flushed->buffer : NULL;
    last_flushed_release(last_flushed);
    return prev_flushed;
}

static void update_last_flushed_buffer(HWND hwnd, struct wayland_shm_buffer *buffer)
{
    struct last_flushed *last_flushed;

    last_flushed = last_flushed_get(hwnd);

    TRACE("hwnd=%p buffer=%p (old_buffer=%p owned=%d)\n",
          hwnd, buffer, last_flushed ? last_flushed->buffer : NULL,
          last_flushed ? last_flushed->owned : FALSE);

    if (last_flushed && last_flushed->owned)
    {
        if (last_flushed->buffer->busy)
            last_flushed->buffer->destroy_on_release = TRUE;
        else
            wayland_shm_buffer_destroy(last_flushed->buffer);
    }

    if (buffer)
    {
        if (!last_flushed)
        {
            last_flushed = calloc(1, sizeof(*last_flushed));
            last_flushed->hwnd = hwnd;
            wayland_mutex_lock(&last_flushed_mutex);
            wl_list_insert(&last_flushed_list, &last_flushed->link);
        }
        last_flushed->buffer = buffer;
        last_flushed->owned = FALSE;
    }
    else if (last_flushed)
    {
        wl_list_remove(&last_flushed->link);
        free(last_flushed);
    }

    last_flushed_release(last_flushed);
}

static void wayland_window_surface_destroy_buffer_queue(struct wayland_window_surface *wws)
{
    struct last_flushed *last_flushed = last_flushed_get(wws->hwnd);

    /* Ensure the last flushed buffer is kept alive, so that we are able to
     * copy data from it in later flushes for the same HWND, if needed. */
    if (last_flushed && !last_flushed->owned)
    {
        wayland_buffer_queue_detach_buffer(wws->wayland_buffer_queue,
                                           last_flushed->buffer, FALSE);
        last_flushed->owned = TRUE;
    }

    last_flushed_release(last_flushed);

    wayland_buffer_queue_destroy(wws->wayland_buffer_queue);
    wws->wayland_buffer_queue = NULL;
}

static struct wayland_window_surface *wayland_window_surface_cast(
    struct window_surface *window_surface)
{
    return (struct wayland_window_surface *)window_surface;
}

static inline int get_dib_stride(int width, int bpp)
{
    return ((width * bpp + 31) >> 3) & ~3;
}

static inline int get_dib_image_size(const BITMAPINFO *info)
{
    return get_dib_stride(info->bmiHeader.biWidth, info->bmiHeader.biBitCount) *
           abs(info->bmiHeader.biHeight);
}

static inline void reset_bounds(RECT *bounds)
{
    bounds->left = bounds->top = INT_MAX;
    bounds->right = bounds->bottom = INT_MIN;
}

/***********************************************************************
 *           wayland_window_surface_preferred_format
 */
static int get_preferred_format(struct wayland_window_surface *wws)
{
    int format;
    HRGN window_region = NtGdiCreateRectRgn(0, 0, 0, 0);

    /* Use ARGB to implement window regions (areas out of the region are
     * transparent). */
    if ((window_region && NtUserGetWindowRgnEx(wws->hwnd, window_region, 0) != ERROR) ||
        wws->color_key != CLR_INVALID || wws->alpha != 255 || wws->src_alpha)
        format = WL_SHM_FORMAT_ARGB8888;
    else
        format = WL_SHM_FORMAT_XRGB8888;

    if (window_region) NtGdiDeleteObjectApp(window_region);

    return format;
}

/***********************************************************************
 *           recreate_wayland_buffer_queue
 */
static void recreate_wayland_buffer_queue(struct wayland_window_surface *wws)
{
    int width;
    int height;
    int format;

    if (!wws->wayland_buffer_queue || !wws->wayland_surface) return;

    width = wws->wayland_buffer_queue->width;
    height = wws->wayland_buffer_queue->height;
    format = get_preferred_format(wws);

    wayland_window_surface_destroy_buffer_queue(wws);

    wws->wayland_buffer_queue =
        wayland_buffer_queue_create(wws->wayland_surface->wayland,
                                    width, height, format);
}

/***********************************************************************
 *           wayland_window_surface_set_window_region
 */
void wayland_window_surface_set_window_region(struct window_surface *window_surface,
                                              HRGN win_region)
{
    struct wayland_window_surface *wws =
        wayland_window_surface_cast(window_surface);
    HRGN region = 0;

    TRACE("hwnd %p surface %p region %p\n", wws->hwnd, wws, win_region);

    if (win_region == (HRGN)1)  /* hack: win_region == 1 means retrieve region from server */
    {
        region = NtGdiCreateRectRgn(0, 0, 0, 0);
        if (region && NtUserGetWindowRgnEx(wws->hwnd, region, 0) == ERROR)
        {
            NtGdiDeleteObjectApp(region);
            region = 0;
        }
    }
    else if (win_region)
    {
        region = NtGdiCreateRectRgn(0, 0, 0, 0);
        if (region) NtGdiCombineRgn(region, win_region, 0, RGN_COPY);
    }

    if (wws->region)
    {
        if (region)
        {
            NtGdiCombineRgn(region, region, wws->region, RGN_AND);
        }
        else
        {
            region = NtGdiCreateRectRgn(0, 0, 0, 0);
            if (region) NtGdiCombineRgn(region, wws->region, 0, RGN_COPY);
        }
    }

    window_surface->funcs->lock(window_surface);

    if (wws->total_region) NtGdiDeleteObjectApp(wws->total_region);
    wws->total_region = region;
    *window_surface->funcs->get_bounds(window_surface) = wws->header.rect;
    /* Unconditionally recreate the buffer queue to ensure we have clean buffers, so
     * that areas outside the region are transparent. */
    recreate_wayland_buffer_queue(wws);

    TRACE("hwnd %p bounds %s rect %s\n", wws->hwnd,
          wine_dbgstr_rect(window_surface->funcs->get_bounds(window_surface)),
          wine_dbgstr_rect(&wws->header.rect));

    window_surface->funcs->unlock(window_surface);
}

/***********************************************************************
 *           wayland_window_surface_lock
 */
static void wayland_window_surface_lock(struct window_surface *window_surface)
{
    struct wayland_window_surface *wws = wayland_window_surface_cast(window_surface);
    wayland_mutex_lock(&wws->mutex);
}

/***********************************************************************
 *           wayland_window_surface_unlock
 */
static void wayland_window_surface_unlock(struct window_surface *window_surface)
{
    struct wayland_window_surface *wws = wayland_window_surface_cast(window_surface);
    wayland_mutex_unlock(&wws->mutex);
}

/***********************************************************************
 *           wayland_window_surface_get_bitmap_info
 */
static void *wayland_window_surface_get_bitmap_info(struct window_surface *window_surface,
                                                    BITMAPINFO *info)
{
    struct wayland_window_surface *surface = wayland_window_surface_cast(window_surface);
    /* We don't store any additional information at the end of our BITMAPINFO, so
     * just copy the structure itself. */
    memcpy(info, &surface->info, sizeof(*info));
    return surface->bits;
}

/***********************************************************************
 *           wayland_window_surface_get_bounds
 */
static RECT *wayland_window_surface_get_bounds(struct window_surface *window_surface)
{
    struct wayland_window_surface *wws = wayland_window_surface_cast(window_surface);
    return &wws->bounds;
}

/***********************************************************************
 *           wayland_window_surface_set_region
 */
static void wayland_window_surface_set_region(struct window_surface *window_surface,
                                              HRGN region)
{
    struct wayland_window_surface *wws = wayland_window_surface_cast(window_surface);

    TRACE("updating hwnd=%p surface=%p region=%p\n", wws->hwnd, wws, region);

    window_surface->funcs->lock(window_surface);
    if (!region)
    {
        if (wws->region) NtGdiDeleteObjectApp(wws->region);
        wws->region = NULL;
    }
    else
    {
        if (!wws->region) wws->region = NtGdiCreateRectRgn(0, 0, 0, 0);
        NtGdiCombineRgn(wws->region, region, 0, RGN_COPY);
    }
    window_surface->funcs->unlock(window_surface);
    wayland_window_surface_set_window_region(&wws->header, (HRGN)1);
}

static void wayland_window_surface_copy_to_buffer(struct wayland_window_surface *wws,
                                                  struct wayland_shm_buffer *buffer,
                                                  HRGN region)
{
    RGNDATA *rgndata = get_region_data(region);
    RECT *rgn_rect;
    RECT *rgn_rect_end;
    BOOL apply_surface_alpha;

    if (!rgndata) return;

    rgn_rect = (RECT *)rgndata->Buffer;
    rgn_rect_end = rgn_rect + rgndata->rdh.nCount;

    /* If we have an ARGB buffer we need to explicitly apply the surface
     * alpha to ensure the destination has sensible alpha values. The
     * exception is when the surface uses source alpha values and the
     * surface alpha is 255, in which case we can just copy pixel values
     * as they are. */
    apply_surface_alpha = buffer->format == WL_SHM_FORMAT_ARGB8888 &&
                          (wws->alpha != 255 || !wws->src_alpha);

    for (;rgn_rect < rgn_rect_end; rgn_rect++)
    {
        unsigned int *src, *dst;
        int x, y, width, height;

        TRACE("rect %s\n", wine_dbgstr_rect(rgn_rect));

        if (IsRectEmpty(rgn_rect))
            continue;

        src = (unsigned int *)wws->bits +
              rgn_rect->top * wws->info.bmiHeader.biWidth +
              rgn_rect->left;
        dst = (unsigned int *)((unsigned char *)buffer->map_data +
              rgn_rect->top * buffer->stride +
              rgn_rect->left * 4);
        width = min(rgn_rect->right, buffer->width) - rgn_rect->left;
        height = min(rgn_rect->bottom, buffer->height) - rgn_rect->top;

        /* Fast path for full width rectangles. */
        if (width == buffer->width && !apply_surface_alpha &&
            wws->color_key == CLR_INVALID)
        {
            memcpy(dst, src, height * buffer->stride);
            continue;
        }

        for (y = 0; y < height; y++)
        {
            if (!apply_surface_alpha)
            {
                memcpy(dst, src, width * 4);
            }
            else if (wws->alpha == 255 && !wws->src_alpha)
            {
                for (x = 0; x < width; x++)
                    dst[x] = 0xff000000 | src[x];
            }
            else if (!wws->src_alpha)
            {
                for (x = 0; x < width; x++)
                {
                    dst[x] = ((wws->alpha << 24) |
                              (((BYTE)(src[x] >> 16) * wws->alpha / 255) << 16) |
                              (((BYTE)(src[x] >> 8) * wws->alpha / 255) << 8) |
                              (((BYTE)src[x] * wws->alpha / 255)));
                }
            }
            else
            {
                for (x = 0; x < width; x++)
                {
                    dst[x] = ((((BYTE)(src[x] >> 24) * wws->alpha / 255) << 24) |
                              (((BYTE)(src[x] >> 16) * wws->alpha / 255) << 16) |
                              (((BYTE)(src[x] >> 8) * wws->alpha / 255) << 8) |
                              (((BYTE)src[x] * wws->alpha / 255)));
                }
            }

            if (wws->color_key != CLR_INVALID)
                for (x = 0; x < width; x++) if ((src[x] & 0xffffff) == wws->color_key) dst[x] = 0;

            src += wws->info.bmiHeader.biWidth;
            dst = (unsigned int*)((unsigned char*)dst + buffer->stride);
        }
    }

    free(rgndata);
}

static void wayland_window_surface_copy_front_to_buffer(struct wayland_window_surface *wws,
                                                        struct wayland_shm_buffer *buffer)
{
    int width = min(wws->info.bmiHeader.biWidth, buffer->width);
    int height = min(abs(wws->info.bmiHeader.biHeight), buffer->height);
    int stride = width * 4;
    unsigned char *src = wws->front_bits;
    unsigned char *dst = buffer->map_data;
    int src_stride = wws->info.bmiHeader.biWidth * 4;
    int dst_stride = buffer->width * 4;
    int i;

    TRACE("front buffer %p -> %p %dx%d\n", src, dst, width, height);

    /* Front buffer lines are stored bottom to top, so we need to flip
     * when copying to our buffer. */
    for (i = 0; i < height; i++)
    {
        memcpy(dst + (height - i - 1) * dst_stride,
               src + i * src_stride,
               stride);
    }
}

/***********************************************************************
 *           wayland_window_surface_flush
 */
void wayland_window_surface_flush(struct window_surface *window_surface)
{
    struct wayland_window_surface *wws = wayland_window_surface_cast(window_surface);
    struct wayland_shm_buffer *buffer, *last_buffer;
    RECT damage_rect;
    BOOL needs_flush;
    HRGN surface_damage_region = NULL;
    HRGN copy_from_window_region;

    window_surface->funcs->lock(window_surface);

    TRACE("hwnd=%p surface_rect=%s bounds=%s\n", wws->hwnd,
          wine_dbgstr_rect(&wws->header.rect), wine_dbgstr_rect(&wws->bounds));

    needs_flush = intersect_rect(&damage_rect, &wws->header.rect, &wws->bounds);
    if (needs_flush)
    {
        RECT total_region_box;
        surface_damage_region = NtGdiCreateRectRgn(damage_rect.left, damage_rect.top,
                                                   damage_rect.right, damage_rect.bottom);
        /* If the total_region is empty we are guaranteed to have empty SHM
         * buffers. In order for this empty content to take effect, we still
         * need to commit with non-empty damage, so don't AND with the
         * total_region in this case, to ensure we don't end up with an empty
         * surface_damage_region. */
        if (wws->total_region &&
            NtGdiGetRgnBox(wws->total_region, &total_region_box) != NULLREGION)
        {
            needs_flush = NtGdiCombineRgn(surface_damage_region, surface_damage_region,
                                          wws->total_region, RGN_AND);
        }
    }

    /* If we have a front buffer we always copy it to the buffer before copying
     * the window surface contents, so the whole surface is considered damaged.
     * We also damage the whole surface if we just cleared the front buffer
     * (i.e., front_bits == NULL and front_bits_dirty == TRUE). */
    if (wws->front_bits || wws->front_bits_dirty)
    {
        needs_flush |= wws->front_bits_dirty;
        if (needs_flush)
        {
            if (surface_damage_region)
            {
                NtGdiSetRectRgn(surface_damage_region,
                                wws->header.rect.left, wws->header.rect.top,
                                wws->header.rect.right, wws->header.rect.bottom);
            }
            else
            {
                surface_damage_region = NtGdiCreateRectRgn(wws->header.rect.left,
                                                           wws->header.rect.top,
                                                           wws->header.rect.right,
                                                           wws->header.rect.bottom);
            }
        }
    }

    if (needs_flush && (!wws->wayland_surface || !wws->wayland_buffer_queue))
    {
        TRACE("missing wayland surface=%p buffer_queue=%p, returning\n",
              wws->wayland_surface, wws->wayland_buffer_queue);
        wws->last_flush_failed = TRUE;
        goto done;
    }

    if (needs_flush)
    {
        BOOL drawing_allowed;
        wayland_mutex_lock(&wws->wayland_surface->mutex);
        drawing_allowed = wws->wayland_surface->drawing_allowed;
        wayland_mutex_unlock(&wws->wayland_surface->mutex);
        if (!drawing_allowed)
        {
            TRACE("drawing disallowed on wayland surface=%p, returning\n",
                  wws->wayland_surface);
            wws->last_flush_failed = TRUE;
            goto done;
        }
    }

    wws->last_flush_failed = FALSE;

    if (!needs_flush) goto done;

    TRACE("flushing surface %p hwnd %p surface_rect %s bits %p color_key %08x "
          "alpha %02x src_alpha %d compression %d region %p\n",
          wws, wws->hwnd, wine_dbgstr_rect(&wws->header.rect),
          wws->bits, (UINT)wws->color_key, wws->alpha, wws->src_alpha,
          (UINT)wws->info.bmiHeader.biCompression,
          wws->total_region);

    assert(wws->wayland_buffer_queue);

    if (DEBUG_DUMP_FLUSH_SURFACE_BUFFER)
    {
        static int dbgid = 0;
        dump_pixels("/tmp/winewaylanddbg/flush-%.4d.pam", dbgid++, wws->bits,
                    wws->info.bmiHeader.biWidth, abs(wws->info.bmiHeader.biHeight),
                    wws->wayland_buffer_queue->format == WL_SHM_FORMAT_ARGB8888,
                    surface_damage_region, wws->total_region);
    }

    wayland_buffer_queue_add_damage(wws->wayland_buffer_queue, surface_damage_region);
    buffer = wayland_buffer_queue_acquire_buffer(wws->wayland_buffer_queue);
    if (!buffer)
    {
        WARN("failed to acquire wayland buffer, returning\n");
        wws->last_flush_failed = TRUE;
        goto done;
    }

    if (wws->front_bits)
        wayland_window_surface_copy_front_to_buffer(wws, buffer);

    /* If we have a front buffer, the whole window is overwritten in every
     * flush, and all "overlay" contents will need to be reapplied
     * from the window surface, rather than from the last buffer. */
    if (!wws->front_bits && (last_buffer = get_last_flushed_buffer(wws->hwnd)))
    {
        if (last_buffer != buffer)
        {
            HRGN copy_from_last_region = NtGdiCreateRectRgn(0, 0, 0, 0);
            NtGdiCombineRgn(copy_from_last_region, buffer->damage_region,
                            surface_damage_region, RGN_DIFF);
            if (wws->total_region)
            {
                NtGdiCombineRgn(copy_from_last_region, copy_from_last_region,
                                wws->total_region, RGN_AND);
            }
            wayland_shm_buffer_copy(buffer, last_buffer, copy_from_last_region);
            NtGdiDeleteObjectApp(copy_from_last_region);
        }
        copy_from_window_region = surface_damage_region;
    }
    else if (wws->total_region)
    {
        copy_from_window_region = NtGdiCreateRectRgn(0, 0, 0, 0);
        NtGdiCombineRgn(copy_from_window_region, buffer->damage_region,
                        wws->total_region, RGN_AND);
    }
    else
    {
        copy_from_window_region = buffer->damage_region;
    }

    wayland_window_surface_copy_to_buffer(wws, buffer, copy_from_window_region);

    if (copy_from_window_region != surface_damage_region &&
        copy_from_window_region != buffer->damage_region)
        NtGdiDeleteObjectApp(copy_from_window_region);

    if (!wayland_surface_commit_buffer(wws->wayland_surface, buffer,
                                       surface_damage_region))
    {
        wws->last_flush_failed = TRUE;
    }

    wayland_shm_buffer_clear_damage(buffer);
    update_last_flushed_buffer(wws->hwnd, buffer);

done:
    if (!wws->last_flush_failed)
    {
        reset_bounds(&wws->bounds);
        wws->front_bits_dirty = FALSE;
    }
    if (surface_damage_region) NtGdiDeleteObjectApp(surface_damage_region);
    window_surface->funcs->unlock(window_surface);
}

/***********************************************************************
 *           wayland_window_surface_destroy
 */
static void wayland_window_surface_destroy(struct window_surface *window_surface)
{
    struct wayland_window_surface *wws = wayland_window_surface_cast(window_surface);

    TRACE("surface=%p\n", wws);

    wayland_mutex_destroy(&wws->mutex);
    if (wws->region) NtGdiDeleteObjectApp(wws->region);
    if (wws->total_region) NtGdiDeleteObjectApp(wws->total_region);
    if (wws->wayland_surface) wayland_surface_unref(wws->wayland_surface);
    if (wws->wayland_buffer_queue)
        wayland_window_surface_destroy_buffer_queue(wws);
    free(wws->bits);
    free(wws->front_bits);
    free(wws);
}

static const struct window_surface_funcs wayland_window_surface_funcs =
{
    wayland_window_surface_lock,
    wayland_window_surface_unlock,
    wayland_window_surface_get_bitmap_info,
    wayland_window_surface_get_bounds,
    wayland_window_surface_set_region,
    wayland_window_surface_flush,
    wayland_window_surface_destroy
};

/***********************************************************************
 *           wayland_window_surface_create
 */
struct window_surface *wayland_window_surface_create(HWND hwnd, const RECT *rect,
                                                     COLORREF color_key, BYTE alpha,
                                                     BOOL src_alpha)
{
    struct wayland_window_surface *wws;
    int width = rect->right - rect->left, height = rect->bottom - rect->top;

    TRACE("win %p rect %s\n", hwnd, wine_dbgstr_rect(rect));
    wws = calloc(1, sizeof(*wws));
    if (!wws) return NULL;
    wws->info.bmiHeader.biSize = sizeof(wws->info.bmiHeader);
    wws->info.bmiHeader.biClrUsed = 0;
    wws->info.bmiHeader.biBitCount = 32;
    wws->info.bmiHeader.biCompression = BI_RGB;
    wws->info.bmiHeader.biWidth       = width;
    wws->info.bmiHeader.biHeight      = -height; /* top-down */
    wws->info.bmiHeader.biPlanes      = 1;
    wws->info.bmiHeader.biSizeImage   = get_dib_image_size(&wws->info);

    wayland_mutex_init(&wws->mutex, PTHREAD_MUTEX_RECURSIVE,
                       __FILE__ ": wayland_window_surface");

    wws->header.funcs = &wayland_window_surface_funcs;
    wws->header.rect  = *rect;
    wws->header.ref   = 1;
    wws->hwnd         = hwnd;
    wws->color_key    = color_key;
    wws->alpha        = alpha;
    wws->src_alpha    = src_alpha;
    wws->front_bits   = NULL;
    wws->front_bits_dirty = FALSE;
    wayland_window_surface_set_window_region(&wws->header, (HRGN)1);
    reset_bounds(&wws->bounds);

    if (!(wws->bits = malloc(wws->info.bmiHeader.biSizeImage)))
        goto failed;

    TRACE("created %p hwnd %p %s bits %p-%p compression %u\n", wws, hwnd, wine_dbgstr_rect(rect),
           wws->bits, (char *)wws->bits + wws->info.bmiHeader.biSizeImage,
           (UINT)wws->info.bmiHeader.biCompression);

    return &wws->header;

failed:
    wayland_window_surface_destroy(&wws->header);
    return NULL;
}

/***********************************************************************
 *           wayland_window_surface_needs_flush
 */
BOOL wayland_window_surface_needs_flush(struct window_surface *window_surface)
{
    struct wayland_window_surface *wws = wayland_window_surface_cast(window_surface);
    return wws->last_flush_failed;
}

/***********************************************************************
 *           wayland_window_surface_update_wayland_surface
 */
void wayland_window_surface_update_wayland_surface(struct window_surface *window_surface,
                                                   struct wayland_surface *wayland_surface)
{
    struct wayland_window_surface *wws = wayland_window_surface_cast(window_surface);

    window_surface->funcs->lock(window_surface);

    if (wayland_surface) wayland_surface_ref(wayland_surface);
    if (wws->wayland_surface) wayland_surface_unref(wws->wayland_surface);
    wws->wayland_surface = wayland_surface;

    /* We only need a buffer queue if we have a surface to commit to. */
    if (wws->wayland_surface && !wws->wayland_buffer_queue)
    {
        wws->wayland_buffer_queue =
            wayland_buffer_queue_create(wws->wayland_surface->wayland,
                    wws->info.bmiHeader.biWidth, abs(wws->info.bmiHeader.biHeight),
                    get_preferred_format(wws));
    }
    else if (!wws->wayland_surface)
    {
        if (wws->wayland_buffer_queue)
            wayland_window_surface_destroy_buffer_queue(wws);
        free(wws->front_bits);
        wws->front_bits = NULL;
        wws->front_bits_dirty = FALSE;
    }

    window_surface->funcs->unlock(window_surface);
}

/***********************************************************************
 *           wayland_window_surface_update_layered
 */
void wayland_window_surface_update_layered(struct window_surface *window_surface,
                                           COLORREF color_key, BYTE alpha,
                                           BOOL src_alpha)
{
    struct wayland_window_surface *wws = wayland_window_surface_cast(window_surface);

    window_surface->funcs->lock(window_surface);

    if (alpha != wws->alpha || color_key != wws->color_key || src_alpha != wws->src_alpha)
        *window_surface->funcs->get_bounds(window_surface) = wws->header.rect;

    wws->alpha = alpha;
    wws->color_key = color_key;
    wws->src_alpha = src_alpha;

    if (wws->wayland_buffer_queue &&
        wws->wayland_buffer_queue->format != get_preferred_format(wws))
    {
        recreate_wayland_buffer_queue(wws);
    }

    window_surface->funcs->unlock(window_surface);
}

/***********************************************************************
 *           wayland_window_surface_update_front_buffer
 */
void wayland_window_surface_update_front_buffer(struct window_surface *window_surface,
                                                void (*read_pixels)(void *pixels_out,
                                                                    int width, int height))
{
    struct wayland_window_surface *wws = wayland_window_surface_cast(window_surface);

    TRACE("hwnd=%p front_bits=%p read_pixels=%p size=%dx%d\n",
          wws->hwnd, wws->front_bits, read_pixels,
          (int)wws->info.bmiHeader.biWidth, abs(wws->info.bmiHeader.biHeight));

    window_surface->funcs->lock(window_surface);

    if (!read_pixels)
    {
        if (wws->front_bits)
        {
            free(wws->front_bits);
            wws->front_bits = NULL;
            /* When the front_bits are first invalidated, we mark them as dirty
             * to force the next window_surface flush. */
            wws->front_bits_dirty = TRUE;
        }
        goto out;
    }

    if (!wws->front_bits)
        wws->front_bits = malloc(wws->info.bmiHeader.biSizeImage);

    if (wws->front_bits)
    {
        (*read_pixels)(wws->front_bits, wws->info.bmiHeader.biWidth,
                       abs(wws->info.bmiHeader.biHeight));
        wws->front_bits_dirty = TRUE;
    }
    else
    {
        WARN("Failed to allocate memory for front buffer pixels\n");
    }

    if (DEBUG_DUMP_FRONT_BUFFER && wws->front_bits)
    {
        static int dbgid = 0;
        dump_pixels("/tmp/winewaylanddbg/front-%.4d.pam", dbgid++,
                    wws->front_bits, wws->info.bmiHeader.biWidth,
                    abs(wws->info.bmiHeader.biHeight),
                    FALSE, NULL, NULL);
    }

out:
    window_surface->funcs->unlock(window_surface);
}

/***********************************************************************
 *           wayland_clear_window_surface_last_flushed
 */
void wayland_clear_window_surface_last_flushed(HWND hwnd)
{
    update_last_flushed_buffer(hwnd, NULL);
}
