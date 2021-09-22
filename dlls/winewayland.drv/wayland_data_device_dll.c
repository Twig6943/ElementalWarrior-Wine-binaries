/*
 * Wayland data device (clipboard and DnD) handling (DLL code)
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

#include "waylanddrv_dll.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(clipboard);

static LRESULT CALLBACK clipboard_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    struct waylanddrv_unix_clipboard_message_params params;

    switch (msg)
    {
    case WM_NCCREATE:
    case WM_CLIPBOARDUPDATE:
    case WM_RENDERFORMAT:
    case WM_DESTROYCLIPBOARD:
        params.hwnd = hwnd;
        params.msg = msg;
        params.wparam = wp;
        params.lparam = lp;
        return WAYLANDDRV_UNIX_CALL(clipboard_message, &params);
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

NTSTATUS WINAPI waylanddrv_client_create_clipboard_window(void *arg, ULONG size)
{
    static const WCHAR clipboard_classname[] = {
        '_','_','w','i','n','e','_','c','l','i','p','b','o','a','r','d',
        '_','m','a','n','a','g','e','r',0
    };
    WNDCLASSW class;
    HWND clipboard_hwnd;

    memset(&class, 0, sizeof(class));
    class.lpfnWndProc = clipboard_wndproc;
    class.lpszClassName = clipboard_classname;

    if (!RegisterClassW(&class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        ERR("could not register clipboard window class err %lu\n", GetLastError());
        return 0;
    }

    if (!(clipboard_hwnd = CreateWindowW(clipboard_classname, NULL, 0, 0, 0, 0, 0,
                                         HWND_MESSAGE, 0, 0, NULL)))
    {
        ERR("failed to create clipboard window err %lu\n", GetLastError());
        return 0;
    }

    if (!AddClipboardFormatListener(clipboard_hwnd))
        ERR("failed to set clipboard listener %lu\n", GetLastError());

    TRACE("clipboard_hwnd=%p\n", clipboard_hwnd);
    return HandleToUlong(clipboard_hwnd);
}
