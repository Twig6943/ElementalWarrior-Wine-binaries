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
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>
#include "xdg-output-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "ntuser.h"

#include "unixlib.h"
#include "wine/gdi_driver.h"

/**********************************************************************
 *          Globals
 */

extern char *process_name DECLSPEC_HIDDEN;
extern struct wl_display *process_wl_display DECLSPEC_HIDDEN;
extern BOOL option_use_system_cursors DECLSPEC_HIDDEN;

/**********************************************************************
  *          Internal messages and data
  */

enum wayland_window_message
{
    WM_WAYLAND_MONITOR_CHANGE = 0x80001000,
    WM_WAYLAND_SET_CURSOR,
    WM_WAYLAND_QUERY_SURFACE_MAPPED,
};

enum wayland_surface_role
{
    WAYLAND_SURFACE_ROLE_NONE,
    WAYLAND_SURFACE_ROLE_SUBSURFACE,
    WAYLAND_SURFACE_ROLE_TOPLEVEL,
};

enum wayland_configure_flags
{
    WAYLAND_CONFIGURE_FLAG_RESIZING   = (1 << 0),
    WAYLAND_CONFIGURE_FLAG_ACTIVATED  = (1 << 1),
    WAYLAND_CONFIGURE_FLAG_MAXIMIZED  = (1 << 2),
    WAYLAND_CONFIGURE_FLAG_FULLSCREEN = (1 << 3),
};

/**********************************************************************
 *          Definitions for wayland types
 */

struct wayland_surface;
struct wayland_shm_buffer;

struct wayland_mutex
{
    pthread_mutex_t mutex;
    UINT owner_tid;
    int lock_count;
    const char *name;
};

struct wayland_keyboard
{
    struct wl_keyboard *wl_keyboard;
    struct wayland_surface *focused_surface;
    int repeat_interval_ms;
    int repeat_delay_ms;
    uint32_t last_pressed_key;
    uint32_t enter_serial;
    struct xkb_context *xkb_context;
    struct xkb_state *xkb_state;
    struct xkb_compose_state *xkb_compose_state;
    UINT xkb_keycode_to_vkey[256];
    WORD xkb_keycode_to_scancode[256];
    xkb_mod_mask_t xkb_mod5_mask;
};

struct wayland_cursor
{
    BOOL owns_wl_buffer;
    struct wl_buffer *wl_buffer;
    int width;
    int height;
    int hotspot_x;
    int hotspot_y;
};

struct wayland_pointer
{
    struct wayland *wayland;
    struct wl_pointer *wl_pointer;
    struct wayland_surface *focused_surface;
    struct wl_surface *cursor_wl_surface;
    uint32_t enter_serial;
    struct wayland_cursor *cursor;
    HCURSOR hcursor;
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
    struct wl_subcompositor *wl_subcompositor;
    struct xdg_wm_base *xdg_wm_base;
    struct wl_shm *wl_shm;
    struct wl_seat *wl_seat;
    struct zxdg_output_manager_v1 *zxdg_output_manager_v1;
    uint32_t next_fallback_output_id;
    struct wl_list output_list;
    struct wl_list detached_shm_buffer_list;
    struct wl_list callback_list;
    struct wayland_keyboard keyboard;
    struct wayland_pointer pointer;
    DWORD last_dispatch_mask;
    BOOL processing_events;
    int last_event_type;
    int event_notification_pipe[2];
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

struct wayland_surface_configure
{
    int width;
    int height;
    enum wayland_configure_flags configure_flags;
    uint32_t serial;
};

struct wayland_surface
{
    struct wayland *wayland;
    struct wl_surface *wl_surface;
    struct wl_subsurface *wl_subsurface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct wayland_surface *parent;
    HWND hwnd;
    struct wayland_mutex mutex;
    struct wayland_surface_configure pending;
    struct wayland_surface_configure current;
    BOOL mapped;
    LONG ref;
    enum wayland_surface_role role;
};

struct wayland_native_buffer
{
    int plane_count;
    int fds[4];
    uint32_t strides[4];
    uint32_t offsets[4];
    uint32_t width, height;
    uint32_t format;
    uint64_t modifier;
};

struct wayland_shm_buffer
{
    struct wl_list link;
    struct wl_buffer *wl_buffer;
    int width, height, stride;
    enum wl_shm_format format;
    void *map_data;
    size_t map_size;
    BOOL busy;
    HRGN damage_region;
    BOOL destroy_on_release;
};

struct wayland_buffer_queue
{
    struct wayland *wayland;
    struct wl_event_queue *wl_event_queue;
    struct wl_list buffer_list;
    int width;
    int height;
    enum wl_shm_format format;
    HRGN damage_region;
};

typedef void (*wayland_callback_func)(void *data);

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
void wayland_read_options_from_registry(void) DECLSPEC_HIDDEN;

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
 *          Wayland event dispatch
 */

int wayland_dispatch_queue(struct wl_event_queue *queue, int timeout_ms) DECLSPEC_HIDDEN;
BOOL wayland_read_events_and_dispatch_process(void) DECLSPEC_HIDDEN;
void wayland_schedule_thread_callback(uintptr_t id, int delay_ms,
                                      wayland_callback_func func, void *data) DECLSPEC_HIDDEN;
void wayland_cancel_thread_callback(uintptr_t id) DECLSPEC_HIDDEN;

/**********************************************************************
 *          Wayland surface
 */

struct wayland_surface *wayland_surface_create_plain(struct wayland *wayland) DECLSPEC_HIDDEN;
void wayland_surface_make_toplevel(struct wayland_surface *surface,
                                   struct wayland_surface *parent) DECLSPEC_HIDDEN;
void wayland_surface_make_subsurface(struct wayland_surface *surface,
                                     struct wayland_surface *parent) DECLSPEC_HIDDEN;
void wayland_surface_clear_role(struct wayland_surface *surface) DECLSPEC_HIDDEN;
BOOL wayland_surface_configure_is_compatible(struct wayland_surface_configure *conf,
                                             int width, int height,
                                             enum wayland_configure_flags flags) DECLSPEC_HIDDEN;
BOOL wayland_surface_commit_buffer(struct wayland_surface *surface,
                                   struct wayland_shm_buffer *shm_buffer,
                                   HRGN surface_damage_region) DECLSPEC_HIDDEN;
void wayland_surface_destroy(struct wayland_surface *surface) DECLSPEC_HIDDEN;
void wayland_surface_unmap(struct wayland_surface *surface) DECLSPEC_HIDDEN;
void wayland_surface_ack_pending_configure(struct wayland_surface *surface) DECLSPEC_HIDDEN;
void wayland_surface_coords_to_screen(struct wayland_surface *surface,
                                      double wayland_x, double wayland_y,
                                      int *screen_x, int *screen_y) DECLSPEC_HIDDEN;
void wayland_surface_coords_from_wine(struct wayland_surface *surface,
                                      int wine_x, int wine_y,
                                      double *wayland_x, double *wayland_y) DECLSPEC_HIDDEN;
void wayland_surface_coords_rounded_from_wine(struct wayland_surface *surface,
                                              int wine_x, int wine_y,
                                              int *wayland_x, int *wayland_y) DECLSPEC_HIDDEN;
void wayland_surface_coords_to_wine(struct wayland_surface *surface,
                                    double wayland_x, double wayland_y,
                                    int *wine_x, int *wine_y) DECLSPEC_HIDDEN;
void wayland_surface_ensure_mapped(struct wayland_surface *surface) DECLSPEC_HIDDEN;
struct wayland_surface *wayland_surface_ref(struct wayland_surface *surface) DECLSPEC_HIDDEN;
void wayland_surface_unref(struct wayland_surface *surface) DECLSPEC_HIDDEN;

/**********************************************************************
 *          Wayland native buffer
 */

BOOL wayland_native_buffer_init_shm(struct wayland_native_buffer *native,
                                    int width, int height,
                                    enum wl_shm_format format) DECLSPEC_HIDDEN;
void wayland_native_buffer_deinit(struct wayland_native_buffer *native) DECLSPEC_HIDDEN;

/**********************************************************************
 *          Wayland SHM buffer
 */

struct wayland_shm_buffer *wayland_shm_buffer_create(struct wayland *wayland,
                                                     int width, int height,
                                                     enum wl_shm_format format) DECLSPEC_HIDDEN;
void wayland_shm_buffer_destroy(struct wayland_shm_buffer *shm_buffer) DECLSPEC_HIDDEN;
struct wl_buffer *wayland_shm_buffer_steal_wl_buffer_and_destroy(struct wayland_shm_buffer *shm_buffer) DECLSPEC_HIDDEN;
void wayland_shm_buffer_clear_damage(struct wayland_shm_buffer *shm_buffer) DECLSPEC_HIDDEN;
void wayland_shm_buffer_add_damage(struct wayland_shm_buffer *shm_buffer, HRGN damage) DECLSPEC_HIDDEN;
void wayland_shm_buffer_copy(struct wayland_shm_buffer *dst_buffer,
                             struct wayland_shm_buffer *src_buffer,
                             HRGN region) DECLSPEC_HIDDEN;

/**********************************************************************
 *          Wayland buffer queue
 */

struct wayland_buffer_queue *wayland_buffer_queue_create(struct wayland *wayland,
                                                         int width, int heigh,
                                                         enum wl_shm_format format) DECLSPEC_HIDDEN;
void wayland_buffer_queue_destroy(struct wayland_buffer_queue *queue) DECLSPEC_HIDDEN;
struct wayland_shm_buffer *wayland_buffer_queue_acquire_buffer(struct wayland_buffer_queue *queue) DECLSPEC_HIDDEN;
void wayland_buffer_queue_detach_buffer(struct wayland_buffer_queue *queue,
                                        struct wayland_shm_buffer *shm_buffer,
                                        BOOL destroy_on_release) DECLSPEC_HIDDEN;
void wayland_buffer_queue_add_damage(struct wayland_buffer_queue *queue, HRGN damage) DECLSPEC_HIDDEN;

/**********************************************************************
 *          Wayland window surface
 */

struct window_surface *wayland_window_surface_create(HWND hwnd, const RECT *rect) DECLSPEC_HIDDEN;
void wayland_window_surface_flush(struct window_surface *window_surface) DECLSPEC_HIDDEN;
BOOL wayland_window_surface_needs_flush(struct window_surface *surface) DECLSPEC_HIDDEN;
void wayland_window_surface_update_wayland_surface(struct window_surface *surface,
                                                   struct wayland_surface *wayland_surface) DECLSPEC_HIDDEN;
void wayland_clear_window_surface_last_flushed(HWND hwnd) DECLSPEC_HIDDEN;

/**********************************************************************
 *          Wayland Keyboard
 */

void wayland_keyboard_init(struct wayland_keyboard *keyboard, struct wayland *wayland,
                           struct wl_keyboard *wl_keyboard) DECLSPEC_HIDDEN;
void wayland_keyboard_deinit(struct wayland_keyboard *keyboard) DECLSPEC_HIDDEN;
void wayland_keyboard_update_layout(struct wayland_keyboard *keyboard) DECLSPEC_HIDDEN;

/**********************************************************************
 *          Wayland Pointer/Cursor
 */

void wayland_pointer_init(struct wayland_pointer *pointer, struct wayland *wayland,
                          struct wl_pointer *wl_pointer) DECLSPEC_HIDDEN;
void wayland_pointer_deinit(struct wayland_pointer *pointer) DECLSPEC_HIDDEN;
void wayland_cursor_destroy(struct wayland_cursor *wayland_cursor) DECLSPEC_HIDDEN;
void wayland_cursor_theme_init(struct wayland *wayland) DECLSPEC_HIDDEN;
void wayland_pointer_update_cursor_from_win32(struct wayland_pointer *pointer,
                                              HCURSOR handle) DECLSPEC_HIDDEN;
BOOL wayland_init_set_cursor(void) DECLSPEC_HIDDEN;
void wayland_reapply_thread_cursor(void) DECLSPEC_HIDDEN;

/**********************************************************************
 *          Registry helpers
 */

HKEY reg_open_key_a(HKEY root, const char *name) DECLSPEC_HIDDEN;
HKEY reg_open_key_w(HKEY root, const WCHAR *nameW) DECLSPEC_HIDDEN;
HKEY reg_open_hkcu_key_a(const char *name) DECLSPEC_HIDDEN;
DWORD reg_get_value_a(HKEY hkey, const char *name, ULONG type, char *buffer,
                      DWORD *buffer_len) DECLSPEC_HIDDEN;

/**********************************************************************
 *          XKB helpers
 */

xkb_layout_index_t _xkb_state_get_active_layout(struct xkb_state *xkb_state) DECLSPEC_HIDDEN;
int _xkb_keysyms_to_utf8(const xkb_keysym_t *syms, int nsyms, char *utf8, int utf8_size) DECLSPEC_HIDDEN;

/**********************************************************************
 *          Misc. helpers
 */

size_t ascii_to_unicode_maybe_z(WCHAR *dst, size_t dst_max_chars,
                                const char *src, size_t src_max_chars) DECLSPEC_HIDDEN;
size_t unicode_to_ascii_maybe_z(char *dst, size_t dst_max_chars,
                                const WCHAR *src, size_t src_max_chars) DECLSPEC_HIDDEN;
size_t ascii_to_unicode_z(WCHAR *dst, size_t dst_max_chars,
                          const char *src, size_t src_max_chars) DECLSPEC_HIDDEN;
int wayland_shmfd_create(const char *name, int size) DECLSPEC_HIDDEN;
RGNDATA *get_region_data(HRGN region) DECLSPEC_HIDDEN;

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

static inline BOOL intersect_rect(RECT *dst, const RECT *src1, const RECT *src2)
{
    dst->left = max(src1->left, src2->left);
    dst->top = max(src1->top, src2->top);
    dst->right = min(src1->right, src2->right);
    dst->bottom = min(src1->bottom, src2->bottom);
    return !IsRectEmpty(dst);
}

static inline BOOL contains_rect(RECT *outer, const RECT *inner)
{
    POINT tl = {inner->left, inner->top};
    POINT br = {inner->right - 1, inner->bottom - 1};
    return PtInRect(outer, tl) && PtInRect(outer, br);
}

static inline HWND get_focus(void)
{
    GUITHREADINFO info;
    info.cbSize = sizeof(info);
    return NtUserGetGUIThreadInfo(GetCurrentThreadId(), &info) ? info.hwndFocus : 0;
}

/**********************************************************************
 *          USER driver functions
 */

BOOL WAYLAND_CreateWindow(HWND hwnd) DECLSPEC_HIDDEN;
LRESULT WAYLAND_DesktopWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) DECLSPEC_HIDDEN;
void WAYLAND_DestroyWindow(HWND hwnd) DECLSPEC_HIDDEN;
BOOL WAYLAND_GetCurrentDisplaySettings(LPCWSTR name, BOOL is_primary,
                                       LPDEVMODEW devmode) DECLSPEC_HIDDEN;
INT WAYLAND_GetDisplayDepth(LPCWSTR name, BOOL is_primary) DECLSPEC_HIDDEN;
INT WAYLAND_GetKeyNameText(LONG lparam, LPWSTR buffer, INT size) DECLSPEC_HIDDEN;
UINT WAYLAND_MapVirtualKeyEx(UINT code, UINT maptype, HKL hkl) DECLSPEC_HIDDEN;
BOOL WAYLAND_ProcessEvents(DWORD mask) DECLSPEC_HIDDEN;
void WAYLAND_SetCursor(HCURSOR hcursor) DECLSPEC_HIDDEN;
INT WAYLAND_ToUnicodeEx(UINT virt, UINT scan, const BYTE *state,
                        LPWSTR buf, int nchars, UINT flags, HKL hkl) DECLSPEC_HIDDEN;
BOOL WAYLAND_UpdateDisplayDevices(const struct gdi_device_manager *device_manager,
                                  BOOL force, void *param) DECLSPEC_HIDDEN;
SHORT WAYLAND_VkKeyScanEx(WCHAR ch, HKL hkl) DECLSPEC_HIDDEN;
LRESULT WAYLAND_WindowMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) DECLSPEC_HIDDEN;
void WAYLAND_WindowPosChanged(HWND hwnd, HWND insert_after, UINT swp_flags,
                              const RECT *window_rect, const RECT *client_rect,
                              const RECT *visible_rect, const RECT *valid_rects,
                              struct window_surface *surface) DECLSPEC_HIDDEN;
BOOL WAYLAND_WindowPosChanging(HWND hwnd, HWND insert_after, UINT swp_flags,
                               const RECT *window_rect, const RECT *client_rect,
                               RECT *visible_rect, struct window_surface **surface) DECLSPEC_HIDDEN;

#endif /* __WINE_WAYLANDDRV_H */
