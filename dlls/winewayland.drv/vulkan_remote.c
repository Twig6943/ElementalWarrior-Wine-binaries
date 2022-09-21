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

#define VK_NO_PROTOTYPES
#define WINE_VK_HOST

#include "wine/vulkan.h"
#include "wine/vulkan_driver.h"

#include "vulkan_remote.h"

#include <stdlib.h>

WINE_DEFAULT_DEBUG_CHANNEL(vulkan);

struct vk_funcs
{
    PFN_vkCreateImage p_vkCreateImage;
    PFN_vkDestroyImage p_vkDestroyImage;
    PFN_vkAllocateMemory p_vkAllocateMemory;
    PFN_vkFreeMemory p_vkFreeMemory;
    PFN_vkBindImageMemory p_vkBindImageMemory;
    PFN_vkGetImageMemoryRequirements p_vkGetImageMemoryRequirements;
    PFN_vkGetPhysicalDeviceMemoryProperties p_vkGetPhysicalDeviceMemoryProperties;
};

struct wayland_remote_vk_image
{
    VkImage native_vk_image;
    VkDeviceMemory native_vk_image_memory;
    VkFormat format;
    uint32_t width, height;
};

struct wayland_remote_vk_swapchain
{
    struct vk_funcs vk_funcs;
    uint32_t count_images;
    struct wayland_remote_vk_image *images;
};

static int get_image_create_flags(VkSwapchainCreateInfoKHR *chain_create_info)
{
    uint32_t flags = 0;

    if (chain_create_info->flags & VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR)
        flags |= VK_IMAGE_CREATE_PROTECTED_BIT;

    if (chain_create_info->flags & VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR)
        flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

    if (chain_create_info->flags & VK_SWAPCHAIN_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT_KHR)
        flags |= VK_IMAGE_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT;

    return flags;
}

static VkImage create_vulkan_image(VkDevice device, struct vk_funcs *vk_funcs,
                                   VkSwapchainCreateInfoKHR *chain_create_info)
{
    VkExternalMemoryImageCreateInfo external_memory_create_info = {0};
    VkImageCreateInfo image_create_info = {0};
    VkResult res;
    VkImage image;

    external_memory_create_info.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    external_memory_create_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.pNext = &external_memory_create_info;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.format = chain_create_info->imageFormat;
    image_create_info.extent.width = chain_create_info->imageExtent.width;
    image_create_info.extent.height = chain_create_info->imageExtent.height;
    image_create_info.extent.depth = 1;
    image_create_info.arrayLayers = chain_create_info->imageArrayLayers;
    image_create_info.sharingMode = chain_create_info->imageSharingMode;
    image_create_info.usage = chain_create_info->imageUsage;
    image_create_info.mipLevels = 1;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    /* We'll create dma-buf buffers for these images, and so we'll need to know
     * the layout of them on memory. When VK_EXT_image_drm_format_modifier is
     * not supported, we can't use TILING_DRM_FORMAT_MODIFIER_EXT, so that
     * leaves us with TILING_LINEAR or TILING_OPTIMAL available. If we choose
     * TILING_OPTIMAL, we are not able to query the modifier chosen by the
     * driver and the number of planes (because we don't have the extension to
     * do so). So it'd be impossible to create dma-buf buffers. This leaves us
     * with TILING_LINEAR, and that makes drivers decisions predictable and we
     * can assume that they'll pick DRM_FORMAT_MOD_LINEAR and there'll be a
     * single plane. This might fail for drivers that do not support modifiers
     * at all, but we can't do better than that. */
    image_create_info.tiling = VK_IMAGE_TILING_LINEAR;
    image_create_info.flags = get_image_create_flags(chain_create_info);

    res = vk_funcs->p_vkCreateImage(device, &image_create_info, NULL, &image);
    if (res != VK_SUCCESS)
    {
        ERR("vkCreateImage failed, res=%d\n", res);
        goto err;
    }

    return image;

err:
    ERR("Failed to create Vulkan image\n");
    return VK_NULL_HANDLE;
}

static int get_memory_property_flags(VkSwapchainCreateInfoKHR *chain_create_info)
{
    uint32_t flags = 0;

    if (chain_create_info->flags & VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR)
        flags |= VK_MEMORY_PROPERTY_PROTECTED_BIT;

    return flags;
}

static VkDeviceMemory create_vulkan_image_memory(VkInstance instance, VkPhysicalDevice physical_device,
                                                 VkDevice device, struct vk_funcs *vk_funcs,
                                                 VkSwapchainCreateInfoKHR *chain_create_info,
                                                 VkImage image)
{
    int32_t mem_type_index = -1;
    uint32_t flags;
    unsigned int i;
    VkMemoryRequirements mem_reqs;
    VkPhysicalDeviceMemoryProperties mem_props;
    VkExportMemoryAllocateInfo export_alloc_info = {0};
    VkMemoryAllocateInfo alloc_info = {0};
    VkResult res;
    VkDeviceMemory image_mem;

    export_alloc_info.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    export_alloc_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext = &export_alloc_info;

    vk_funcs->p_vkGetImageMemoryRequirements(device, image, &mem_reqs);
    vk_funcs->p_vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

    flags = get_memory_property_flags(chain_create_info);
    for (i = 0; i < mem_props.memoryTypeCount; i++)
        if ((mem_reqs.memoryTypeBits & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & flags) == flags)
                mem_type_index = i;

    if (mem_type_index == -1)
    {
        ERR("Failed to find memoryTypeIndex\n");
        goto err;
    }

    alloc_info.memoryTypeIndex = mem_type_index;
    alloc_info.allocationSize = mem_reqs.size;

    res = vk_funcs->p_vkAllocateMemory(device, &alloc_info, NULL, &image_mem);
    if (res != VK_SUCCESS)
    {
        ERR("pfn_vkAllocateMemory failed, res=%d\n", res);
        goto err;
    }

    return image_mem;

err:
    ERR("Failed to create Vulkan image memory\n");
    return VK_NULL_HANDLE;
}

static void wayland_remote_vk_image_deinit(VkDevice device, struct vk_funcs *vk_funcs,
                                           struct wayland_remote_vk_image *image)
{
    vk_funcs->p_vkDestroyImage(device, image->native_vk_image, NULL);
    vk_funcs->p_vkFreeMemory(device, image->native_vk_image_memory, NULL);
}


static int wayland_remote_vk_image_init(VkInstance instance, VkPhysicalDevice physical_device,
                                        VkDevice device, struct vk_funcs *vk_funcs,
                                        VkSwapchainCreateInfoKHR *create_info,
                                        struct wayland_remote_vk_image *image)
{
    VkResult res;

    image->native_vk_image = VK_NULL_HANDLE;
    image->native_vk_image_memory = VK_NULL_HANDLE;
    image->format = create_info->imageFormat;
    image->width = create_info->imageExtent.width;
    image->height = create_info->imageExtent.height;

    image->native_vk_image = create_vulkan_image(device, vk_funcs, create_info);
    if (image->native_vk_image == VK_NULL_HANDLE)
        goto err;

    image->native_vk_image_memory =
        create_vulkan_image_memory(instance, physical_device, device,
                                   vk_funcs, create_info, image->native_vk_image);
    if (image->native_vk_image_memory == VK_NULL_HANDLE)
        goto err;

    res = vk_funcs->p_vkBindImageMemory(device, image->native_vk_image,
                                        image->native_vk_image_memory, 0);
    if (res != VK_SUCCESS)
    {
        ERR("pfn_vkBindImageMemory failed, res=%d\n", res);
        goto err;
    }

    return 0;

err:
    ERR("Failed to create remote swapchain image\n");
    wayland_remote_vk_image_deinit(device, vk_funcs, image);
    return -1;
}

void wayland_remote_vk_swapchain_destroy(struct wayland_remote_vk_swapchain *swapchain,
                                         VkDevice device)
{
    if (swapchain->images)
    {
        unsigned int i;
        for (i = 0; i < swapchain->count_images; i++)
        {
            wayland_remote_vk_image_deinit(device, &swapchain->vk_funcs,
                                           &swapchain->images[i]);
        }
        free(swapchain->images);
    }
    free(swapchain);
}

struct wayland_remote_vk_swapchain *wayland_remote_vk_swapchain_create(HWND hwnd, VkInstance instance,
                                                                       VkPhysicalDevice physical_device,
                                                                       VkDevice device,
                                                                       const struct vulkan_funcs *vulkan_funcs,
                                                                       VkSwapchainCreateInfoKHR *create_info)
{
    static const uint32_t min_number_images = 4;
    struct wayland_remote_vk_swapchain *swapchain;
    unsigned int i;
    int res = 0;

    swapchain = calloc(1, sizeof(*swapchain));
    if (!swapchain)
    {
        ERR("Failed to allocate memory\n");
        goto err;
    }

#define LOAD_DEVICE_FUNCPTR(f) \
    if (!(swapchain->vk_funcs.p_##f = vulkan_funcs->p_vkGetDeviceProcAddr(device, #f))) \
        goto err

#define LOAD_INSTANCE_FUNCPTR(f) \
    if (!(swapchain->vk_funcs.p_##f = vulkan_funcs->p_vkGetInstanceProcAddr(instance, #f))) \
        goto err

    LOAD_DEVICE_FUNCPTR(vkCreateImage);
    LOAD_DEVICE_FUNCPTR(vkDestroyImage);
    LOAD_DEVICE_FUNCPTR(vkAllocateMemory);
    LOAD_DEVICE_FUNCPTR(vkFreeMemory);
    LOAD_DEVICE_FUNCPTR(vkBindImageMemory);
    LOAD_DEVICE_FUNCPTR(vkGetImageMemoryRequirements);
    LOAD_INSTANCE_FUNCPTR(vkGetPhysicalDeviceMemoryProperties);

#undef LOAD_DEVICE_FUNCPTR
#undef LOAD_INSTANCE_FUNCPTR

    swapchain->count_images = max(create_info->minImageCount, min_number_images);
    swapchain->images = calloc(swapchain->count_images, sizeof(*swapchain->images));
    if (!swapchain->images)
    {
        ERR("Failed to allocate memory\n");
        goto err;
    }

    for (i = 0; i < swapchain->count_images; i++)
        res |= wayland_remote_vk_image_init(instance, physical_device, device,
                                            &swapchain->vk_funcs, create_info,
                                            &swapchain->images[i]);
    if (res < 0)
        goto err;


    return swapchain;

err:
    ERR("Failed to create remote swapchain\n");
    if (swapchain)
        wayland_remote_vk_swapchain_destroy(swapchain, device);
    return NULL;
}

VkResult wayland_remote_vk_swapchain_get_images(struct wayland_remote_vk_swapchain *swapchain,
                                                uint32_t *count, VkImage *images)
{
    unsigned int i;
    VkResult res = VK_SUCCESS;

    if (!images)
    {
        *count = swapchain->count_images;
        return VK_SUCCESS;
    }

    if (*count < swapchain->count_images)
        res = VK_INCOMPLETE;

    /* The client want us to fill images, but for some reason the size of the
     * array is larger than the number of formats that we support. So we correct
     * that size. */
    if (*count > swapchain->count_images)
        *count = swapchain->count_images;

    for (i = 0; i < (*count); i++)
        images[i] = swapchain->images[i].native_vk_image;

    return res;
}
