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
#include "xdg-output-unstable-v1-client-protocol.h"

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "ntuser.h"

#include "unixlib.h"
#include "wine/gdi_driver.h"

/**********************************************************************
 *          Globals
 */

extern struct wl_display *process_wl_display DECLSPEC_HIDDEN;

/**********************************************************************
  *          Internal messages and data
  */

enum wayland_window_message
{
    WM_WAYLAND_MONITOR_CHANGE = 0x80001000,
};

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
    struct zxdg_output_manager_v1 *zxdg_output_manager_v1;
    uint32_t next_fallback_output_id;
    struct wl_list output_list;
};

struct wayland_output_mode
{
    struct wl_list link;
    int32_t width;
    int32_t height;
    int32_t refresh;
    int bpp;
    BOOL native;
};

struct wayland_output
{
    struct wl_list link;
    struct wayland *wayland;
    struct wl_output *wl_output;
    struct zxdg_output_v1 *zxdg_output_v1;
    struct wl_list mode_list;
    struct wayland_output_mode *current_mode;
    int logical_x, logical_y;  /* logical position */
    int logical_w, logical_h;  /* logical size */
    int x, y;  /* position in native pixel coordinate space */
    double compositor_scale; /* scale factor reported by compositor */
    double scale; /* effective wayland output scale factor for hidpi */
    char *name;
    WCHAR wine_name[128];
    uint32_t global_id;
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
BOOL wayland_is_process(struct wayland *wayland) DECLSPEC_HIDDEN;
struct wayland *wayland_process_acquire(void) DECLSPEC_HIDDEN;
void wayland_process_release(void) DECLSPEC_HIDDEN;
void wayland_init_display_devices(void) DECLSPEC_HIDDEN;

/**********************************************************************
 *          Wayland mutex
 */

void wayland_mutex_init(struct wayland_mutex *wayland_mutex, int kind,
                        const char *name) DECLSPEC_HIDDEN;
void wayland_mutex_destroy(struct wayland_mutex *wayland_mutex) DECLSPEC_HIDDEN;
void wayland_mutex_lock(struct wayland_mutex *wayland_mutex) DECLSPEC_HIDDEN;
void wayland_mutex_unlock(struct wayland_mutex *wayland_mutex) DECLSPEC_HIDDEN;

/**********************************************************************
 *          Wayland output
 */

BOOL wayland_output_create(struct wayland *wayland, uint32_t id, uint32_t version) DECLSPEC_HIDDEN;
void wayland_output_destroy(struct wayland_output *output) DECLSPEC_HIDDEN;
void wayland_output_use_xdg_extension(struct wayland_output *output) DECLSPEC_HIDDEN;
void wayland_notify_wine_monitor_change(void) DECLSPEC_HIDDEN;
void wayland_update_outputs_from_process(struct wayland *wayland) DECLSPEC_HIDDEN;
struct wayland_output *wayland_output_get_by_wine_name(struct wayland *wayland,
                                                       LPCWSTR wine_name) DECLSPEC_HIDDEN;

/**********************************************************************
 *          Misc. helpers
 */

size_t ascii_to_unicode_maybe_z(WCHAR *dst, size_t dst_max_chars,
                                const char *src, size_t src_max_chars) DECLSPEC_HIDDEN;
size_t unicode_to_ascii_maybe_z(char *dst, size_t dst_max_chars,
                                const WCHAR *src, size_t src_max_chars) DECLSPEC_HIDDEN;
size_t ascii_to_unicode_z(WCHAR *dst, size_t dst_max_chars,
                          const char *src, size_t src_max_chars) DECLSPEC_HIDDEN;

/**********************************************************************
 *          USER32 helpers
 */

static inline LRESULT send_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    return NtUserMessageCall(hwnd, msg, wparam, lparam, NULL, NtUserSendDriverMessage, FALSE);
}

static inline LRESULT send_message_timeout(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam,
                                           UINT flags, UINT timeout, PDWORD_PTR res_ptr)
{
    struct send_message_timeout_params params = { .flags = flags, .timeout = timeout };
    LRESULT res = NtUserMessageCall(hwnd, msg, wparam, lparam, &params,
                                    NtUserSendMessageTimeout, FALSE);
    if (res_ptr) *res_ptr = res;
    return params.result;
}

/**********************************************************************
 *          USER driver functions
 */

BOOL WAYLAND_CreateWindow(HWND hwnd) DECLSPEC_HIDDEN;
LRESULT WAYLAND_DesktopWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) DECLSPEC_HIDDEN;
BOOL WAYLAND_UpdateDisplayDevices(const struct gdi_device_manager *device_manager,
                                  BOOL force, void *param) DECLSPEC_HIDDEN;
LRESULT WAYLAND_WindowMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) DECLSPEC_HIDDEN;

#endif /* __WINE_WAYLANDDRV_H */
