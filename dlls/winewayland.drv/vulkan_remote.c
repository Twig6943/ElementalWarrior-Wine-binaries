/* WAYLANDDRV Vulkan remote implementation
 *
 * Copyright 2022 Leandro Ribeiro
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

#include <stdlib.h>

WINE_DEFAULT_DEBUG_CHANNEL(vulkan);

struct wayland_remote_vk_swapchain
{
};

void wayland_remote_vk_swapchain_destroy(struct wayland_remote_vk_swapchain *swapchain)
{
    free(swapchain);
}

struct wayland_remote_vk_swapchain *wayland_remote_vk_swapchain_create(HWND hwnd)
{
    struct wayland_remote_vk_swapchain *swapchain;

    swapchain = calloc(1, sizeof(*swapchain));
    if (!swapchain)
    {
        ERR("Failed to allocate memory\n");
        goto err;
    }

    return swapchain;

err:
    ERR("Failed to create remote swapchain\n");
    if (swapchain)
        wayland_remote_vk_swapchain_destroy(swapchain);
    return NULL;
}
