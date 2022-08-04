/*
 * Wayland GBM support
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

#include <assert.h>
#include <errno.h>
#include <drm_fourcc.h>
#include <fcntl.h>
#ifdef HAVE_LIBUDEV_H
#include <libudev.h>
#endif
#include <stdlib.h>
#include <unistd.h>
#include <xf86drm.h>

#include <sys/types.h>
#include <sys/stat.h>

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

struct gbm_device *process_gbm_device;
static pthread_once_t init_once;

static const char default_seat[] = "seat0";
static const char default_render_node[] = "/dev/dri/renderD128";
static const char default_primary_node[] = "/dev/dri/card0";
static const char primary_node_sysname[] = "card[0-9]*";
static const char render_node_sysname[] = "renderD[0-9]*";

#ifdef HAVE_UDEV

typedef BOOL (*filter_func)(struct udev_device *, const char *);

/* returns TRUE for every udev_dev whose devnode is not devnode_to_ignore */
static BOOL filter_has_different_devnode(struct udev_device *udev_dev,
                                         const char *devnode_to_ignore)
{
    const char *devnode;

    assert(udev_dev && devnode_to_ignore);

    /* If we can't get a devnode from the device, we prefer to filter it out */
    devnode = udev_device_get_devnode(udev_dev);
    if (!devnode)
        return FALSE;

    /* devnode is equal to devnode_to_ignore */
    if (strcmp(devnode, devnode_to_ignore) == 0)
        return FALSE;

    return TRUE;
}

/* returns TRUE for every udev_dev that is not the primary system GPU */
static BOOL filter_is_not_primary_system_gpu(struct udev_device *udev_dev,
                                             const char *unused_arg)
{
    struct udev_device *pci_device;
    const char *boot_vga;

    assert(udev_dev);

    /* If we can't get pci_device, we prefer to filter the device out */
    pci_device = udev_device_get_parent(udev_dev);
    if (!pci_device)
        return FALSE;

    /* It is the primary system GPU */
    boot_vga = udev_device_get_sysattr_value(pci_device, "boot_vga");
    if (boot_vga && strcmp(boot_vga, "1") == 0)
        return FALSE;

    return TRUE;
}

/* returns TRUE for every udev_dev whose ID_PATH_TAG is id_path_tag */
static BOOL filter_has_same_id_path_tag(struct udev_device *udev_dev,
                                        const char *id_path_tag)
{
    const char *dev_id_path_tag;

    assert(udev_dev && id_path_tag);

    /* If we can't get dev_id_path_tag, we prefer to filter the device out */
    dev_id_path_tag = udev_device_get_property_value(udev_dev, "ID_PATH_TAG");
    if (!dev_id_path_tag)
        return FALSE;

    /* ID_PATH_TAG is different from id_path_tag */
    if (strcmp(dev_id_path_tag, id_path_tag) != 0)
        return FALSE;

    return TRUE;
}

static BOOL is_primary_system_gpu_set(void)
{
    struct udev *udev;
    struct udev_enumerate *e = NULL;
    BOOL ret = FALSE;

    udev = udev_new();
    if (!udev) goto out;

    e = udev_enumerate_new(udev);
    if (!e) goto out;

    udev_enumerate_add_match_sysattr(e, "boot_vga", "1");

    /* if list is not empty we have a PCI device with boot_vga set to 1 (i.e. we
     * have a PCI device marked as the primary system GPU) */
    udev_enumerate_scan_devices(e);
    if (udev_enumerate_get_list_entry(e)) ret = TRUE;

out:
    if (e) udev_enumerate_unref(e);
    if (udev) udev_unref(udev);

    return ret;
}

static int wayland_gbm_get_drm_fd(const char *sysname, const char *desc,
                                  filter_func filter, const char *filter_arg)
{
    const char *seat;
    struct udev *udev = NULL;
    struct udev_enumerate *e = NULL;
    struct udev_list_entry *entry;
    int drm_fd = -1;

    seat = getenv("XDG_SEAT");
    if (!seat) seat = default_seat;

    udev = udev_new();
    if (!udev) goto out;

    e = udev_enumerate_new(udev);
    if (!e) goto out;
    udev_enumerate_add_match_subsystem(e, "drm");
    udev_enumerate_add_match_sysname(e, sysname);

    udev_enumerate_scan_devices(e);
    udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e))
    {
        const char *path, *device_seat, *devnode;
        struct udev_device *device;

        path = udev_list_entry_get_name(entry);
        device = udev_device_new_from_syspath(udev, path);
        if (!device) continue;

        device_seat = udev_device_get_property_value(device, "ID_SEAT");
        if (!device_seat) device_seat = default_seat;
        if (strcmp(device_seat, seat))
        {
            udev_device_unref(device);
            continue;
        }

        devnode = udev_device_get_devnode(device);
        if (!devnode)
        {
            udev_device_unref(device);
            continue;
        }

        /* If we have a filter, we may ignore certain devices */
        if (filter && filter(device, filter_arg) == FALSE)
        {
            udev_device_unref(device);
            continue;
        }

        drm_fd = open(devnode, O_RDWR);
        TRACE("Trying to open drm device (%s) %s => fd=%d\n", desc, devnode, drm_fd);

        udev_device_unref(device);
        if (drm_fd >= 0) break;
    }

out:
    if (e) udev_enumerate_unref(e);
    if (udev) udev_unref(udev);

    return drm_fd;
}

#else

typedef void (*filter_func)(void);

static void filter_has_different_devnode(void)
{
}

static void filter_is_not_primary_system_gpu(void)
{
}

static void filter_has_same_id_path_tag(void)
{
}

static BOOL is_primary_system_gpu_set(void)
{
    return FALSE;
}

static int wayland_gbm_get_drm_fd(const char *sysname, const char *desc,
                                  filter_func filter, const char *filter_arg)
{
    return -1;
}

#endif

static char *get_compositor_render_node(void)
{
    struct wayland *wayland = wayland_process_acquire();
    char *compositor_render_node = NULL;
    drmDevicePtr dev_ptr;

    if (!wayland->dmabuf.default_feedback)
        goto out;

    if (drmGetDeviceFromDevId(wayland->dmabuf.default_feedback->main_device,
                              0, &dev_ptr) < 0)
        goto out;

    if (dev_ptr->available_nodes & (1 << DRM_NODE_RENDER))
        compositor_render_node = strdup(dev_ptr->nodes[DRM_NODE_RENDER]);

    drmFreeDevice(&dev_ptr);

out:
    wayland_process_release();
    return compositor_render_node;
}

/**********************************************************************
 *          wayland_gbm_create_surface
 */
struct gbm_surface *wayland_gbm_create_surface(uint32_t drm_format, int width, int height,
                                               size_t count_modifiers, uint64_t *modifiers,
                                               BOOL format_is_scanoutable)
{
    uint32_t gbm_bo_flags = GBM_BO_USE_RENDERING;
    size_t i;

    if (TRACE_ON(waylanddrv))
    {
        TRACE("%dx%d %.4s scanout=%d count_mods=%zu\n",
              width, height, (const char *)&drm_format,
              format_is_scanoutable, count_modifiers);

        for (i = 0; i < count_modifiers; i++)
            TRACE("    mod: 0x%.16llx\n", (long long)modifiers[i]);
    }

    if (format_is_scanoutable) gbm_bo_flags |= GBM_BO_USE_SCANOUT;

    if (count_modifiers)
    {
        struct gbm_surface *surf;

#ifdef HAVE_GBM_SURFACE_CREATE_WITH_MODIFIERS2
        surf = gbm_surface_create_with_modifiers2(process_gbm_device, width, height,
                                                  drm_format, modifiers, count_modifiers, gbm_bo_flags);
#else
        surf = gbm_surface_create_with_modifiers(process_gbm_device, width, height,
                                                 drm_format, modifiers, count_modifiers);
#endif
        if (surf) return surf;

        TRACE("Failed to create gbm surface with explicit modifiers API " \
              "(errno=%d), falling back to implicit modifiers API\n", errno);

        for (i = 0; i < count_modifiers; i++)
            if (modifiers[i] == DRM_FORMAT_MOD_INVALID) break;

        if (i == count_modifiers)
        {
            ERR("Will not create gbm surface with implicit modifiers API, as " \
                "that is not supported by the compositor\n");
            return NULL;
        }
    }

    return gbm_surface_create(process_gbm_device, width, height, drm_format, gbm_bo_flags);
}

static void wayland_gbm_init_once(void)
{
    int drm_fd = -1;
    char *compositor_render_node = get_compositor_render_node();
    const char *dri_prime = getenv("DRI_PRIME");
    const char *desc;

    if (option_drm_device)
    {
        drm_fd = open(option_drm_device, O_RDWR);
        TRACE("Trying to open drm device (from options) %s => fd=%d\n",
              option_drm_device, drm_fd);
        if (drm_fd < 0)
            WARN("Failed to open device from DRMDevice driver option\n");
    }

    if (drm_fd < 0 && dri_prime)
    {
        if (strcmp(dri_prime, "1") == 0)
        {
            if (compositor_render_node)
            {
                /* DRI_PRIME is 1, so we open the non-default device (device
                 * that is different from whatever the compositor is using) */
                desc = "from DRI_PRIME == 1, different from compositor render node";
                drm_fd = wayland_gbm_get_drm_fd(render_node_sysname, desc,
                                                filter_has_different_devnode, compositor_render_node);
            }
            else if (is_primary_system_gpu_set())
            {
                /* We don't know what device the compositor is using, so we
                 * consider that the primary system GPU is the default device. */
                desc = "from DRI_PRIME == 1, different from primary system GPU";
                drm_fd = wayland_gbm_get_drm_fd(render_node_sysname, desc,
                                                filter_is_not_primary_system_gpu, NULL);
            }
        }
        else
        {
            /* DRI_PRIME should be set to ID_TAG_PATH of the GPU the user wants
             * us to use. */
            desc = "from DRI_PRIME == ID_PATH_TAG";
            drm_fd = wayland_gbm_get_drm_fd(render_node_sysname, desc,
                                            filter_has_same_id_path_tag, dri_prime);
        }

        if (drm_fd < 0)
            WARN("Failed to open DRI_PRIME device\n");
    }

    if (drm_fd < 0 && compositor_render_node)
    {
        drm_fd = open(compositor_render_node, O_RDWR);
        TRACE("Trying to open drm device (from compositor render node) %s => fd=%d\n",
              compositor_render_node, drm_fd);
        if (drm_fd < 0)
            WARN("Failed to open drm device that compositor is using\n");
    }

    if (drm_fd < 0)
    {
        desc = "random render node";
        drm_fd = wayland_gbm_get_drm_fd(render_node_sysname, desc, NULL, NULL);
        if (drm_fd < 0)
            WARN("Failed to find a suitable render node\n");
    }

    if (drm_fd < 0)
    {
        drm_fd = open(default_render_node, O_RDWR);
        TRACE("Trying to open drm device (default render node) %s => fd=%d\n",
              default_render_node, drm_fd);
        if (drm_fd < 0)
            WARN("Failed to open default render node\n");
    }

    if (drm_fd < 0)
    {
        desc = "random primary node";
        drm_fd = wayland_gbm_get_drm_fd(primary_node_sysname, desc, NULL, NULL);
        if (drm_fd < 0)
            WARN("Failed to find a suitable primary node\n");
    }

    if (drm_fd < 0)
    {
        drm_fd = open(default_primary_node, O_RDWR);
        TRACE("Trying to open drm device (default primary node) %s => fd=%d\n",
              default_primary_node, drm_fd);
        if (drm_fd < 0)
            WARN("Failed to open default primary node\n");
    }

    free(compositor_render_node);

    if (drm_fd < 0)
    {
        ERR("Failed to find a suitable drm device\n");
        return;
    }

    process_gbm_device = gbm_create_device(drm_fd);
    if (!process_gbm_device)
    {
        ERR("Failed to create gbm device (errno=%d)\n", errno);
        close(drm_fd);
    }
}

/**********************************************************************
 *          wayland_gbm_get_render_dev
 */
dev_t wayland_gbm_get_render_dev()
{
    int dev_fd = gbm_device_get_fd(process_gbm_device);
    struct stat dev_stat;

    if (dev_fd >= 0 && !fstat(dev_fd, &dev_stat))
        return dev_stat.st_rdev;

    return 0;
}

BOOL wayland_gbm_init(void)
{
    pthread_once(&init_once, wayland_gbm_init_once);

    return process_gbm_device != NULL;
}
