/*
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

#include <assert.h>
#include <unistd.h>
#include <xf86drm.h>

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

/**********************************************************************
 *          wayland_native_buffer_init_shm
 *
 * Deinitializes a native buffer, releasing any associated resources.
 */
BOOL wayland_native_buffer_init_shm(struct wayland_native_buffer *native,
                                    int width, int height,
                                    enum wl_shm_format format)
{
    int stride;
    off_t size;
    int fd;

    assert(format == WL_SHM_FORMAT_ARGB8888 || format == WL_SHM_FORMAT_XRGB8888);

    stride = width * 4;
    size = stride * height;

    fd = wayland_shmfd_create("wayland-shm", size);
    if (fd < 0) return FALSE;

    native->plane_count = 1;
    native->fds[0] = fd;
    native->strides[0] = stride;
    native->offsets[0] = 0;
    native->width = width;
    native->height = height;
    native->format = format;

    return TRUE;
}

/**********************************************************************
 *          wayland_native_buffer_init_gbm
 *
 * Initializes a native buffer from a gbm_bo.
 */
BOOL wayland_native_buffer_init_gbm(struct wayland_native_buffer *native,
                                    struct gbm_bo *bo)
{
    int i;

    native->plane_count = gbm_bo_get_plane_count(bo);
    native->width = gbm_bo_get_width(bo);
    native->height = gbm_bo_get_height(bo);
    native->format = gbm_bo_get_format(bo);
    native->modifier = gbm_bo_get_modifier(bo);
    for (i = 0; i < ARRAY_SIZE(native->fds); i++)
        native->fds[i] = -1;

    for (i = 0; i < native->plane_count; i++)
    {
        int ret;
        union gbm_bo_handle handle;

        handle = gbm_bo_get_handle_for_plane(bo, i);
        if (handle.s32 == -1)
        {
            ERR("error: failed to get gbm_bo_handle\n");
            goto err;
        }

        ret = drmPrimeHandleToFD(gbm_device_get_fd(gbm_bo_get_device(bo)),
                                 handle.u32, 0, &native->fds[i]);
        if (ret < 0 || native->fds[i] < 0)
        {
            ERR("error: failed to get dmabuf_fd\n");
            goto err;
        }
        native->strides[i] = gbm_bo_get_stride_for_plane(bo, i);
        native->offsets[i] = gbm_bo_get_offset(bo, i);
    }

    return TRUE;

err:
    wayland_native_buffer_deinit(native);
    return FALSE;
}

/**********************************************************************
 *          wayland_native_buffer_deinit
 *
 * Deinitializes a native buffer, releasing any associated resources.
 */
void wayland_native_buffer_deinit(struct wayland_native_buffer *native)
{
    int i;

    for (i = 0; i < native->plane_count; i++)
        if (native->fds[i] >= 0) close(native->fds[i]);

    native->plane_count = 0;
}
