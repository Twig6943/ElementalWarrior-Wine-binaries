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

#include <assert.h>
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
    /* wayland surface (if any) representing this window on the wayland side */
    struct wayland_surface *wayland_surface;
    /* wine window_surface backing this window */
    struct window_surface *window_surface;
    /* pending wine window_surface for this window */
    struct window_surface *pending_window_surface;
    /* whether the pending_window_surface value is valid */
    BOOL           has_pending_window_surface;
    /* whether this window is visible */
    BOOL           visible;
    /* whether a wayland surface update is needed */
    BOOL           wayland_surface_needs_update;
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

    if (data->has_pending_window_surface && data->pending_window_surface)
    {
        wayland_window_surface_update_wayland_surface(data->pending_window_surface, NULL);
        window_surface_release(data->pending_window_surface);
    }
    if (data->window_surface)
    {
        wayland_window_surface_update_wayland_surface(data->window_surface, NULL);
        window_surface_release(data->window_surface);
    }
    if (data->wayland_surface) wayland_surface_unref(data->wayland_surface);
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
    data->wayland_surface_needs_update = TRUE;

    wayland_mutex_lock(&win_data_mutex);
    win_data_context[context_idx(hwnd)] = data;

    TRACE("hwnd=%p\n", data->hwnd);

    return data;
}

/***********************************************************************
 *           wayland_surface_for_hwnd_unlocked
 *
 * Helper function to get the wayland_surface for a HWND without any locking.
 * The caller should ensure that win_data_mutex has been locked before this
 * operation, and for as long as the association between the HWND and the
 * returned wayland_surface needs to remain valid.
 */
static struct wayland_surface *wayland_surface_for_hwnd_unlocked(HWND hwnd)
{
    struct wayland_win_data *data;

    assert(win_data_mutex.owner_tid == GetCurrentThreadId());

    if ((data = win_data_context[context_idx(hwnd)]) && data->hwnd == hwnd)
        return data->wayland_surface;

    return NULL;
}

static BOOL wayland_win_data_wayland_surface_needs_update(struct wayland_win_data *data)
{
    if (data->wayland_surface_needs_update)
        return TRUE;

    /* If this is currently or potentially a toplevel surface, and its
     * visibility state has changed, recreate win_data so that we only have
     * xdg_toplevels for visible windows. */
    if (data->wayland_surface && !data->wayland_surface->wl_subsurface)
    {
        BOOL visible = data->wayland_surface->xdg_toplevel != NULL;
        if (data->visible != visible)
            return TRUE;
    }

    return FALSE;
}

static struct wayland_surface *update_surface_for_role(struct wayland_win_data *data,
                                                       enum wayland_surface_role role,
                                                       struct wayland *wayland,
                                                       struct wayland_surface *parent_surface)
{
    struct wayland_surface *surface = data->wayland_surface;

    if (!surface ||
        (role != WAYLAND_SURFACE_ROLE_NONE &&
         surface->role != WAYLAND_SURFACE_ROLE_NONE &&
         surface->role != role))
    {
        surface = wayland_surface_create_plain(wayland);
        if (surface) wayland_mutex_lock(&surface->mutex);
        surface->hwnd = data->hwnd;
    }
    else
    {
        /* Lock the wayland surface to avoid other threads interacting with it
         * while we are updating. */
        wayland_mutex_lock(&surface->mutex);
        wayland_surface_clear_role(surface);
    }

    if (role == WAYLAND_SURFACE_ROLE_TOPLEVEL)
        wayland_surface_make_toplevel(surface, parent_surface);
    else if (role == WAYLAND_SURFACE_ROLE_SUBSURFACE)
        wayland_surface_make_subsurface(surface, parent_surface);

    wayland_mutex_unlock(&surface->mutex);

    return surface;
}

static void wayland_win_data_update_wayland_surface(struct wayland_win_data *data)
{
    struct wayland *wayland = thread_wayland();
    HWND effective_parent_hwnd;
    struct wayland_surface *surface;
    struct wayland_surface *parent_surface;
    DWORD style;

    TRACE("hwnd=%p\n", data->hwnd);

    data->wayland_surface_needs_update = FALSE;

    /* GWLP_HWNDPARENT gets the owner for any kind of toplevel windows,
     * and the parent for child windows. */
    effective_parent_hwnd = (HWND)NtUserGetWindowLongPtrW(data->hwnd, GWLP_HWNDPARENT);
    parent_surface = NULL;

    if (effective_parent_hwnd)
        parent_surface = wayland_surface_for_hwnd_unlocked(effective_parent_hwnd);

    style = NtUserGetWindowLongW(data->hwnd, GWL_STYLE);

    /* Use wayland subsurfaces for children windows and windows that are
     * transient (i.e., don't have a titlebar). Otherwise, if the window is
     * visible make it wayland toplevel. Finally, if the window is not visible
     * create a plain (without a role) surface to avoid polluting the
     * compositor with empty xdg_toplevels. */
    if ((style & WS_CAPTION) != WS_CAPTION)
    {
        surface = update_surface_for_role(data, WAYLAND_SURFACE_ROLE_SUBSURFACE,
                                          wayland, parent_surface);
    }
    else if (data->visible)
    {
        surface = update_surface_for_role(data, WAYLAND_SURFACE_ROLE_TOPLEVEL,
                                          wayland, parent_surface);
    }
    else
    {
        surface = update_surface_for_role(data, WAYLAND_SURFACE_ROLE_NONE,
                                          wayland, parent_surface);
    }

    if (data->wayland_surface != surface)
    {
        if (data->wayland_surface)
            wayland_surface_unref(data->wayland_surface);
        data->wayland_surface = surface;
    }
}

static struct wayland_win_data *update_wayland_state(struct wayland_win_data *data)
{
    HWND hwnd = data->hwnd;

    /* Ensure we have a thread wayland instance. Perform the initialization
     * outside the win_data lock to avoid potential deadlocks. */
    if (!thread_wayland())
    {
        wayland_win_data_release(data);
        thread_init_wayland();
        data = wayland_win_data_get(hwnd);
        if (!data) return NULL;
    }

    if (data->has_pending_window_surface)
    {
        if (data->window_surface)
        {
            if (data->window_surface != data->pending_window_surface)
                wayland_window_surface_update_wayland_surface(data->window_surface, NULL);
            window_surface_release(data->window_surface);
        }
        data->window_surface = data->pending_window_surface;
        data->has_pending_window_surface = FALSE;
        data->pending_window_surface = NULL;
    }

    if (wayland_win_data_wayland_surface_needs_update(data))
        wayland_win_data_update_wayland_surface(data);

    if (data->window_surface)
    {
        wayland_window_surface_update_wayland_surface(data->window_surface,
                                                      data->wayland_surface);
        if (wayland_window_surface_needs_flush(data->window_surface))
            wayland_window_surface_flush(data->window_surface);
    }

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
    RECT surface_rect;

    TRACE("win %p window %s client %s visible %s style %08x ex %08x flags %08x after %p\n",
          hwnd, wine_dbgstr_rect(window_rect), wine_dbgstr_rect(client_rect),
          wine_dbgstr_rect(visible_rect), (UINT)style, exstyle, swp_flags, insert_after);

    if (!data && !(data = wayland_win_data_create(hwnd))) return TRUE;

    data->parent = (parent == NtUserGetDesktopWindow()) ? 0 : parent;
    data->window_rect = *window_rect;
    data->client_rect = *client_rect;
    data->visible = ((style & WS_VISIBLE) == WS_VISIBLE ||
                     (swp_flags & SWP_SHOWWINDOW)) &&
                    !(swp_flags & SWP_HIDEWINDOW);

    /* Release the dummy surface wine provides for toplevels. */
    if (*surface) window_surface_release(*surface);
    *surface = NULL;

    /* Check if we don't want a dedicated window surface. */
    if (data->parent || !data->visible) goto done;

    surface_rect = *window_rect;
    OffsetRect(&surface_rect, -surface_rect.left, -surface_rect.top);

    /* Check if we can reuse our current window surface. */
    if (data->window_surface &&
        EqualRect(&data->window_surface->rect, &surface_rect))
    {
        window_surface_add_ref(data->window_surface);
        *surface = data->window_surface;
        TRACE("reusing surface %p\n", *surface);
        goto done;
    }

    /* Create new window surface. */
    *surface = wayland_window_surface_create(data->hwnd, &surface_rect);

done:
    wayland_win_data_release(data);
    return TRUE;
}

/***********************************************************************
 *           WAYLAND_WindowPosChanged
 */
void WAYLAND_WindowPosChanged(HWND hwnd, HWND insert_after, UINT swp_flags,
                              const RECT *window_rect, const RECT *client_rect,
                              const RECT *visible_rect, const RECT *valid_rects,
                              struct window_surface *surface)
{
    struct wayland_win_data *data;

    if (!(data = wayland_win_data_get(hwnd))) return;

    TRACE("hwnd %p window %s client %s visible %s style %08x after %p flags %08x\n",
          hwnd, wine_dbgstr_rect(window_rect), wine_dbgstr_rect(client_rect),
          wine_dbgstr_rect(visible_rect), (UINT)NtUserGetWindowLongW(hwnd, GWL_STYLE),
          insert_after, swp_flags);

    if (surface) window_surface_add_ref(surface);
    if (data->has_pending_window_surface && data->pending_window_surface)
        window_surface_release(data->pending_window_surface);
    data->pending_window_surface = surface;
    data->has_pending_window_surface = TRUE;

    data = update_wayland_state(data);

    wayland_win_data_release(data);
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
