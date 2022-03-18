/*
 * Wayland dmabuf buffers
 *
 * Copyright 2022 Alexandros Frantzis for Collabora Ltd
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
#include "wine/debug.h"

#include <drm_fourcc.h>
#include <stdlib.h>

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

/***********************************************************************
 *           wayland_dmabuf_init
 */
void wayland_dmabuf_init(struct wayland_dmabuf *dmabuf,
                         struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf_v1)
{
    dmabuf->zwp_linux_dmabuf_v1 = zwp_linux_dmabuf_v1;
}

/***********************************************************************
 *           wayland_dmabuf_deinit
 */
void wayland_dmabuf_deinit(struct wayland_dmabuf *dmabuf)
{
    if (dmabuf->zwp_linux_dmabuf_v1)
        zwp_linux_dmabuf_v1_destroy(dmabuf->zwp_linux_dmabuf_v1);
}

/**********************************************************************
 *          wayland_dmabuf_buffer_from_native
 *
 * Creates a wayland dmabuf buffer from the specified native buffer.
 */
struct wayland_dmabuf_buffer *wayland_dmabuf_buffer_create_from_native(struct wayland *wayland,
                                                                       struct wayland_native_buffer *native)
{
    struct wayland_dmabuf_buffer *dmabuf_buffer;
    struct zwp_linux_buffer_params_v1 *params;
    int i;

    dmabuf_buffer = calloc(1, sizeof(*dmabuf_buffer));
    if (!dmabuf_buffer)
        goto err;

    params = zwp_linux_dmabuf_v1_create_params(wayland->dmabuf.zwp_linux_dmabuf_v1);
    for (i = 0; i < native->plane_count; i++)
    {
        zwp_linux_buffer_params_v1_add(params,
                                       native->fds[i],
                                       i,
                                       native->offsets[i],
                                       native->strides[i],
                                       native->modifier >> 32,
                                       native->modifier & 0xffffffff);
    }

    dmabuf_buffer->wl_buffer =
        zwp_linux_buffer_params_v1_create_immed(params,
                                                native->width,
                                                native->height,
                                                native->format,
                                                0);

    zwp_linux_buffer_params_v1_destroy(params);

    return dmabuf_buffer;

err:
    if (dmabuf_buffer)
        wayland_dmabuf_buffer_destroy(dmabuf_buffer);
    return NULL;
}

/**********************************************************************
 *          wayland_dmabuf_buffer_destroy
 *
 * Destroys a dmabuf buffer.
 */
void wayland_dmabuf_buffer_destroy(struct wayland_dmabuf_buffer *dmabuf_buffer)
{
    TRACE("%p\n", dmabuf_buffer);

    if (dmabuf_buffer->wl_buffer)
        wl_buffer_destroy(dmabuf_buffer->wl_buffer);

    free(dmabuf_buffer);
}

/**********************************************************************
 *          wayland_dmabuf_buffer_steal_wl_buffer_and_destroy
 *
 * Steal the wl_buffer from a dmabuf buffer and destroy the dmabuf buffer.
 */
struct wl_buffer *wayland_dmabuf_buffer_steal_wl_buffer_and_destroy(struct wayland_dmabuf_buffer *dmabuf_buffer)
{
    struct wl_buffer *wl_buffer;

    wl_buffer = dmabuf_buffer->wl_buffer;
    dmabuf_buffer->wl_buffer = NULL;

    wayland_dmabuf_buffer_destroy(dmabuf_buffer);

    return wl_buffer;
}
