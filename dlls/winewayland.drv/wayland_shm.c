/*
 * Wayland SHM buffers
 *
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

#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "waylanddrv.h"
#include "wine/debug.h"
#include "ntgdi.h"

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

static struct wayland_shm_buffer *wayland_shm_buffer_create_from_native(struct wayland *wayland,
                                                                        struct wayland_native_buffer *native)
{
    struct wayland_shm_buffer *shm_buffer;
    struct wl_shm_pool *pool;
    int size;
    void *data;

    shm_buffer = calloc(1, sizeof(*shm_buffer));
    if (!shm_buffer)
        goto err;

    wl_list_init(&shm_buffer->link);

    size = native->strides[0] * native->height;

    TRACE("%p %dx%d format=%d size=%d\n",
          shm_buffer, native->width, native->height, native->format, size);

    if (size == 0)
        goto err;

    data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, native->fds[0], 0);
    if (data == MAP_FAILED)
    {
        ERR("mmap failed: %s size=%d\n", strerror(errno), size);
        goto err;
    }

    pool = wl_shm_create_pool(wayland->wl_shm, native->fds[0], size);
    shm_buffer->wl_buffer = wl_shm_pool_create_buffer(pool, 0, native->width, native->height,
                                                      native->strides[0], native->format);
    wl_shm_pool_destroy(pool);

    shm_buffer->width = native->width;
    shm_buffer->height = native->height;
    shm_buffer->stride = native->strides[0];
    shm_buffer->format = native->format;
    shm_buffer->map_data = data;
    shm_buffer->map_size = size;
    shm_buffer->damage_region = NtGdiCreateRectRgn(0, 0, 0, 0);
    if (!shm_buffer->damage_region)
    {
        ERR("failed to create buffer damage region\n");
        goto err;
    }

    TRACE("%p %dx%d size=%d => map=%p\n",
          shm_buffer, native->width, native->height, size, data);

    return shm_buffer;

err:
    if (shm_buffer)
        wayland_shm_buffer_destroy(shm_buffer);
    return NULL;
}

/**********************************************************************
 *          wayland_shm_buffer_create
 *
 * Creates a SHM buffer with the specified width, height and format.
 */
struct wayland_shm_buffer *wayland_shm_buffer_create(struct wayland *wayland,
                                                     int width, int height,
                                                     enum wl_shm_format format)
{
    struct wayland_native_buffer native;
    struct wayland_shm_buffer *shm_buffer;

    if (wayland_native_buffer_init_shm(&native, width, height, format))
    {
        shm_buffer = wayland_shm_buffer_create_from_native(wayland, &native);
        wayland_native_buffer_deinit(&native);
    }
    else
    {
        shm_buffer = NULL;
    }

    return shm_buffer;
}

/**********************************************************************
 *          wayland_shm_buffer_destroy
 *
 * Destroys a SHM buffer.
 */
void wayland_shm_buffer_destroy(struct wayland_shm_buffer *shm_buffer)
{
    TRACE("%p map=%p\n", shm_buffer, shm_buffer->map_data);

    wl_list_remove(&shm_buffer->link);

    if (shm_buffer->wl_buffer)
        wl_buffer_destroy(shm_buffer->wl_buffer);
    if (shm_buffer->map_data)
        munmap(shm_buffer->map_data, shm_buffer->map_size);
    if (shm_buffer->damage_region)
        NtGdiDeleteObjectApp(shm_buffer->damage_region);

    free(shm_buffer);
}

/**********************************************************************
 *          wayland_shm_buffer_steal_wl_buffer_and_destroy
 *
 * Steal the wl_buffer from a SHM buffer and destroy the SHM buffer.
 */
struct wl_buffer *wayland_shm_buffer_steal_wl_buffer_and_destroy(struct wayland_shm_buffer *shm_buffer)
{
    struct wl_buffer *wl_buffer;

    wl_buffer = shm_buffer->wl_buffer;
    shm_buffer->wl_buffer = NULL;

    wayland_shm_buffer_destroy(shm_buffer);

    return wl_buffer;
}

/**********************************************************************
 *          wayland_shm_buffer_clear_damage
 *
 *  Clears all damage accumulated by a SHM buffer.
 */
void wayland_shm_buffer_clear_damage(struct wayland_shm_buffer *shm_buffer)
{
    NtGdiSetRectRgn(shm_buffer->damage_region, 0, 0, 0, 0);
}

/**********************************************************************
 *          wayland_shm_buffer_add_damage
 *
 *  Adds damage (i.e., a region which needs update) to a SHM buffer.
 */
void wayland_shm_buffer_add_damage(struct wayland_shm_buffer *shm_buffer, HRGN damage)
{
    NtGdiCombineRgn(shm_buffer->damage_region, shm_buffer->damage_region, damage, RGN_OR);
}

/**********************************************************************
 *          wayland_shm_buffer_copy
 *
 *  Copies a region from one SHM buffer to another. The region is clipped
 *  within the source and destination rectangles.
 */
void wayland_shm_buffer_copy(struct wayland_shm_buffer *dst_buffer,
                             struct wayland_shm_buffer *src_buffer,
                             HRGN region)
{
    static const int bpp = 4; /* All buffers we use have 4 bytes per pixel */
    RGNDATA *rgndata = get_region_data(region);
    RECT *rgn_rect;
    RECT *rgn_rect_end;
    RECT src_rect, dst_rect;

    if (!rgndata) return;

    rgn_rect = (RECT *)rgndata->Buffer;
    rgn_rect_end = rgn_rect + rgndata->rdh.nCount;

    SetRect(&src_rect, 0, 0, src_buffer->width, src_buffer->height);
    SetRect(&dst_rect, 0, 0, dst_buffer->width, dst_buffer->height);

    for (;rgn_rect < rgn_rect_end; rgn_rect++)
    {
        unsigned int *src, *dst;
        int y, width, height;
        RECT rc;

        if (!intersect_rect(&rc, rgn_rect, &src_rect)) continue;
        if (!intersect_rect(&rc, &rc, &dst_rect)) continue;

        TRACE("rect %s => %s\n", wine_dbgstr_rect(rgn_rect),
              wine_dbgstr_rect(&rc));

        src = (unsigned int *)((unsigned char *)src_buffer->map_data +
              rc.top * src_buffer->stride + rc.left * bpp);
        dst = (unsigned int *)((unsigned char *)dst_buffer->map_data +
              rc.top * dst_buffer->stride + rc.left * bpp);
        width = rc.right - rc.left;
        height = rc.bottom - rc.top;

        /* Fast path for full width rectangles. */
        if (width == dst_buffer->width && width == src_buffer->width &&
            src_buffer->stride == dst_buffer->stride)
        {
            memcpy(dst, src, height * src_buffer->stride);
            continue;
        }

        for (y = 0; y < height; y++)
        {
            memcpy(dst, src, width * 4);
            src = (unsigned int*)((unsigned char*)src + src_buffer->stride);
            dst = (unsigned int*)((unsigned char*)dst + dst_buffer->stride);
        }
    }
}
