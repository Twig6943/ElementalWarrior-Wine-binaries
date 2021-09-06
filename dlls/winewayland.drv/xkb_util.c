/*
 * XKB related utility functions
 *
 * Copyright 2021 Alexandros Frantzis for Collabora Ltd.
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

#include <xkbcommon/xkbcommon.h>

/**********************************************************************
 *          _xkb_state_get_active_layout
 *
 * Gets the active layout of the xkb state.
 */
xkb_layout_index_t _xkb_state_get_active_layout(struct xkb_state *xkb_state)
{
    struct xkb_keymap *xkb_keymap = xkb_state_get_keymap(xkb_state);
    xkb_layout_index_t num_layouts = xkb_keymap_num_layouts(xkb_keymap);
    xkb_layout_index_t layout;

    for (layout = 0; layout < num_layouts; layout++)
    {
        if (xkb_state_layout_index_is_active(xkb_state, layout,
                                             XKB_STATE_LAYOUT_LOCKED))
            return layout;
    }

    return XKB_LAYOUT_INVALID;
}

/**********************************************************************
 *          _xkb_keysyms_to_utf8
 *
 * Get the null-terminated UTF-8 string representation of a sequence of
 * keysyms. Returns the length of the UTF-8 string written, *not* including
 * the null byte. If no bytes were produced or in case of error returns 0
 * and produces a properly null-terminated empty string if possible.
 */
int _xkb_keysyms_to_utf8(const xkb_keysym_t *syms, int nsyms, char *utf8, int utf8_size)
{
    int i;
    int utf8_len = 0;

    if (utf8_size == 0) return 0;

    for (i = 0; i < nsyms; i++)
    {
        int nwritten = xkb_keysym_to_utf8(syms[i], utf8 + utf8_len,
                                          utf8_size - utf8_len);
        if (nwritten <= 0)
        {
            utf8_len = 0;
            break;
        }

        /* nwritten includes the terminating null byte */
        utf8_len += nwritten - 1;
    }

    utf8[utf8_len] = '\0';

    return utf8_len;
}
