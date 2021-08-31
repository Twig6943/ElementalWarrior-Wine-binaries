/*
 * Wayland driver
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

#ifndef __WINE_WAYLANDDRV_H
#define __WINE_WAYLANDDRV_H

#ifndef __WINE_CONFIG_H
# error You must include config.h to use this header
#endif

#include <pthread.h>
#include <stdarg.h>
#include <wayland-client.h>

#include "windef.h"
#include "winbase.h"
#include "ntuser.h"

#include "unixlib.h"

/**********************************************************************
 *          Globals
 */

extern struct wl_display *process_wl_display DECLSPEC_HIDDEN;

/**********************************************************************
 *          Definitions for wayland types
 */

struct wayland_mutex
{
    pthread_mutex_t mutex;
    UINT owner_tid;
    int lock_count;
    const char *name;
};

struct wayland
{
    struct wl_list thread_link;
    BOOL initialized;
    DWORD process_id;
    DWORD thread_id;
    struct wl_display *wl_display;
    struct wl_event_queue *wl_event_queue;
    struct wl_registry *wl_registry;
    struct wl_compositor *wl_compositor;
};

/**********************************************************************
 *          Wayland thread data
 */

struct wayland_thread_data
{
    struct wayland wayland;
};

extern struct wayland_thread_data *wayland_init_thread_data(void) DECLSPEC_HIDDEN;

static inline struct wayland_thread_data *wayland_thread_data(void)
{
    return (struct wayland_thread_data *)(UINT_PTR)NtUserGetThreadInfo()->driver_data;
}

static inline struct wayland *thread_init_wayland(void)
{
    return &wayland_init_thread_data()->wayland;
}

static inline struct wayland *thread_wayland(void)
{
    struct wayland_thread_data *data = wayland_thread_data();
    if (!data) return NULL;
    return &data->wayland;
}

/**********************************************************************
 *          Wayland initialization
 */

BOOL wayland_process_init(void) DECLSPEC_HIDDEN;
BOOL wayland_init(struct wayland *wayland) DECLSPEC_HIDDEN;
void wayland_deinit(struct wayland *wayland) DECLSPEC_HIDDEN;

/**********************************************************************
 *          Wayland mutex
 */

void wayland_mutex_init(struct wayland_mutex *wayland_mutex, int kind,
                        const char *name) DECLSPEC_HIDDEN;
void wayland_mutex_destroy(struct wayland_mutex *wayland_mutex) DECLSPEC_HIDDEN;
void wayland_mutex_lock(struct wayland_mutex *wayland_mutex) DECLSPEC_HIDDEN;
void wayland_mutex_unlock(struct wayland_mutex *wayland_mutex) DECLSPEC_HIDDEN;

#endif /* __WINE_WAYLANDDRV_H */
