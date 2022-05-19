/*
 * Wayland driver unicode helpers
 *
 * Copyright (c) 2022 Alexandros Frantzis for Collabora Ltd
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

/**********************************************************************
 *          ascii_to_unicode_maybe_z
 *
 * Converts an ascii, possibly zero-terminated, string containing up to
 * src_max_chars to a unicode string. Returns the number of characters
 * (including any trailing zero) in the source ascii string. If the returned
 * number of characters is greater than dst_max_chars the output will have been
 * truncated.
 */
size_t ascii_to_unicode_maybe_z(WCHAR *dst, size_t dst_max_chars,
                                const char *src, size_t src_max_chars)
{
    size_t src_len = 0;

    while (src_max_chars--)
    {
        src_len++;
        if (dst_max_chars)
        {
            *dst++ = *src;
            dst_max_chars--;
        }
        if (!*src++) break;
    }

    return src_len;
}

/**********************************************************************
 *          unicode_to_ascii_maybe_z
 *
 * Converts a unicode, possibly zero-terminated, string containing up to
 * src_max_chars to an ascii string. Returns the number of characters
 * (including any trailing zero) in the source unicode string. If the returned
 * number of characters is greater than dst_max_chars the output will have been
 * truncated.
 */
size_t unicode_to_ascii_maybe_z(char *dst, size_t dst_max_chars,
                                const WCHAR *src, size_t src_max_chars)
{
    size_t src_len = 0;

    while (src_max_chars--)
    {
        src_len++;
        if (dst_max_chars)
        {
            *dst++ = *src;
            dst_max_chars--;
        }
        if (!*src++) break;
    }

    return src_len;
}

/**********************************************************************
 *          ascii_to_unicode_z
 *
 * Converts an ascii, possibly zero-terminated, string containing up to
 * src_max_chars to a zero-terminated unicode string. Returns the number of
 * characters (including the trailing zero) written to the destination string.
 * If there isn't enough space in the destination to hold all the characters
 * and the trailing zero, the string is truncated enough so that a trailing
 * zero can be placed.
 */
size_t ascii_to_unicode_z(WCHAR *dst, size_t dst_max_chars,
                          const char *src, size_t src_max_chars)
{
    size_t len;
    if (src_max_chars == 0) return 0;
    len = ascii_to_unicode_maybe_z(dst, dst_max_chars, src, src_max_chars);
    if (len >= dst_max_chars) len = dst_max_chars - 1;
    if (len > 0 && dst[len - 1] == 0) len--;
    dst[len] = 0;
    return len + 1;
}
