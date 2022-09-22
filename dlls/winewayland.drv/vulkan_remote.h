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

#ifndef __WINE_WAYLANDDRV_VULKAN_REMOTE_H
#define __WINE_WAYLANDDRV_VULKAN_REMOTE_H

#include "config.h"

#include "waylanddrv.h"

#define VK_NO_PROTOTYPES
#define WINE_VK_HOST

#include "wine/vulkan.h"

struct wayland_remote_vk_swapchain;
struct vulkan_funcs;

struct wayland_remote_vk_swapchain *wayland_remote_vk_swapchain_create(HWND hwnd, VkInstance instance,
                                                                       VkPhysicalDevice physical_device,
                                                                       VkDevice device,
                                                                       const struct vulkan_funcs *vulkan_funcs,
                                                                       VkSwapchainCreateInfoKHR *create_info) DECLSPEC_HIDDEN;
void wayland_remote_vk_swapchain_destroy(struct wayland_remote_vk_swapchain *swapchain,
                                         VkDevice device) DECLSPEC_HIDDEN;
VkResult wayland_remote_vk_swapchain_get_images(struct wayland_remote_vk_swapchain *swapchain,
                                                uint32_t *count, VkImage *images) DECLSPEC_HIDDEN;
VkResult wayland_remote_vk_swapchain_acquire_next_image(struct wayland_remote_vk_swapchain *swapchain,
                                                        VkDevice device, uint64_t timeout_ns,
                                                        VkSemaphore semaphore, VkFence fence,
                                                        uint32_t *image_index) DECLSPEC_HIDDEN;
int wayland_remote_vk_swapchain_present(struct wayland_remote_vk_swapchain *swapchain,
                                        uint32_t image_index) DECLSPEC_HIDDEN;
VkResult wayland_remote_vk_filter_supported_formats(uint32_t *count_filtered_formats,
                                                    void *filtered_formats,
                                                    uint32_t count_formats_to_filter,
                                                    void *formats_to_filter,
                                                    size_t format_size,
                                                    size_t vk_surface_format_offset) DECLSPEC_HIDDEN;

#endif /* __WINE_WAYLANDDRV_VULKAN_REMOTE_H */
