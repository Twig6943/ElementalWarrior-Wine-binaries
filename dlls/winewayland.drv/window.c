/*
 * Window related functions
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

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include "waylanddrv.h"

#include "wine/debug.h"

#include "ntuser.h"

#include <stdlib.h>

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

/* private window data */
struct wayland_win_data
{
    /* hwnd that this private data belongs to */
    HWND           hwnd;
    /* parent hwnd for child windows */
    HWND           parent;
    /* USER window rectangle relative to parent */
    RECT           window_rect;
    /* client area relative to parent */
    RECT           client_rect;
};

static struct wayland_mutex win_data_mutex =
{
    PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP, 0, 0, __FILE__ ": win_data_mutex"
};

static struct wayland_win_data *win_data_context[32768];

static inline int context_idx(HWND hwnd)
{
    return LOWORD(hwnd) >> 1;
}

/***********************************************************************
 *           wayland_win_data_destroy
 */
static void wayland_win_data_destroy(struct wayland_win_data *data)
{
    TRACE("hwnd=%p\n", data->hwnd);
    win_data_context[context_idx(data->hwnd)] = NULL;

    free(data);

    wayland_mutex_unlock(&win_data_mutex);
}

/***********************************************************************
 *           wayland_win_data_get
 *
 * Lock and return the data structure associated with a window.
 */
static struct wayland_win_data *wayland_win_data_get(HWND hwnd)
{
    struct wayland_win_data *data;

    if (!hwnd) return NULL;

    wayland_mutex_lock(&win_data_mutex);
    if ((data = win_data_context[context_idx(hwnd)]) && data->hwnd == hwnd)
        return data;
    wayland_mutex_unlock(&win_data_mutex);

    return NULL;
}

/***********************************************************************
 *           wayland_win_data_release
 *
 * Release the data returned by wayland_win_data_get.
 */
static void wayland_win_data_release(struct wayland_win_data *data)
{
    if (data) wayland_mutex_unlock(&win_data_mutex);
}

/***********************************************************************
 *           wayland_win_data_create
 *
 * Create a data window structure for an existing window.
 */
static struct wayland_win_data *wayland_win_data_create(HWND hwnd)
{
    struct wayland_win_data *data;
    HWND parent;

    /* Don't create win data for desktop or HWND_MESSAGE windows. */
    if (!(parent = NtUserGetAncestor(hwnd, GA_PARENT))) return NULL;
    if (parent != NtUserGetDesktopWindow() && !NtUserGetAncestor(parent, GA_PARENT))
        return NULL;

    if (!(data = calloc(1, sizeof(*data))))
        return NULL;

    data->hwnd = hwnd;

    wayland_mutex_lock(&win_data_mutex);
    win_data_context[context_idx(hwnd)] = data;

    TRACE("hwnd=%p\n", data->hwnd);

    return data;
}

/**********************************************************************
 *           WAYLAND_CreateWindow
 */
BOOL WAYLAND_CreateWindow(HWND hwnd)
{
    TRACE("%p\n", hwnd);

    if (hwnd == NtUserGetDesktopWindow())
    {
        /* Initialize wayland so that the desktop process has access
         * to all the wayland related information (e.g., displays). */
        wayland_init_thread_data();
    }

    return TRUE;
}

/***********************************************************************
 *           WAYLAND_DestroyWindow
 */
void WAYLAND_DestroyWindow(HWND hwnd)
{
    struct wayland_win_data *data;

    TRACE("%p\n", hwnd);

    if (!(data = wayland_win_data_get(hwnd))) return;
    wayland_win_data_destroy(data);
}

/***********************************************************************
 *           WAYLAND_WindowPosChanging
 */
BOOL WAYLAND_WindowPosChanging(HWND hwnd, HWND insert_after, UINT swp_flags,
                               const RECT *window_rect, const RECT *client_rect,
                               RECT *visible_rect, struct window_surface **surface)
{
    struct wayland_win_data *data = wayland_win_data_get(hwnd);
    BOOL exstyle = NtUserGetWindowLongW(hwnd, GWL_EXSTYLE);
    DWORD style = NtUserGetWindowLongW(hwnd, GWL_STYLE);
    HWND parent = NtUserGetAncestor(hwnd, GA_PARENT);

    TRACE("win %p window %s client %s visible %s style %08x ex %08x flags %08x after %p\n",
          hwnd, wine_dbgstr_rect(window_rect), wine_dbgstr_rect(client_rect),
          wine_dbgstr_rect(visible_rect), (UINT)style, exstyle, swp_flags, insert_after);

    if (!data && !(data = wayland_win_data_create(hwnd))) return TRUE;

    data->parent = (parent == NtUserGetDesktopWindow()) ? 0 : parent;
    data->window_rect = *window_rect;
    data->client_rect = *client_rect;

    wayland_win_data_release(data);
    return TRUE;
}

static void handle_wm_wayland_monitor_change(struct wayland *wayland)
{
    wayland_update_outputs_from_process(wayland);
}

/**********************************************************************
 *           WAYLAND_DesktopWindowProc
 */
LRESULT WAYLAND_DesktopWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_DISPLAYCHANGE:
        {
            RECT virtual_rect = NtUserGetVirtualScreenRect();
            NtUserSetWindowPos(hwnd, 0, virtual_rect.left, virtual_rect.top,
                               virtual_rect.right - virtual_rect.left,
                               virtual_rect.bottom - virtual_rect.top,
                               SWP_NOZORDER | SWP_NOACTIVATE | SWP_DEFERERASE);
        }
        break;
    }

    return NtUserMessageCall(hwnd, msg, wp, lp, 0, NtUserDefWindowProc, FALSE);
}

/**********************************************************************
 *           WAYLAND_WindowMessage
 */
LRESULT WAYLAND_WindowMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    TRACE("msg %x hwnd %p wp %lx lp %lx\n", msg, hwnd, (long)wp, lp);

    switch (msg)
    {
    case WM_WAYLAND_MONITOR_CHANGE:
        handle_wm_wayland_monitor_change(thread_wayland());
        break;
    default:
        FIXME("got window msg %x hwnd %p wp %lx lp %lx\n", msg, hwnd, (long)wp, lp);
    }

    return 0;
}
