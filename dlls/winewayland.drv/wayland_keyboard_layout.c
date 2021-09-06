/*
 * Wayland keyboard driver layouts, adapted from X11 driver.
 *
 * Copyright 1993 Bob Amstadt
 * Copyright 1996 Albrecht Kleine
 * Copyright 1997 David Faure
 * Copyright 1998 Morten Welinder
 * Copyright 1998 Ulrich Weigand
 * Copyright 1999 Ove Kåven
 * Copyright 2021 Alexandros Frantzis
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

#include "ime.h"
#include "winuser.h"

#include "wayland_keyboard_layout.h"

WINE_DEFAULT_DEBUG_CHANNEL(keyboard);
WINE_DECLARE_DEBUG_CHANNEL(key);

static int score_symbols(const xkb_keysym_t sym[MAIN_KEY_SYMBOLS_LEN],
                         const xkb_keysym_t ref[MAIN_KEY_SYMBOLS_LEN])
{
    int score = 0, i;

    for (i = 0; i < MAIN_KEY_SYMBOLS_LEN && ref[i]; i++)
    {
        if (ref[i] != sym[i]) return 0;
        score++;
    }

    return score;
}

static int score_layout(int layout,
                        const xkb_keysym_t symbols_for_keycode[256][MAIN_KEY_SYMBOLS_LEN])
{
    xkb_keycode_t xkb_keycode;
    int score = 0;
    int prev_key = 1000;
    char key_used[MAIN_KEY_LEN] = { 0 };

    for (xkb_keycode = 0; xkb_keycode < 256; xkb_keycode++)
    {
        int key, key_score = 0;
        const xkb_keysym_t *symbols = symbols_for_keycode[xkb_keycode];

        if (*symbols == 0)
            continue;

        for (key = 0; key < MAIN_KEY_LEN; key++)
        {
            if (key_used[key]) continue;
            key_score = score_symbols(symbols_for_keycode[xkb_keycode],
                                      (*main_key_tab[layout].symbols)[key]);
            if (key_score)
                break;
        }

        if (TRACE_ON(key))
        {
            char utf8[64];
            _xkb_keysyms_to_utf8(symbols, MAIN_KEY_SYMBOLS_LEN, utf8, sizeof(utf8));
            TRACE_(key)("xkb_keycode=%d syms={0x%x,0x%x} utf8='%s' key=%d score=%d order=%d\n",
                        xkb_keycode,
                        symbols_for_keycode[xkb_keycode][0],
                        symbols_for_keycode[xkb_keycode][1],
                        utf8, key, key_score, key_score && (key > prev_key));
        }

        if (key_score)
        {
            /* Multiply score by 100 to allow the key order bonus to break ties,
             * while not being a primary decision factor. */
            score += key_score * 100;

            /* xkb keycodes roughly follow a top left to bottom right direction
             * on the keyboard as they increase, similarly to the keys in
             * main_key_tab. Give a bonus to layouts that more closely match
             * the expected ordering. We compare with the last key to get
             * some reasonable (although local) measure of the order. */
            score += (key > prev_key);
            prev_key = key;
            key_used[key] = 1;
        }
    }

    return score;
}

static void _xkb_keymap_populate_symbols_for_keycode(
    struct xkb_keymap *xkb_keymap,
    xkb_layout_index_t layout,
    xkb_keysym_t symbols_for_keycode[256][MAIN_KEY_SYMBOLS_LEN])
{
    xkb_keycode_t xkb_keycode, min_xkb_keycode, max_xkb_keycode;

    min_xkb_keycode = xkb_keymap_min_keycode(xkb_keymap);
    max_xkb_keycode = xkb_keymap_max_keycode(xkb_keymap);
    if (max_xkb_keycode > 255) max_xkb_keycode = 255;

    for (xkb_keycode = min_xkb_keycode; xkb_keycode <= max_xkb_keycode; xkb_keycode++)
    {
        xkb_level_index_t num_levels =
            xkb_keymap_num_levels_for_key(xkb_keymap, xkb_keycode, layout);
        xkb_level_index_t level;

        if (num_levels > MAIN_KEY_SYMBOLS_LEN) num_levels = MAIN_KEY_SYMBOLS_LEN;

        for (level = 0; level < num_levels; level++)
        {
            const xkb_keysym_t *syms;
            int nsyms = xkb_keymap_key_get_syms_by_level(xkb_keymap, xkb_keycode,
                                                         layout, level, &syms);
            if (nsyms)
                symbols_for_keycode[xkb_keycode][level] = syms[0];
        }
    }
}

static int detect_main_key_layout(struct wayland_keyboard *keyboard,
                                  const xkb_keysym_t symbols_for_keycode[256][MAIN_KEY_SYMBOLS_LEN])
{
    int max_score = 0;
    int max_i = 0;

    for (int i = 0; i < ARRAY_SIZE(main_key_tab); i++)
    {
        int score = score_layout(i, symbols_for_keycode);
        if (score > max_score)
        {
            max_i = i;
            max_score = score;
        }
        TRACE("evaluated layout '%s' score %d\n", main_key_tab[i].name, score);
    }

    if (max_score == 0)
    {
        max_i = 0;
        while (strcmp(main_key_tab[max_i].name, "us")) max_i++;
        TRACE("failed to detect layout, falling back to layout 'us'\n");
    }
    else
    {
        TRACE("detected layout '%s' (score %d)\n", main_key_tab[max_i].name, max_score);
    }

    return max_i;
}

/* Populate the xkb_keycode_to_vkey[] and xkb_keycode_to_scan[] arrays based on
 * the specified main_key layout (see wayland_keyboard_layout.h) and the
 * xkb_keycode to xkb_keysym_t mappings which have been created from the
 * currently active Wayland keymap. */
static void populate_xkb_keycode_maps(struct wayland_keyboard *keyboard, int main_key_layout,
                                      const xkb_keysym_t symbols_for_keycode[256][MAIN_KEY_SYMBOLS_LEN])
{
    xkb_keycode_t xkb_keycode;
    char key_used[MAIN_KEY_LEN] = { 0 };
    const xkb_keysym_t (*lsymbols)[MAIN_KEY_SYMBOLS_LEN] =
        (*main_key_tab[main_key_layout].symbols);
    const WORD *lvkey = (*main_key_tab[main_key_layout].vkey);
    const WORD *lscan = (*main_key_tab[main_key_layout].scan);

    for (xkb_keycode = 0; xkb_keycode < 256; xkb_keycode++)
    {
        int max_key = -1;
        int max_score = 0;
        xkb_keysym_t xkb_keysym = symbols_for_keycode[xkb_keycode][0];
        UINT vkey = 0;
        WORD scan = 0;

        if ((xkb_keysym >> 8) == 0xFF)
        {
            vkey = xkb_keysym_0xff00_to_vkey[xkb_keysym & 0xff];
            scan = xkb_keysym_0xff00_to_scan[xkb_keysym & 0xff];
        }
        else if ((xkb_keysym >> 8) == 0x1008FF)
        {
            vkey = xkb_keysym_xfree86_to_vkey[xkb_keysym & 0xff];
            scan = xkb_keysym_xfree86_to_scan[xkb_keysym & 0xff];
        }
        else if (xkb_keysym == 0x20)
        {
            vkey = VK_SPACE;
            scan = 0x39;
        }
        else
        {
            int key;

            for (key = 0; key < MAIN_KEY_LEN; key++)
            {
                int score = score_symbols(symbols_for_keycode[xkb_keycode],
                                          lsymbols[key]);
                /* Consider this key if it has a better score, or the same
                 * score as a previous match that is already in use (in order
                 * to prefer unused keys). */
                if (score > max_score ||
                    (max_key >= 0 && score == max_score && key_used[max_key]))
                {
                    max_key = key;
                    max_score = score;
                }
            }

            if (max_key >= 0)
            {
                vkey = lvkey[max_key];
                scan = lscan[max_key];
                key_used[max_key] = 1;
            }
        }

        keyboard->xkb_keycode_to_vkey[xkb_keycode] = vkey;
        keyboard->xkb_keycode_to_scancode[xkb_keycode] = scan;

        if (TRACE_ON(key))
        {
            char utf8[64];
            _xkb_keysyms_to_utf8(symbols_for_keycode[xkb_keycode],
                                 MAIN_KEY_SYMBOLS_LEN, utf8, sizeof(utf8));
            TRACE_(key)("Mapped xkb_keycode=%d syms={0x%x,0x%x} utf8='%s' => "
                        "vkey=0x%x scan=0x%x\n",
                        xkb_keycode,
                        symbols_for_keycode[xkb_keycode][0],
                        symbols_for_keycode[xkb_keycode][1],
                        utf8, vkey, scan);
        }
    }
}

/***********************************************************************
 *           wayland_keyboard_update_layout
 *
 * Updates the internal weston_keyboard layout information (xkb keycode
 * mappings etc) based on the current XKB layout.
 */
void wayland_keyboard_update_layout(struct wayland_keyboard *keyboard)
{
    xkb_layout_index_t layout;
    struct xkb_state *xkb_state = keyboard->xkb_state;
    struct xkb_keymap *xkb_keymap;
    xkb_keysym_t symbols_for_keycode[256][MAIN_KEY_SYMBOLS_LEN] = { 0 };
    int main_key_layout;

    if (!xkb_state)
    {
        TRACE("no xkb state, returning\n");
        return;
    }

    layout = _xkb_state_get_active_layout(xkb_state);
    if (layout == XKB_LAYOUT_INVALID)
    {
        TRACE("no active layout, returning\n");
        return;
    }

    xkb_keymap = xkb_state_get_keymap(xkb_state);

    _xkb_keymap_populate_symbols_for_keycode(xkb_keymap, layout, symbols_for_keycode);

    main_key_layout = detect_main_key_layout(keyboard, symbols_for_keycode);

    populate_xkb_keycode_maps(keyboard, main_key_layout, symbols_for_keycode);
}
