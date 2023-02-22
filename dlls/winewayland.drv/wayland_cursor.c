/*
 * Wayland cursor handling
 *
 * Copyright 1993, 1994, 1995, 1996, 2001, 2013-2017 Alexandre Julliard
 * Copyright 1993 David Metcalfe
 * Copyright 1995, 1996 Alex Korobka
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

#define NONAMELESSUNION
#define NONAMELESSSTRUCT

#include "waylanddrv.h"

#include "ntgdi.h"
#include "ntuser.h"
#include "wine/debug.h"
#include "wine/server.h"

#include <stdlib.h>

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

static HCURSOR last_cursor;
static HCURSOR invalid_cursor;

/***********************************************************************
 *           get_icon_info
 *
 * Local GetIconInfoExW helper implementation.
 */
static BOOL get_icon_info(HICON handle, ICONINFOEXW *ret)
{
    UNICODE_STRING module, res_name;
    ICONINFO info;

    module.Buffer = ret->szModName;
    module.MaximumLength = sizeof(ret->szModName) - sizeof(WCHAR);
    res_name.Buffer = ret->szResName;
    res_name.MaximumLength = sizeof(ret->szResName) - sizeof(WCHAR);
    if (!NtUserGetIconInfo(handle, &info, &module, &res_name, NULL, 0)) return FALSE;
    ret->fIcon = info.fIcon;
    ret->xHotspot = info.xHotspot;
    ret->yHotspot = info.yHotspot;
    ret->hbmColor = info.hbmColor;
    ret->hbmMask = info.hbmMask;
    ret->wResID = res_name.Length ? 0 : LOWORD(res_name.Buffer);
    ret->szModName[module.Length] = 0;
    ret->szResName[res_name.Length] = 0;
    return TRUE;
}

/***********************************************************************
 *           create_mono_cursor_buffer
 *
 * Return a monochrome icon/cursor wl_shm_buffer
 */
static struct wayland_shm_buffer *create_mono_cursor_buffer(struct wayland *wayland,
                                                            HBITMAP bmp)
{
    struct wayland_shm_buffer *shm_buffer = NULL;
    BITMAP bm;
    char *mask = NULL;
    unsigned int i, j, stride, mask_size, *ptr;

    if (!NtGdiExtGetObjectW(bmp, sizeof(bm), &bm)) return NULL;
    stride = ((bm.bmWidth + 15) >> 3) & ~1;
    mask_size = stride * bm.bmHeight;
    if (!(mask = malloc(mask_size))) return NULL;
    if (!NtGdiGetBitmapBits(bmp, mask_size, mask)) goto done;

    bm.bmHeight /= 2;
    shm_buffer = wayland_shm_buffer_create(wayland, bm.bmWidth, bm.bmHeight,
                                           WL_SHM_FORMAT_ARGB8888);
    if (!shm_buffer) goto done;

    ptr = shm_buffer->map_data;
    for (i = 0; i < bm.bmHeight; i++)
    {
        for (j = 0; j < bm.bmWidth; j++, ptr++)
        {
            int and = ((mask[i * stride + j / 8] << (j % 8)) & 0x80);
            int xor = ((mask[(i + bm.bmHeight) * stride + j / 8] << (j % 8)) & 0x80);
            if (!xor && and)
                *ptr = 0;
            else if (xor && !and)
                *ptr = 0xffffffff;
            else
                /* we can't draw "invert" pixels, so render them as black instead */
                *ptr = 0xff000000;
        }
    }

done:
    free(mask);
    return shm_buffer;
}

/***********************************************************************
 *           get_bitmap_argb
 *
 * Return the bitmap bits in ARGB format. Helper for setting icons and cursors.
 */
static struct wayland_shm_buffer *create_color_cursor_buffer(struct wayland *wayland,
                                                             HDC hdc, HBITMAP color,
                                                             HBITMAP mask)
{
    struct wayland_shm_buffer *shm_buffer = NULL;
    char buffer[FIELD_OFFSET(BITMAPINFO, bmiColors[256])];
    BITMAPINFO *info = (BITMAPINFO *)buffer;
    BITMAP bm;
    unsigned int *ptr, *bits = NULL;
    unsigned char *mask_bits = NULL;
    int i, j;
    BOOL has_alpha = FALSE;

    if (!NtGdiExtGetObjectW(color, sizeof(bm), &bm)) goto failed;

    shm_buffer = wayland_shm_buffer_create(wayland, bm.bmWidth, bm.bmHeight,
                                           WL_SHM_FORMAT_ARGB8888);
    if (!shm_buffer) goto failed;
    bits = shm_buffer->map_data;

    info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info->bmiHeader.biWidth = bm.bmWidth;
    info->bmiHeader.biHeight = -bm.bmHeight;
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biBitCount = 32;
    info->bmiHeader.biCompression = BI_RGB;
    info->bmiHeader.biSizeImage = bm.bmWidth * bm.bmHeight * 4;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrUsed = 0;
    info->bmiHeader.biClrImportant = 0;

    if (!NtGdiGetDIBitsInternal(hdc, color, 0, bm.bmHeight, bits, info, DIB_RGB_COLORS, 0, 0)) goto failed;

    for (i = 0; i < bm.bmWidth * bm.bmHeight; i++)
        if ((has_alpha = (bits[i] & 0xff000000) != 0)) break;

    if (!has_alpha)
    {
        unsigned int width_bytes = (bm.bmWidth + 31) / 32 * 4;
        /* generate alpha channel from the mask */
        info->bmiHeader.biBitCount = 1;
        info->bmiHeader.biSizeImage = width_bytes * bm.bmHeight;
        if (!(mask_bits = malloc(info->bmiHeader.biSizeImage))) goto failed;
        if (!NtGdiGetDIBitsInternal(hdc, mask, 0, bm.bmHeight, mask_bits, info, DIB_RGB_COLORS, 0, 0)) goto failed;
        ptr = bits;
        for (i = 0; i < bm.bmHeight; i++)
            for (j = 0; j < bm.bmWidth; j++, ptr++)
                if (!((mask_bits[i * width_bytes + j / 8] << (j % 8)) & 0x80)) *ptr |= 0xff000000;
        free(mask_bits);
    }

    /* Wayland requires pre-multiplied alpha values */
    for (ptr = bits, i = 0; i < bm.bmWidth * bm.bmHeight; ptr++, i++)
    {
        unsigned char alpha = *ptr >> 24;
        if (alpha == 0)
        {
            *ptr = 0;
        }
        else if (alpha != 255)
        {
            *ptr = (alpha << 24) |
                   (((BYTE)(*ptr >> 16) * alpha / 255) << 16) |
                   (((BYTE)(*ptr >> 8) * alpha / 255) << 8) |
                   (((BYTE)*ptr * alpha / 255));
        }
    }

    return shm_buffer;

failed:
    if (shm_buffer)
        wayland_shm_buffer_destroy(shm_buffer);
    free(mask_bits);
    return NULL;
}

static struct wayland_cursor *wayland_cursor_from_win32(struct wayland_pointer *pointer,
                                                        HCURSOR handle)
{
    ICONINFOEXW info = { 0 };
    struct wayland_cursor *wayland_cursor = NULL;
    struct wayland_shm_buffer *shm_buffer = NULL;

    if (!handle) return NULL;

    wayland_cursor = calloc(1, sizeof(*wayland_cursor));
    if (!wayland_cursor) goto out;

    if (!get_icon_info(handle, &info)) goto out;

    if (info.hbmColor)
    {
        HDC hdc = NtGdiCreateCompatibleDC(0);
        shm_buffer = create_color_cursor_buffer(pointer->wayland, hdc,
                                                info.hbmColor, info.hbmMask);
        NtGdiDeleteObjectApp(hdc);
    }
    else
    {
        shm_buffer = create_mono_cursor_buffer(pointer->wayland, info.hbmMask);
    }

    if (!shm_buffer) goto out;

    wayland_cursor->width = shm_buffer->width;
    wayland_cursor->height = shm_buffer->height;
    wayland_cursor->wl_buffer =
        wayland_shm_buffer_steal_wl_buffer_and_destroy(shm_buffer);

    /* make sure hotspot is valid */
    if (info.xHotspot >= wayland_cursor->width ||
        info.yHotspot >= wayland_cursor->height)
    {
        info.xHotspot = wayland_cursor->width / 2;
        info.yHotspot = wayland_cursor->height / 2;
    }

    if (pointer->focused_surface)
    {
        wayland_surface_coords_rounded_from_wine(pointer->focused_surface,
                                                 info.xHotspot, info.yHotspot,
                                                 &wayland_cursor->hotspot_x,
                                                 &wayland_cursor->hotspot_y);
    }
    else
    {
        wayland_cursor->hotspot_x = info.xHotspot;
        wayland_cursor->hotspot_y = info.yHotspot;
    }

out:
    if (info.hbmColor) NtGdiDeleteObjectApp(info.hbmColor);
    if (info.hbmMask) NtGdiDeleteObjectApp(info.hbmMask);
    if (wayland_cursor && !wayland_cursor->wl_buffer)
    {
        wayland_cursor_destroy(wayland_cursor);
        wayland_cursor = NULL;
    }
    return wayland_cursor;
}

/***********************************************************************
 *           wayland_cursor_destroy
 *
 *  Destroy a Wayland cursor and its associated resources.
 */
void wayland_cursor_destroy(struct wayland_cursor *wayland_cursor)
{
    if (!wayland_cursor)
        return;

    if (wayland_cursor->wl_buffer)
        wl_buffer_destroy(wayland_cursor->wl_buffer);

    free(wayland_cursor);
}

/***********************************************************************
 *           wayland_pointer_update_cursor_from_win32
 *
 *  Update a Wayland pointer to use the specified cursor, or NULL
 *  to hide the cursor.
 */
void wayland_pointer_update_cursor_from_win32(struct wayland_pointer *pointer,
                                              HCURSOR handle)
{
    struct wayland_cursor *wayland_cursor = pointer->cursor;

    TRACE("pointer=%p pointer->hcursor=%p handle=%p\n",
          pointer, pointer ? pointer->hcursor : 0, handle);

    if (!pointer->wl_pointer)
        return;

    if (pointer->hcursor != handle)
    {
        wayland_cursor = wayland_cursor_from_win32(pointer, handle);
        /* If we can't create a cursor from a valid handle, better to keep the
         * previous cursor than make it disappear completely. */
        if (!wayland_cursor && handle)
            return;

        if (pointer->cursor)
            wayland_cursor_destroy(pointer->cursor);
    }

    pointer->cursor = wayland_cursor;
    pointer->hcursor = handle;

    if (!pointer->cursor)
    {
            wl_pointer_set_cursor(pointer->wl_pointer,
                                  pointer->enter_serial,
                                  NULL, 0, 0);
            return;
    }

    wl_surface_attach(pointer->cursor_wl_surface, pointer->cursor->wl_buffer, 0, 0);
    wl_surface_damage_buffer(pointer->cursor_wl_surface, 0, 0,
                             wayland_cursor->width, wayland_cursor->height);

    wl_surface_commit(pointer->cursor_wl_surface);

    wl_pointer_set_cursor(pointer->wl_pointer,
                          pointer->enter_serial,
                          pointer->cursor_wl_surface,
                          pointer->cursor->hotspot_x,
                          pointer->cursor->hotspot_y);
}

/***********************************************************************
 *           wayland_init_set_cursor
 *
 *  Initalize internal information, so that we can track the last set
 *  cursor properly.
 */
BOOL wayland_init_set_cursor(void)
{
    /* Allocate a handle that we are going to treat as invalid. */
    SERVER_START_REQ(alloc_user_handle)
    {
        if (!wine_server_call_err(req))
            invalid_cursor = wine_server_ptr_handle(reply->handle);
    }
    SERVER_END_REQ;

    TRACE("invalid_cursor=%p\n", invalid_cursor);

    last_cursor = invalid_cursor;

    return invalid_cursor != NULL;
}

static HWND wayland_get_thread_cursor_hwnd(void)
{
    struct wayland *wayland = thread_wayland();
    HWND cursor_hwnd;

    if (wayland && wayland->pointer.focused_surface)
        cursor_hwnd = wayland->pointer.focused_surface->hwnd;
    else
        cursor_hwnd = NULL;

    return cursor_hwnd;
}

/***********************************************************************
 *           wayland_reapply_thread_cursor
 *
 *  Reapply the cursor settings in the current thread.
 */
void wayland_reapply_thread_cursor(void)
{
    HWND cursor_hwnd = wayland_get_thread_cursor_hwnd();

    TRACE("cursor_hwnd=%p\n", cursor_hwnd);

    if (!cursor_hwnd) return;

    /* Invalidate the set cursor cache, so that next update is
     * unconditionally applied. */
    __atomic_store_n(&last_cursor, invalid_cursor, __ATOMIC_SEQ_CST);
    /* Reapply the current cursor, using NtUserSetCursor() instead of
     * directly calling our driver function, so that the per-thread cursor
     * visibility state (i.e., ShowCursor()), which is difficult to access
     * otherwise, is taken into account. */
    NtUserSetCursor(NtUserGetCursor());
}

/***********************************************************************
 *           WAYLAND_SetCursor
 */
void WAYLAND_SetCursor(HCURSOR hcursor)
{
    HWND cursor_hwnd = wayland_get_thread_cursor_hwnd();

    TRACE("hcursor=%p last_cursor=%p cursor_hwnd=%p\n",
          hcursor, last_cursor, cursor_hwnd);

    if (!cursor_hwnd) return;

    if (__atomic_exchange_n(&last_cursor, hcursor, __ATOMIC_SEQ_CST) != hcursor)
    {
        send_message(cursor_hwnd, WM_WAYLAND_SET_CURSOR, GetCurrentThreadId(),
                     (LPARAM)hcursor);
    }
}
