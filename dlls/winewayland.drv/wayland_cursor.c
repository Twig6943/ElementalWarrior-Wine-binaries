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

#include <math.h>
#include <stdlib.h>

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

static struct wl_cursor_theme *cursor_theme = NULL;

static HCURSOR last_cursor;
static HCURSOR invalid_cursor;

/* Mapping between Windows cursors and native Wayland cursors
 *
 * Note that we have multiple possible names for each Wayland cursor. This
 * happens because the names for each cursor may vary across different themes.
 *
 * This table was created based on the docs below.
 *
 * https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-loadcursora
 * https://bugs.kde.org/attachment.cgi?id=67313
 */
static const char *idc_appstarting[] = {"half-busy", "progress", "left_ptr_watch",
                                        "00000000000000020006000e7e9ffc3f",
                                        "08e8e1c95fe2fc01f976f1e063a24ccd",
                                        "3ecb610c1bf2410f44200f48c40d3599",
                                        "9116a3ea924ed2162ecab71ba103b17f"};
static const char *idc_arrow[] = {"default", "left_ptr",
                                  "top_left_arrow", "left-arrow"};
static const char *idc_cross[] = {"crosshair"};
static const char *idc_hand[] = {"pointing_hand", "pointer", "hand", "hand2"};
static const char *idc_help[] = {"help", "question_arrow", "whats_this",
                                 "5c6cd98b3f3ebcb1f9c7f1c204630408",
                                 "d9ce0ab605698f320427677b458ad60b"};
static const char *idc_ibeam[] = {"text", "ibeam", "xterm"};
static const char *idc_icon[] = {"icon"};
static const char *idc_no[] = {"forbidden", "not-allowed"};
static const char *idc_pen[] = {"pencil"};
static const char *idc_sizeall[] = {"size_all"};
static const char *idc_sizenesw[] = {"nesw-resize", "size_bdiag",
                                     "50585d75b494802d0151028115016902",
                                     "fcf1c3c7cd4491d801f1e1c78f100000"};
static const char *idc_sizens[] = {"ns-resize", "size_ver", "v_double_arrow",
                                   "00008160000006810000408080010102"};
static const char *idc_sizenwse[] = {"nwse-resize", "size_fdiag",
                                     "38c5dff7c7b8962045400281044508d2",
                                     "c7088f0f3e6c8088236ef8e1e3e70000"};
static const char *idc_sizewe[] = {"ew-resize", "size_hor", "h_double_arrow",
                                   "028006030e0e7ebffc7f7070c0600140"};
static const char *idc_uparrow[] = {"up_arrow"};
static const char *idc_wait[] = {"wait", "watch",
                                 "0426c94ea35c87780ff01dc239897213"};

static struct wl_cursor *_wl_cursor_from_wine_cursor(struct wl_cursor_theme *wl_cursor_theme,
                                                     unsigned long int wine_cursor_enum)
{
    unsigned int i, count;
    static const char **cursors;
    struct wl_cursor *cursor;

    switch(wine_cursor_enum)
    {
        case IDC_APPSTARTING:
            cursors = idc_appstarting;
            count = ARRAY_SIZE(idc_appstarting);
            break;
        case IDC_ARROW:
            cursors = idc_arrow;
            count = ARRAY_SIZE(idc_arrow);
            break;
        case IDC_CROSS:
            cursors = idc_cross;
            count = ARRAY_SIZE(idc_cross);
            break;
        case IDC_HAND:
            cursors = idc_hand;
            count = ARRAY_SIZE(idc_hand);
            break;
        case IDC_HELP:
            cursors = idc_help;
            count = ARRAY_SIZE(idc_help);
            break;
        case IDC_IBEAM:
            cursors = idc_ibeam;
            count = ARRAY_SIZE(idc_ibeam);
            break;
        case IDC_ICON:
            cursors = idc_icon;
            count = ARRAY_SIZE(idc_icon);
            break;
        case IDC_NO:
            cursors = idc_no;
            count = ARRAY_SIZE(idc_no);
            break;
        case IDC_PEN:
            cursors = idc_pen;
            count = ARRAY_SIZE(idc_pen);
            break;
        case IDC_SIZE:
        case IDC_SIZEALL:
            cursors = idc_sizeall;
            count = ARRAY_SIZE(idc_sizeall);
            break;
        case IDC_SIZENESW:
            cursors = idc_sizenesw;
            count = ARRAY_SIZE(idc_sizenesw);
            break;
        case IDC_SIZENS:
            cursors = idc_sizens;
            count = ARRAY_SIZE(idc_sizens);
            break;
        case IDC_SIZENWSE:
            cursors = idc_sizenwse;
            count = ARRAY_SIZE(idc_sizenwse);
            break;
        case IDC_SIZEWE:
            cursors = idc_sizewe;
            count = ARRAY_SIZE(idc_sizewe);
            break;
        case IDC_UPARROW:
            cursors = idc_uparrow;
            count = ARRAY_SIZE(idc_uparrow);
            break;
        case IDC_WAIT:
            cursors = idc_wait;
            count = ARRAY_SIZE(idc_wait);
            break;
        default:
            return NULL;
    }

    for (i = 0; i < count; i++)
    {
        cursor = wl_cursor_theme_get_cursor(wl_cursor_theme, cursors[i]);
        if (cursor)
            return cursor;
    }

    return NULL;
}

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

/***********************************************************************
 *           get_wine_cursor_size
 *
 * We use the Wine cursor IDC_ARROW to compute the size that we should use in
 * the Wayland native cursors. The bitmap usually does not have the same
 * dimensions of the icon, as it uses a margin. So we take the IDC_ARROW and
 * compute its height.
 */
static int get_wine_cursor_size(struct wayland *wayland)
{
    HCURSOR handle = NULL;
    ICONINFOEXW info = { 0 };
    struct wayland_shm_buffer *shm_buffer = NULL;
    unsigned int *pixels, *row, p, x, y;
    int first_non_empty_line = -1, last_non_empty_line = -1;

    handle = LoadImageW(0, (const WCHAR *)IDC_ARROW, IMAGE_CURSOR, 0, 0,
                        LR_SHARED | LR_DEFAULTSIZE);
    if (!handle)
        goto out;

    if (!get_icon_info(handle, &info))
        goto out;

    if (info.hbmColor)
    {
        HDC hdc = NtGdiCreateCompatibleDC(0);
        shm_buffer = create_color_cursor_buffer(wayland, hdc,
                                                info.hbmColor, info.hbmMask);
        NtGdiDeleteObjectApp(hdc);
    }
    else
    {
        shm_buffer = create_mono_cursor_buffer(wayland, info.hbmMask);
    }

    if (!shm_buffer)
        goto out;

    pixels = (unsigned int *) shm_buffer->map_data;

    /* Compute the height of the IDC_ARROW */
    for (y = 0; y < shm_buffer->height; y++)
    {
        row = (unsigned int *)((unsigned char *)pixels + y * shm_buffer->stride);
        for (x = 0; x < shm_buffer->width; x++)
        {
            p = row[x];
            /* alpha 0 means fully transparent, so no content in the
             * pixel - any other pixel we consider content */
            if ((p & 0xff000000) == 0)
                continue;
            /* it's the first time that we find a content pixel, so we set
             * the first non empty line variable accordingly */
            if (first_non_empty_line == -1)
                first_non_empty_line = y;
            /* we found a content pixel in a line, so update the latest line
             * that does have content */
            last_non_empty_line = y;
            /* we don't care about the other pixels of the line if we have
             * already found a content pixel on it */
            break;
        }
    }

out:
    if (handle) NtUserDestroyCursor(handle, 0);
    if (info.hbmMask) NtGdiDeleteObjectApp(info.hbmMask);
    if (info.hbmColor) NtGdiDeleteObjectApp(info.hbmColor);
    if (shm_buffer) wayland_shm_buffer_destroy(shm_buffer);

    if (first_non_empty_line == -1 || last_non_empty_line == -1)
        return -1;

    return (last_non_empty_line - first_non_empty_line + 1);
}

void wayland_cursor_theme_init(struct wayland *wayland)
{
    char *theme;
    int size;

    if (!wayland->wl_shm)
        return;

    size = get_wine_cursor_size(wayland);
    if (size <= 0)
       return;

    /* Some compositors set this env var, others don't. But that's fine, if we
     * call wl_cursor_theme_load() with theme == NULL it will fallback and try
     * to load the default system theme. */
    theme = getenv("XCURSOR_THEME");

    cursor_theme = wl_cursor_theme_load(theme, size, wayland->wl_shm);
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

    /* First try to get the native Wayland cursor (if the config option is set
     * and the per-process Wayland instance was able to load the theme) */
    if (option_use_system_cursors && cursor_theme)
    {
        struct wl_cursor_image *wl_cursor_image;
        struct wl_cursor *wl_cursor;

        wayland_cursor->owns_wl_buffer = FALSE;
        wl_cursor = _wl_cursor_from_wine_cursor(cursor_theme, MAKEINTRESOURCE(info.wResID));
        if (wl_cursor && wl_cursor->image_count > 0)
        {
            /* TODO: add animated cursor support
             * cursor->images[i] for i > 0 is only used by animations. */
            wl_cursor_image = wl_cursor->images[0];
            wayland_cursor->wl_buffer = wl_cursor_image_get_buffer(wl_cursor_image);
            if (wayland_cursor->wl_buffer)
            {
                wayland_cursor->width = wl_cursor_image->width;
                wayland_cursor->height = wl_cursor_image->height;

                if (pointer->focused_surface)
                {
                    wayland_surface_coords_rounded_from_wine(pointer->focused_surface,
                                                             wl_cursor_image->hotspot_x,
                                                             wl_cursor_image->hotspot_y,
                                                             &wayland_cursor->hotspot_x,
                                                             &wayland_cursor->hotspot_y);
                }
                else
                {
                    wayland_cursor->hotspot_x = wl_cursor_image->hotspot_x;
                    wayland_cursor->hotspot_y = wl_cursor_image->hotspot_y;
                }
            }
        }
    }

    /* If we couldn't get native Wayland cursor (or we didn't even try,
     * because the config to use it was not set), we copy the Wine cursor
     * content to a wl_buffer */
    if (!wayland_cursor->wl_buffer)
    {
        wayland_cursor->owns_wl_buffer = TRUE;
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
    {
        /* When using Wayland native cursors, we get the cursor wl_buffer from
         * using wl_cursor_image_get_buffer(). In such case, the compositor owns
         * the wl_buffer instead of us. So we should not destroy it. */
        if (wayland_cursor->owns_wl_buffer)
            wl_buffer_destroy(wayland_cursor->wl_buffer);
    }

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

    /* Scale the cursor */
    if (pointer->focused_surface)
    {
        double scale = wayland_surface_get_buffer_scale(pointer->focused_surface);

        /* Setting only the viewport is enough, but some compositors don't
         * support wp_viewport for cursor surfaces, so also set the buffer
         * scale. Note that setting viewport destination overrides
         * the buffer scale, so it's fine to set both. */
        wl_surface_set_buffer_scale(pointer->cursor_wl_surface, round(scale));

        if (pointer->cursor_wp_viewport)
        {
            int width, height;

            wayland_surface_coords_rounded_from_wine(pointer->focused_surface,
                    pointer->cursor->width,
                    pointer->cursor->height,
                    &width, &height);
            wp_viewport_set_destination(pointer->cursor_wp_viewport, width, height);
        }
    }
    else
    {
        wl_surface_set_buffer_scale(pointer->cursor_wl_surface, 1);

        if (pointer->cursor_wp_viewport)
            wp_viewport_set_destination(pointer->cursor_wp_viewport, -1, -1);
    }


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
    RECT clip;

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
    /* Reapply the current cursor clip, so that the wayland pointer
     * constraint is updated for the newly entered window. */
    NtUserClipCursor(NtUserGetClipCursor(&clip) ? &clip : NULL);
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
        /* Cursor visibility affects pointer confinement mode. */
        send_message(cursor_hwnd, WM_WAYLAND_POINTER_CONSTRAINT_UPDATE,
                     WAYLAND_POINTER_CONSTRAINT_RETAIN_CLIP, 0);
    }
}

/***********************************************************************
 *           WAYLAND_ClipCursor
 */
BOOL WAYLAND_ClipCursor(const RECT *clip)
{
    HWND cursor_hwnd = wayland_get_thread_cursor_hwnd();
    WPARAM constrain;

    if (!cursor_hwnd) return TRUE;

    constrain = clip ? WAYLAND_POINTER_CONSTRAINT_SYSTEM_CLIP :
                       WAYLAND_POINTER_CONSTRAINT_UNSET_CLIP;

    send_message(cursor_hwnd, WM_WAYLAND_POINTER_CONSTRAINT_UPDATE, constrain, 0);

    return TRUE;
}

/***********************************************************************
 *           WAYLAND_SetCursorPos
 */
BOOL WAYLAND_SetCursorPos(int x, int y)
{
    HWND cursor_hwnd = wayland_get_thread_cursor_hwnd();

    TRACE("cursor_hwnd=%p, x=%d, y=%d\n", cursor_hwnd, x, y);

    if (!cursor_hwnd) return TRUE;

    send_message(cursor_hwnd, WM_WAYLAND_POINTER_CONSTRAINT_UPDATE,
                 WAYLAND_POINTER_CONSTRAINT_SET_CURSOR_POS, 0);

    return TRUE;
}
