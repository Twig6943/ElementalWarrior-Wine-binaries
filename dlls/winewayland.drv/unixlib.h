/*
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

#ifndef __WINE_WAYLANDDRV_UNIXLIB_H
#define __WINE_WAYLANDDRV_UNIXLIB_H

#include "windef.h"
#include "ntuser.h"
#include "wine/unixlib.h"

/* A pointer to memory that is guaranteed to be usable by both 32-bit and
 * 64-bit processes. */
typedef UINT PTR32;

enum waylanddrv_unix_func
{
    waylanddrv_unix_func_init,
    waylanddrv_unix_func_read_events,
    waylanddrv_unix_func_clipboard_message,
    waylanddrv_unix_func_data_offer_accept_format,
    waylanddrv_unix_func_data_offer_import_format,
    waylanddrv_unix_func_data_offer_enum_formats,
    waylanddrv_unix_func_count,
};

struct waylanddrv_unix_clipboard_message_params
{
    HWND hwnd;
    UINT msg;
    WPARAM wparam;
    LPARAM lparam;
};

struct waylanddrv_unix_data_offer_accept_format_params
{
    PTR32 data_offer;
    UINT format;
};

struct waylanddrv_unix_data_offer_import_format_params
{
    PTR32 data_offer;
    UINT format;
    PTR32 data;
    UINT size;
};

struct waylanddrv_unix_data_offer_enum_formats_params
{
    PTR32 data_offer;
    UINT *formats;
    UINT num_formats;
};

/* driver client callbacks exposed with KernelCallbackTable interface */
enum waylanddrv_client_func
{
    waylanddrv_client_func_create_clipboard_window = NtUserDriverCallbackFirst,
    waylanddrv_client_func_last
};

C_ASSERT(waylanddrv_client_func_last <= NtUserDriverCallbackLast + 1);

#endif /* __WINE_WAYLANDDRV_UNIXLIB_H */
