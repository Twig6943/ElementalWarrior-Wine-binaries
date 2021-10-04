/*
 * Wayland input handling
 *
 * Copyright (c) 2020 Alexandros Frantzis for Collabora Ltd
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

#include "winuser.h"

#include <linux/input.h>

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

/**********************************************************************
 *          Pointer handling
 */

static void pointer_handle_motion_internal(void *data, struct wl_pointer *pointer,
                                           uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
    struct wayland *wayland = data;
    HWND focused_hwnd = wayland->pointer.focused_surface ?
                        wayland->pointer.focused_surface->hwnd : 0;
    INPUT input = {0};
    int screen_x, screen_y;
    RECT screen_rect;

    if (!focused_hwnd)
        return;

    wayland_surface_coords_to_screen(wayland->pointer.focused_surface,
                                     wl_fixed_to_double(sx),
                                     wl_fixed_to_double(sy),
                                     &screen_x, &screen_y);

    /* Sometimes, due to rounding, we may end up with pointer coordinates
     * slightly outside the target window, so bring them within bounds. */
    if (NtUserGetWindowRect(focused_hwnd, &screen_rect))
    {
        if (screen_x >= screen_rect.right) screen_x = screen_rect.right - 1;
        else if (screen_x < screen_rect.left) screen_x = screen_rect.left;
        if (screen_y >= screen_rect.bottom) screen_y = screen_rect.bottom - 1;
        else if (screen_y < screen_rect.top) screen_y = screen_rect.top;
    }

    TRACE("surface=%p hwnd=%p wayland_xy=%.2f,%.2f screen_xy=%d,%d\n",
          wayland->pointer.focused_surface, focused_hwnd,
          wl_fixed_to_double(sx), wl_fixed_to_double(sy),
          screen_x, screen_y);

    input.type           = INPUT_MOUSE;
    input.mi.dx          = screen_x;
    input.mi.dy          = screen_y;
    input.mi.dwFlags     = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;

    wayland->last_dispatch_mask |= QS_MOUSEMOVE;

    __wine_send_input(focused_hwnd, &input, NULL);
}

static void pointer_handle_motion(void *data, struct wl_pointer *pointer,
                                  uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
    pointer_handle_motion_internal(data, pointer, time, sx, sy);
}

static void pointer_handle_enter(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface,
                                 wl_fixed_t sx, wl_fixed_t sy)
{
    struct wayland *wayland = data;
    struct wayland_surface *wayland_surface =
        surface ? wl_surface_get_user_data(surface) : NULL;

    /* Since pointer events can arrive in multiple threads, ensure we only
     * handle them in the thread that owns the surface, to avoid passing
     * duplicate events to Wine. */
    if (wayland_surface && wayland_surface->hwnd &&
        wayland_surface->wayland == wayland)
    {
        TRACE("surface=%p hwnd=%p\n", wayland_surface, wayland_surface->hwnd);
        wayland->pointer.focused_surface = wayland_surface;
        wayland->pointer.enter_serial = serial;
        /* Handle the enter as a motion, to account for cases where the
         * window first appears beneath the pointer and won't get a separate
         * motion event. */
        pointer_handle_motion_internal(data, pointer, 0, sx, sy);
    }
}

static void pointer_handle_leave(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface)
{
    struct wayland *wayland = data;

    if (wayland->pointer.focused_surface &&
        wayland->pointer.focused_surface->wl_surface == surface)
    {
        TRACE("surface=%p hwnd=%p\n",
              wayland->pointer.focused_surface,
              wayland->pointer.focused_surface->hwnd);
        wayland->pointer.focused_surface = NULL;
        wayland->pointer.enter_serial = 0;
    }
}

static void pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
                                  uint32_t serial, uint32_t time, uint32_t button,
                                  uint32_t state)
{
    struct wayland *wayland = data;
    HWND focused_hwnd = wayland->pointer.focused_surface ?
                        wayland->pointer.focused_surface->hwnd : 0;
    INPUT input = {0};

    if (!focused_hwnd)
        return;

    TRACE("button=%#x state=%#x hwnd=%p\n", button, state, focused_hwnd);

    input.type = INPUT_MOUSE;

    switch (button)
    {
    case BTN_LEFT: input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN; break;
    case BTN_RIGHT: input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN; break;
    case BTN_MIDDLE: input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN; break;
    default: break;
    }

    if (state == WL_POINTER_BUTTON_STATE_RELEASED)
        input.mi.dwFlags <<= 1;

    wayland->last_dispatch_mask |= QS_MOUSEBUTTON;

    __wine_send_input(focused_hwnd, &input, NULL);
}

static void pointer_handle_axis(void *data, struct wl_pointer *wl_pointer,
                                uint32_t time, uint32_t axis, wl_fixed_t value)
{
}

static void pointer_handle_frame(void *data, struct wl_pointer *wl_pointer)
{
}

static void pointer_handle_axis_source(void *data, struct wl_pointer *wl_pointer,
                                       uint32_t axis_source)
{
}

static void pointer_handle_axis_stop(void *data, struct wl_pointer *wl_pointer,
                                     uint32_t time, uint32_t axis)
{
}

static void pointer_handle_axis_discrete(void *data, struct wl_pointer *wl_pointer,
                                         uint32_t axis, int32_t discrete)
{
    struct wayland *wayland = data;
    HWND focused_hwnd = wayland->pointer.focused_surface ?
                        wayland->pointer.focused_surface->hwnd : 0;
    INPUT input = {0};

    if (!focused_hwnd)
        return;

    TRACE("axis=%#x discrete=%d hwnd=%p\n", axis, discrete, focused_hwnd);

    input.type = INPUT_MOUSE;

    switch (axis)
    {
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
        input.mi.dwFlags = MOUSEEVENTF_WHEEL;
        input.mi.mouseData = -WHEEL_DELTA * discrete;
        break;
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
        input.mi.dwFlags = MOUSEEVENTF_HWHEEL;
        input.mi.mouseData = WHEEL_DELTA * discrete;
        break;
    default: break;
    }

    wayland->last_dispatch_mask |= QS_MOUSEBUTTON;

    __wine_send_input(focused_hwnd, &input, NULL);
}

static const struct wl_pointer_listener pointer_listener = {
    pointer_handle_enter,
    pointer_handle_leave,
    pointer_handle_motion,
    pointer_handle_button,
    pointer_handle_axis,
    pointer_handle_frame,
    pointer_handle_axis_source,
    pointer_handle_axis_stop,
    pointer_handle_axis_discrete,
};

void wayland_pointer_init(struct wayland_pointer *pointer, struct wayland *wayland,
                          struct wl_pointer *wl_pointer)
{
    wayland->pointer.wayland = wayland;
    wayland->pointer.wl_pointer = wl_pointer;
    wl_pointer_add_listener(wayland->pointer.wl_pointer, &pointer_listener, wayland);
}

void wayland_pointer_deinit(struct wayland_pointer *pointer)
{
    if (pointer->wl_pointer)
        wl_pointer_destroy(pointer->wl_pointer);

    memset(pointer, 0, sizeof(*pointer));
}
