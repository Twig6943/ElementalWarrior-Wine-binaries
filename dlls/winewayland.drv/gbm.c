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

#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_LIBUDEV_H
#include <libudev.h>
#endif
#include <stdlib.h>
#include <unistd.h>

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

struct gbm_device *process_gbm_device;
static pthread_once_t init_once;

static const char default_seat[] = "seat0";
static const char default_render_node[] = "/dev/dri/renderD128";
static const char default_primary_node[] = "/dev/dri/card0";
static const char primary_node_sysname[] = "card[0-9]*";
static const char render_node_sysname[] = "renderD[0-9]*";

#ifdef HAVE_UDEV

static int wayland_gbm_get_drm_fd(const char *sysname, const char *desc)
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

static int wayland_gbm_get_drm_fd(const char *sysname, const char *desc)
{
    return -1;
}

#endif

static void wayland_gbm_init_once(void)
{
    int drm_fd = -1;
    const char *desc;

    if (option_drm_device)
    {
        drm_fd = open(option_drm_device, O_RDWR);
        TRACE("Trying to open drm device (from options) %s => fd=%d\n",
              option_drm_device, drm_fd);
        if (drm_fd < 0)
            WARN("Failed to open device from DRMDevice driver option\n");
    }

    if (drm_fd < 0)
    {
        desc = "random render node";
        drm_fd = wayland_gbm_get_drm_fd(render_node_sysname, desc);
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
        drm_fd = wayland_gbm_get_drm_fd(primary_node_sysname, desc);
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

BOOL wayland_gbm_init(void)
{
    pthread_once(&init_once, wayland_gbm_init_once);

    return process_gbm_device != NULL;
}
