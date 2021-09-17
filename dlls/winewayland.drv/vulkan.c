/* WAYLANDDRV Vulkan implementation
 *
 * Copyright 2017 Roderick Colenbrander
 * Copyright 2021 Alexandros Frantzis
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

#include <dlfcn.h>
#include <stdlib.h>

WINE_DEFAULT_DEBUG_CHANNEL(vulkan);

#ifdef SONAME_LIBVULKAN

#define VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR 1000006000

typedef struct VkWaylandSurfaceCreateInfoKHR
{
    VkStructureType sType;
    const void *pNext;
    VkWaylandSurfaceCreateFlagsKHR flags;
    struct wl_display *display;
    struct wl_surface *surface;
} VkWaylandSurfaceCreateInfoKHR;

static VkResult (*pvkCreateInstance)(const VkInstanceCreateInfo *, const VkAllocationCallbacks *, VkInstance *);
static VkResult (*pvkCreateSwapchainKHR)(VkDevice, const VkSwapchainCreateInfoKHR *, const VkAllocationCallbacks *, VkSwapchainKHR *);
static VkResult (*pvkCreateWaylandSurfaceKHR)(VkInstance, const VkWaylandSurfaceCreateInfoKHR *, const VkAllocationCallbacks *, VkSurfaceKHR *);
static void (*pvkDestroyInstance)(VkInstance, const VkAllocationCallbacks *);
static void (*pvkDestroySurfaceKHR)(VkInstance, VkSurfaceKHR, const VkAllocationCallbacks *);
static void (*pvkDestroySwapchainKHR)(VkDevice, VkSwapchainKHR, const VkAllocationCallbacks *);
static VkResult (*pvkQueuePresentKHR)(VkQueue, const VkPresentInfoKHR *);

static void *vulkan_handle;

static struct wayland_mutex wine_vk_object_mutex =
{
    PTHREAD_MUTEX_INITIALIZER, 0, 0, __FILE__ ": wine_vk_object_mutex"
};

static struct wl_list wine_vk_surface_list = { &wine_vk_surface_list, &wine_vk_surface_list };
static struct wl_list wine_vk_swapchain_list = { &wine_vk_swapchain_list, &wine_vk_swapchain_list };

struct wine_vk_surface
{
    struct wl_list link;
    HWND hwnd;
    struct wayland_surface *wayland_surface;
    VkSurfaceKHR native_vk_surface;
};

struct wine_vk_swapchain
{
    struct wl_list link;
    HWND hwnd;
    struct wayland_surface *wayland_surface;
    VkSwapchainKHR native_vk_swapchain;
};

static inline void wine_vk_list_add(struct wl_list *list, struct wl_list *link)
{
    wayland_mutex_lock(&wine_vk_object_mutex);
    wl_list_insert(list, link);
    wayland_mutex_unlock(&wine_vk_object_mutex);
}

static inline void wine_vk_list_remove(struct wl_list *link)
{
    wayland_mutex_lock(&wine_vk_object_mutex);
    wl_list_remove(link);
    wayland_mutex_unlock(&wine_vk_object_mutex);
}

static void wine_vk_surface_destroy(struct wine_vk_surface *wine_vk_surface)
{
    wine_vk_list_remove(&wine_vk_surface->link);

    if (wine_vk_surface->wayland_surface)
        wayland_surface_unref_glvk(wine_vk_surface->wayland_surface);

    free(wine_vk_surface);
}

static struct wine_vk_surface *wine_vk_surface_from_handle(VkSurfaceKHR handle)
{
    struct wine_vk_surface *surf;

    wayland_mutex_lock(&wine_vk_object_mutex);

    wl_list_for_each(surf, &wine_vk_surface_list, link)
        if (surf->native_vk_surface == handle) goto out;

    surf = NULL;

out:
    wayland_mutex_unlock(&wine_vk_object_mutex);
    return surf;
}

static void wine_vk_swapchain_destroy(struct wine_vk_swapchain *wine_vk_swapchain)
{
    wine_vk_list_remove(&wine_vk_swapchain->link);

    if (wine_vk_swapchain->wayland_surface)
        wayland_surface_unref_glvk(wine_vk_swapchain->wayland_surface);

    free(wine_vk_swapchain);
}

static struct wine_vk_swapchain *wine_vk_swapchain_from_handle(VkSurfaceKHR handle)
{
    struct wine_vk_swapchain *swap;

    wayland_mutex_lock(&wine_vk_object_mutex);

    wl_list_for_each(swap, &wine_vk_swapchain_list, link)
        if (swap->native_vk_swapchain == handle) goto out;

    swap = NULL;

out:
    wayland_mutex_unlock(&wine_vk_object_mutex);
    return swap;
}

/* Helper function for converting between win32 and Wayland compatible VkInstanceCreateInfo.
 * Caller is responsible for allocation and cleanup of 'dst'.
 */
static VkResult wine_vk_instance_convert_create_info(const VkInstanceCreateInfo *src,
                                                     VkInstanceCreateInfo *dst)
{
    unsigned int i;
    const char **enabled_extensions = NULL;

    dst->sType = src->sType;
    dst->flags = src->flags;
    dst->pApplicationInfo = src->pApplicationInfo;
    dst->pNext = src->pNext;
    dst->enabledLayerCount = 0;
    dst->ppEnabledLayerNames = NULL;
    dst->enabledExtensionCount = 0;
    dst->ppEnabledExtensionNames = NULL;

    if (src->enabledExtensionCount > 0)
    {
        enabled_extensions = calloc(src->enabledExtensionCount, sizeof(*src->ppEnabledExtensionNames));
        if (!enabled_extensions)
        {
            ERR("Failed to allocate memory for enabled extensions\n");
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        for (i = 0; i < src->enabledExtensionCount; i++)
        {
            /* Substitute extension with Wayland ones else copy. Long-term, when we
             * support more extensions, we should store these in a list.
             */
            if (!strcmp(src->ppEnabledExtensionNames[i], "VK_KHR_win32_surface"))
                enabled_extensions[i] = "VK_KHR_wayland_surface";
            else
                enabled_extensions[i] = src->ppEnabledExtensionNames[i];
        }
        dst->ppEnabledExtensionNames = enabled_extensions;
        dst->enabledExtensionCount = src->enabledExtensionCount;
    }

    return VK_SUCCESS;
}

#define RETURN_VK_ERROR_SURFACE_LOST_KHR { \
    TRACE("VK_ERROR_SURFACE_LOST_KHR\n"); \
    return VK_ERROR_SURFACE_LOST_KHR; \
}

static VkResult wayland_vkCreateInstance(const VkInstanceCreateInfo *create_info,
                                         const VkAllocationCallbacks *allocator,
                                         VkInstance *instance)
{
    VkInstanceCreateInfo create_info_host;
    VkResult res;
    TRACE("create_info %p, allocator %p, instance %p\n", create_info, allocator, instance);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    /* Perform a second pass on converting VkInstanceCreateInfo. Winevulkan
     * performed a first pass in which it handles everything except for WSI
     * functionality such as VK_KHR_win32_surface. Handle this now.
     */
    res = wine_vk_instance_convert_create_info(create_info, &create_info_host);
    if (res != VK_SUCCESS)
    {
        ERR("Failed to convert instance create info, res=%d\n", res);
        return res;
    }

    res = pvkCreateInstance(&create_info_host, NULL /* allocator */, instance);

    free((void *)create_info_host.ppEnabledExtensionNames);
    return res;
}

static VkResult wayland_vkCreateSwapchainKHR(VkDevice device,
                                             const VkSwapchainCreateInfoKHR *create_info,
                                             const VkAllocationCallbacks *allocator,
                                             VkSwapchainKHR *swapchain)
{
    VkResult res;
    struct wine_vk_surface *wine_vk_surface;
    struct wine_vk_swapchain *wine_vk_swapchain;
    VkSwapchainCreateInfoKHR info = *create_info;

    TRACE("%p %p %p %p\n", device, create_info, allocator, swapchain);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    /* Wayland can't deal with 0x0 swapchains, use the minimum 1x1. */
    if (info.imageExtent.width == 0)
        info.imageExtent.width = 1;
    if (info.imageExtent.height == 0)
        info.imageExtent.height = 1;

    wine_vk_surface = wine_vk_surface_from_handle(info.surface);
    if (!wine_vk_surface)
        RETURN_VK_ERROR_SURFACE_LOST_KHR;

    wine_vk_swapchain = calloc(1, sizeof(*wine_vk_swapchain));
    if (!wine_vk_swapchain)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    wl_list_init(&wine_vk_swapchain->link);

    res = pvkCreateSwapchainKHR(device, &info, NULL /* allocator */, swapchain);
    if (res != VK_SUCCESS)
        goto err;

    wine_vk_swapchain->hwnd = wine_vk_surface->hwnd;
    if (wine_vk_surface->wayland_surface)
    {
        wayland_surface_create_or_ref_glvk(wine_vk_surface->wayland_surface);
        wine_vk_swapchain->wayland_surface = wine_vk_surface->wayland_surface;
    }
    wine_vk_swapchain->native_vk_swapchain = *swapchain;

    wine_vk_list_add(&wine_vk_swapchain_list, &wine_vk_swapchain->link);

    return res;

err:
    wine_vk_swapchain_destroy(wine_vk_swapchain);
    return res;
}

static VkResult wayland_vkCreateWin32SurfaceKHR(VkInstance instance,
                                                const VkWin32SurfaceCreateInfoKHR *create_info,
                                                const VkAllocationCallbacks *allocator,
                                                VkSurfaceKHR *vk_surface)
{
    VkResult res;
    VkWaylandSurfaceCreateInfoKHR create_info_host;
    struct wine_vk_surface *wine_vk_surface;
    struct wayland_surface *wayland_surface;
    BOOL ref_vk;

    TRACE("%p %p %p %p\n", instance, create_info, allocator, vk_surface);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    wine_vk_surface = calloc(1, sizeof(*wine_vk_surface));
    if (!wine_vk_surface)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    wl_list_init(&wine_vk_surface->link);

    wayland_surface = wayland_surface_for_hwnd_lock(create_info->hwnd);
    if (!wayland_surface)
    {
        ERR("Failed to find wayland surface for hwnd=%p\n", create_info->hwnd);
        /* VK_KHR_win32_surface only allows out of host and device memory as errors. */
        res = VK_ERROR_OUT_OF_HOST_MEMORY;
        goto err;
    }

    ref_vk = wayland_surface_create_or_ref_glvk(wayland_surface);
    wayland_surface_for_hwnd_unlock(wayland_surface);
    if (!ref_vk)
    {
        ERR("Failed to create or ref vulkan surface for hwnd=%p\n", create_info->hwnd);
        /* VK_KHR_win32_surface only allows out of host and device memory as errors. */
        res = VK_ERROR_OUT_OF_HOST_MEMORY;
        goto err;
    }

    wine_vk_surface->wayland_surface = wayland_surface;

    create_info_host.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    create_info_host.pNext = NULL;
    create_info_host.flags = 0; /* reserved */
    create_info_host.display = process_wl_display;
    create_info_host.surface = wayland_surface->glvk->wl_surface;

    res = pvkCreateWaylandSurfaceKHR(instance, &create_info_host, NULL /* allocator */, vk_surface);
    if (res != VK_SUCCESS)
    {
        ERR("Failed to create vulkan wayland surface, res=%d\n", res);
        goto err;
    }

    wine_vk_surface->hwnd = create_info->hwnd;
    wine_vk_surface->native_vk_surface = *vk_surface;

    wine_vk_list_add(&wine_vk_surface_list, &wine_vk_surface->link);

    TRACE("Created surface=0x%s\n", wine_dbgstr_longlong(*vk_surface));
    return VK_SUCCESS;

err:
    wine_vk_surface_destroy(wine_vk_surface);
    return res;
}

static void wayland_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *allocator)
{
    TRACE("%p %p\n", instance, allocator);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    pvkDestroyInstance(instance, NULL /* allocator */);
}

static void wayland_vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface,
                                        const VkAllocationCallbacks *allocator)
{
    struct wine_vk_surface *wine_vk_surface = wine_vk_surface_from_handle(surface);

    TRACE("%p 0x%s %p\n", instance, wine_dbgstr_longlong(surface), allocator);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if (wine_vk_surface)
    {
        pvkDestroySurfaceKHR(instance, wine_vk_surface->native_vk_surface,
                             NULL /* allocator */);
        wine_vk_surface_destroy(wine_vk_surface);
    }
}

static void wayland_vkDestroySwapchainKHR(VkDevice device,
                                          VkSwapchainKHR swapchain,
                                          const VkAllocationCallbacks *allocator)
{
    struct wine_vk_swapchain *wine_vk_swapchain = wine_vk_swapchain_from_handle(swapchain);

    TRACE("%p, 0x%s %p\n", device, wine_dbgstr_longlong(swapchain), allocator);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if (wine_vk_swapchain)
    {
        pvkDestroySwapchainKHR(device, wine_vk_swapchain->native_vk_swapchain,
                               NULL /* allocator */);
        wine_vk_swapchain_destroy(wine_vk_swapchain);
    }
}

static VkResult validate_present_info(const VkPresentInfoKHR *present_info)
{
    uint32_t i;
    VkResult res = VK_SUCCESS;

    for (i = 0; i < present_info->swapchainCount; ++i)
    {
        const VkSwapchainKHR vk_swapchain = present_info->pSwapchains[i];
        struct wine_vk_swapchain *wine_vk_swapchain =
            wine_vk_swapchain_from_handle(vk_swapchain);
        BOOL drawing_allowed =
            (wine_vk_swapchain && wine_vk_swapchain->wayland_surface) ?
            wine_vk_swapchain->wayland_surface->drawing_allowed : TRUE;

        TRACE("swapchain[%d] vk=0x%s wine=%p wayland_surface=%p "
               "drawing_allowed=%d\n",
               i, wine_dbgstr_longlong(vk_swapchain), wine_vk_swapchain,
               wine_vk_swapchain ? wine_vk_swapchain->wayland_surface : NULL,
               drawing_allowed);

        if (!wine_vk_swapchain)
            res = VK_ERROR_SURFACE_LOST_KHR;
        else if (!drawing_allowed)
            if (res == VK_SUCCESS) res = VK_ERROR_OUT_OF_DATE_KHR;

        /* Since Vulkan content is presented on a Wayland subsurface, we need
         * to ensure the parent Wayland surface is mapped for the Vulkan
         * content to be visible. */
        if (wine_vk_swapchain->wayland_surface && drawing_allowed)
            wayland_surface_ensure_mapped(wine_vk_swapchain->wayland_surface);
    }

    /* In case of error in any swapchain, we are not going to present at all,
     * so mark all swapchains as failures. */
    if (res != VK_SUCCESS && present_info->pResults)
    {
        for (i = 0; i < present_info->swapchainCount; ++i)
            present_info->pResults[i] = res;
    }

    return res;
}

static void lock_swapchain_wayland_surfaces(const VkPresentInfoKHR *present_info,
                                            BOOL lock)
{
    uint32_t i;

    for (i = 0; i < present_info->swapchainCount; ++i)
    {
        const VkSwapchainKHR vk_swapchain = present_info->pSwapchains[i];
        struct wine_vk_swapchain *wine_vk_swapchain =
            wine_vk_swapchain_from_handle(vk_swapchain);

        if (wine_vk_swapchain && wine_vk_swapchain->wayland_surface)
        {
            if (lock)
                wayland_mutex_lock(&wine_vk_swapchain->wayland_surface->mutex);
            else
                wayland_mutex_unlock(&wine_vk_swapchain->wayland_surface->mutex);
        }
    }
}

static VkResult wayland_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *present_info)
{
    VkResult res;

    TRACE("%p, %p\n", queue, present_info);

    /* Lock the surfaces to ensure we don't present while reconfiguration is
     * taking place, so we don't inadvertently commit an in-progress,
     * incomplete configuration state. */
    lock_swapchain_wayland_surfaces(present_info, TRUE);

    if ((res = validate_present_info(present_info)) == VK_SUCCESS)
        res = pvkQueuePresentKHR(queue, present_info);

    lock_swapchain_wayland_surfaces(present_info, FALSE);

    return res;
}

static void wine_vk_init(void)
{
    if (!(vulkan_handle = dlopen(SONAME_LIBVULKAN, RTLD_NOW)))
    {
        ERR("Failed to load %s.\n", SONAME_LIBVULKAN);
        return;
    }

#define LOAD_FUNCPTR(f) if (!(p##f = dlsym(vulkan_handle, #f))) goto fail
    LOAD_FUNCPTR(vkCreateInstance);
    LOAD_FUNCPTR(vkCreateSwapchainKHR);
    LOAD_FUNCPTR(vkCreateWaylandSurfaceKHR);
    LOAD_FUNCPTR(vkDestroyInstance);
    LOAD_FUNCPTR(vkDestroySurfaceKHR);
    LOAD_FUNCPTR(vkDestroySwapchainKHR);
    LOAD_FUNCPTR(vkQueuePresentKHR);
#undef LOAD_FUNCPTR

    return;

fail:
    dlclose(vulkan_handle);
    vulkan_handle = NULL;
}

static const struct vulkan_funcs vulkan_funcs =
{
    .p_vkCreateInstance = wayland_vkCreateInstance,
    .p_vkCreateSwapchainKHR = wayland_vkCreateSwapchainKHR,
    .p_vkCreateWin32SurfaceKHR = wayland_vkCreateWin32SurfaceKHR,
    .p_vkDestroyInstance = wayland_vkDestroyInstance,
    .p_vkDestroySurfaceKHR = wayland_vkDestroySurfaceKHR,
    .p_vkDestroySwapchainKHR = wayland_vkDestroySwapchainKHR,
    .p_vkQueuePresentKHR = wayland_vkQueuePresentKHR,
};

/**********************************************************************
 *           WAYLAND_wine_get_vulkan_driver
 */
const struct vulkan_funcs *WAYLAND_wine_get_vulkan_driver(UINT version)
{
    static pthread_once_t init_once = PTHREAD_ONCE_INIT;

    if (version != WINE_VULKAN_DRIVER_VERSION)
    {
        ERR("version mismatch, vulkan wants %u but driver has %u\n", version, WINE_VULKAN_DRIVER_VERSION);
        return NULL;
    }

    pthread_once(&init_once, wine_vk_init);
    if (vulkan_handle)
        return &vulkan_funcs;

    return NULL;
}

#else /* No vulkan */

const struct vulkan_funcs *WAYLAND_wine_get_vulkan_driver(UINT version)
{
    ERR("Wine was built without Vulkan support.\n");
    return NULL;
}

#endif /* SONAME_LIBVULKAN */
