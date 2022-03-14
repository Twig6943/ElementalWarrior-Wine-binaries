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

#include <unistd.h>

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
