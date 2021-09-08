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
#include "winuser.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

struct wayland_window_surface
{
    struct window_surface header;
    HWND                  hwnd;
    struct wayland_surface *wayland_surface; /* Not owned by us */
    struct wayland_buffer_queue *wayland_buffer_queue;
    RECT                  bounds;
    void                 *bits;
    struct wayland_mutex  mutex;
    BOOL                  last_flush_failed;
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
    /* TODO */
}

static void wayland_window_surface_copy_to_buffer(struct wayland_window_surface *wws,
                                                  struct wayland_shm_buffer *buffer,
                                                  HRGN region)
{
    RGNDATA *rgndata = get_region_data(region);
    RECT *rgn_rect;
    RECT *rgn_rect_end;

    if (!rgndata) return;

    rgn_rect = (RECT *)rgndata->Buffer;
    rgn_rect_end = rgn_rect + rgndata->rdh.nCount;

    for (;rgn_rect < rgn_rect_end; rgn_rect++)
    {
        unsigned int *src, *dst;
        int y, width, height;

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
        if (width == buffer->width)
        {
            memcpy(dst, src, height * buffer->stride);
            continue;
        }

        for (y = 0; y < height; y++)
        {
            memcpy(dst, src, width * 4);

            src += wws->info.bmiHeader.biWidth;
            dst = (unsigned int*)((unsigned char*)dst + buffer->stride);
        }
    }

    free(rgndata);
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

    surface_damage_region = NtGdiCreateRectRgn(damage_rect.left, damage_rect.top,
                                               damage_rect.right, damage_rect.bottom);

    TRACE("flushing surface %p hwnd %p surface_rect %s bits %p\n",
          wws, wws->hwnd, wine_dbgstr_rect(&wws->header.rect),
          wws->bits);

    assert(wws->wayland_buffer_queue);

    wayland_buffer_queue_add_damage(wws->wayland_buffer_queue, surface_damage_region);
    buffer = wayland_buffer_queue_acquire_buffer(wws->wayland_buffer_queue);
    if (!buffer)
    {
        WARN("failed to acquire wayland buffer, returning\n");
        wws->last_flush_failed = TRUE;
        goto done;
    }

    last_buffer = get_last_flushed_buffer(wws->hwnd);

    if (last_buffer)
    {
        if (last_buffer != buffer)
        {
            HRGN copy_from_last_region = NtGdiCreateRectRgn(0, 0, 0, 0);
            NtGdiCombineRgn(copy_from_last_region, buffer->damage_region,
                            surface_damage_region, RGN_DIFF);
            wayland_shm_buffer_copy(buffer, last_buffer, copy_from_last_region);
            NtGdiDeleteObjectApp(copy_from_last_region);
        }
        copy_from_window_region = surface_damage_region;
    }
    else
    {
        copy_from_window_region = buffer->damage_region;
    }

    wayland_window_surface_copy_to_buffer(wws, buffer, buffer->damage_region);

    if (!wayland_surface_commit_buffer(wws->wayland_surface, buffer,
                                       surface_damage_region))
    {
        wws->last_flush_failed = TRUE;
    }

    wayland_shm_buffer_clear_damage(buffer);
    update_last_flushed_buffer(wws->hwnd, buffer);

done:
    if (!wws->last_flush_failed) reset_bounds(&wws->bounds);
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
    if (wws->wayland_surface) wayland_surface_unref(wws->wayland_surface);
    if (wws->wayland_buffer_queue)
        wayland_window_surface_destroy_buffer_queue(wws);
    free(wws->bits);
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
struct window_surface *wayland_window_surface_create(HWND hwnd, const RECT *rect)
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
                    WL_SHM_FORMAT_XRGB8888);
    }
    else if (!wws->wayland_surface && wws->wayland_buffer_queue)
    {
        wayland_window_surface_destroy_buffer_queue(wws);
    }

    window_surface->funcs->unlock(window_surface);
}

/***********************************************************************
 *           wayland_clear_window_surface_last_flushed
 */
void wayland_clear_window_surface_last_flushed(HWND hwnd)
{
    update_last_flushed_buffer(hwnd, NULL);
}
