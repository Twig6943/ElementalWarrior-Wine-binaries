/*
 * Wayland data device (clipboard and DnD) handling
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

#include <assert.h>
#include <stdlib.h>

WINE_DEFAULT_DEBUG_CHANNEL(clipboard);

struct wayland_data_offer
{
    struct wayland *wayland;
    struct wl_data_offer *wl_data_offer;
    struct wl_array types;
    uint32_t source_actions;
    uint32_t action;
};

/* Normalize the mime type by skipping inconsequential characters, such as
 * spaces and double quotes, and converting to lower case. */
static char *normalize_mime_type(const char *mime)
{
    char *new_mime;
    const char *cur_read;
    char *cur_write;
    size_t new_mime_len = 0;

    cur_read = mime;
    for (; *cur_read != '\0'; cur_read++)
    {
        if (*cur_read != ' ' && *cur_read != '"')
            new_mime_len++;
    }

    new_mime = malloc(new_mime_len + 1);
    if (!new_mime) return NULL;
    cur_read = mime;
    cur_write = new_mime;

    for (; *cur_read != '\0'; cur_read++)
    {
        if (*cur_read != ' ' && *cur_read != '"')
            *cur_write++ = tolower(*cur_read);
    }

    *cur_write = '\0';

    return new_mime;
}

/**********************************************************************
 *          wl_data_offer handling
 */

static void data_offer_offer(void *data, struct wl_data_offer *wl_data_offer,
                             const char *type)
{
    struct wayland_data_offer *data_offer = data;
    char **p;

    p = wl_array_add(&data_offer->types, sizeof *p);
    *p = normalize_mime_type(type);
}

static void data_offer_source_actions(void *data,
                                      struct wl_data_offer *wl_data_offer,
                                      uint32_t source_actions)
{
    struct wayland_data_offer *data_offer = data;

    data_offer->source_actions = source_actions;
}

static void data_offer_action(void *data, struct wl_data_offer *wl_data_offer,
                              uint32_t dnd_action)
{
    struct wayland_data_offer *data_offer = data;

    data_offer->action = dnd_action;
}

static const struct wl_data_offer_listener data_offer_listener = {
    data_offer_offer,
    data_offer_source_actions,
    data_offer_action
};

static void wayland_data_offer_create(struct wayland *wayland,
                                      struct wl_data_offer *wl_data_offer)
{
    struct wayland_data_offer *data_offer;

    data_offer = calloc(1, sizeof(*data_offer));
    if (!data_offer)
    {
        ERR("Failed to allocate memory for data offer\n");
        return;
    }

    data_offer->wayland = wayland;
    data_offer->wl_data_offer = wl_data_offer;
    wl_array_init(&data_offer->types);
    wl_data_offer_add_listener(data_offer->wl_data_offer,
                               &data_offer_listener, data_offer);
}

static void wayland_data_offer_destroy(struct wayland_data_offer *data_offer)
{
    char **p;

    wl_data_offer_destroy(data_offer->wl_data_offer);
    wl_array_for_each(p, &data_offer->types)
        free(*p);
    wl_array_release(&data_offer->types);

    free(data_offer);
}

/**********************************************************************
 *          wl_data_device handling
 */

static void wayland_data_device_destroy_clipboard_data_offer(struct wayland_data_device *data_device)
{
    if (data_device->clipboard_wl_data_offer)
    {
        struct wayland_data_offer *data_offer =
            wl_data_offer_get_user_data(data_device->clipboard_wl_data_offer);
        wayland_data_offer_destroy(data_offer);
        data_device->clipboard_wl_data_offer = NULL;
    }
}

static void wayland_data_device_destroy_dnd_data_offer(struct wayland_data_device *data_device)
{
    if (data_device->dnd_wl_data_offer)
    {
        struct wayland_data_offer *data_offer =
            wl_data_offer_get_user_data(data_device->dnd_wl_data_offer);
        wayland_data_offer_destroy(data_offer);
        data_device->dnd_wl_data_offer = NULL;
    }
}

static void data_device_data_offer(void *data,
                                   struct wl_data_device *wl_data_device,
                                   struct wl_data_offer *wl_data_offer)
{
    struct wayland_data_device *data_device = data;

    wayland_data_offer_create(data_device->wayland, wl_data_offer);
}

static void data_device_enter(void *data, struct wl_data_device *wl_data_device,
                              uint32_t serial, struct wl_surface *wl_surface,
                              wl_fixed_t x_w, wl_fixed_t y_w,
                              struct wl_data_offer *wl_data_offer)
{
    struct wayland_data_device *data_device = data;

    /* Any previous dnd offer should have been freed by a drop or leave event. */
    assert(data_device->dnd_wl_data_offer == NULL);

    data_device->dnd_wl_data_offer = wl_data_offer;
}

static void data_device_leave(void *data, struct wl_data_device *wl_data_device)
{
    struct wayland_data_device *data_device = data;

    wayland_data_device_destroy_dnd_data_offer(data_device);
}

static void data_device_motion(void *data, struct wl_data_device *wl_data_device,
                               uint32_t time, wl_fixed_t x_w, wl_fixed_t y_w)
{
}

static void data_device_drop(void *data, struct wl_data_device *wl_data_device)
{
    struct wayland_data_device *data_device = data;

    wayland_data_device_destroy_dnd_data_offer(data_device);
}

static void data_device_selection(void *data,
                                  struct wl_data_device *wl_data_device,
                                  struct wl_data_offer *wl_data_offer)
{
    struct wayland_data_device *data_device = data;

    /* Destroy any previous data offer. */
    wayland_data_device_destroy_clipboard_data_offer(data_device);

    data_device->clipboard_wl_data_offer = wl_data_offer;
}

static const struct wl_data_device_listener data_device_listener = {
    data_device_data_offer,
    data_device_enter,
    data_device_leave,
    data_device_motion,
    data_device_drop,
    data_device_selection
};

/**********************************************************************
 *          wayland_data_device_init
 *
 * Initializes the data_device extension in order to support clipboard
 * operations.
 */
void wayland_data_device_init(struct wayland_data_device *data_device,
                              struct wayland *wayland)
{
    data_device->wayland = wayland;
    data_device->wl_data_device =
        wl_data_device_manager_get_data_device(wayland->wl_data_device_manager,
                                               wayland->wl_seat);

    wl_data_device_add_listener(data_device->wl_data_device, &data_device_listener,
                                data_device);
}

/**********************************************************************
 *          wayland_data_device_deinit
 */
void wayland_data_device_deinit(struct wayland_data_device *data_device)
{
    wayland_data_device_destroy_clipboard_data_offer(data_device);
    wayland_data_device_destroy_dnd_data_offer(data_device);

    if (data_device->wl_data_device)
        wl_data_device_destroy(data_device->wl_data_device);

    memset(data_device, 0, sizeof(*data_device));
}

/**********************************************************************
 *          waylanddrv_unix_clipboard_message
 */
NTSTATUS waylanddrv_unix_clipboard_message(void *arg)
{
    struct waylanddrv_unix_clipboard_message_params *params = arg;

    switch (params->msg)
    {
    case WM_NCCREATE:
        return TRUE;
    }

    return NtUserMessageCall(params->hwnd, params->msg, params->wparam,
                             params->lparam, NULL, NtUserDefWindowProc, FALSE);
}

/**********************************************************************
 *          wayland_data_device_ensure_clipboard_window
 *
 * Creates (if not already created) the window which handles clipboard
 * messages for the specified wayland instance.
 */
void wayland_data_device_ensure_clipboard_window(struct wayland *wayland)
{
    if (!wayland->clipboard_hwnd)
    {
        wayland->clipboard_hwnd =
            ULongToHandle(WAYLANDDRV_CLIENT_CALL(create_clipboard_window, NULL, 0));
    }
}
