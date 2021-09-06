/*
 * Wayland keyboard driver layouts, adapted from X11 driver.
 *
 * This header file contains the tables used by keyboard_layout.c
 * to perform layout mapping.
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

#ifndef __WINE_WAYLANDDRV_WAYLAND_KEYBOARD_LAYOUT_H
#define __WINE_WAYLANDDRV_WAYLAND_KEYBOARD_LAYOUT_H

#define MAIN_KEY_LEN 50
/* We currently use two symbols (levels) per key to differentiate layouts. */
#define MAIN_KEY_SYMBOLS_LEN 2

/* Windows uses PS/2 scan code set 1 for the scan codes sent to applications. */
static const WORD main_key_scan_ps2_set1[MAIN_KEY_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    0x29,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x7D,
    /* Row D: AD01-AD12 */
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,
    /* Row C: AC01-AC12 */
    0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x2B,
    /* Row B: LSGT, AB01-AB11 */
    0x56,0x2C,0x2D,0x2E,0x2F,0x30,0x31,0x32,0x33,0x34,0x35,0x73
};

static const WORD main_key_vkey_qwerty[MAIN_KEY_LEN] =
{
    /* NOTE: this layout must concur with the scan codes layout above */
    VK_OEM_3,'1','2','3','4','5','6','7','8','9','0',VK_OEM_MINUS,VK_OEM_PLUS,0,
    'Q','W','E','R','T','Y','U','I','O','P',VK_OEM_4,VK_OEM_6,
    'A','S','D','F','G','H','J','K','L',VK_OEM_1,VK_OEM_7,VK_OEM_5,
    VK_OEM_102,'Z','X','C','V','B','N','M',VK_OEM_COMMA,VK_OEM_PERIOD,VK_OEM_2,
};

#define K(x) XKB_KEY_##x

static const xkb_keysym_t main_key_symbols_us[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {'`', '~'}, {'1', '!'}, {'2', '@'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', '^'}, {'7', '&'}, {'8', '*'}, {'9', '('}, {'0', ')'}, {'-', '_'}, {'=', '+'}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {'[', '{'}, {']', '}'},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {';', ':'}, {'\'', '"'}, {'\\', '|'},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', '<'}, {'.', '>'}, {'/', '?'}, {},
};

#undef K

/*** Layout table. Add your keyboard mappings to this list */
static struct {
    LCID lcid; /* input locale identifier, look for LOCALE_ILANGUAGE
                 in the appropriate dlls/kernel/nls/.nls file */
    const char *name;
    const xkb_keysym_t (*symbols)[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN];
    const WORD (*scan)[MAIN_KEY_LEN]; /* scan codes mapping */
    const WORD (*vkey)[MAIN_KEY_LEN]; /* virtual key codes mapping */
} main_key_tab[]={
    {0x0409, "us", &main_key_symbols_us, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
};

const WORD xkb_keysym_0xff00_to_vkey[256] DECLSPEC_HIDDEN =
{
    /* unused */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* FF00 */
    /* special keys */
    VK_BACK, VK_TAB, 0, VK_CLEAR, 0, VK_RETURN, 0, 0,           /* FF08 */
    0, 0, 0, VK_PAUSE, VK_SCROLL, VK_SNAPSHOT, 0, 0,            /* FF10 */
    0, 0, 0, VK_ESCAPE, 0, 0, 0, 0,                             /* FF18 */
    /* Japanese special keys */
    0, VK_KANJI, VK_NONCONVERT, VK_CONVERT,                     /* FF20 */
    VK_DBE_ROMAN, 0, 0, VK_DBE_HIRAGANA,
    0, 0, VK_DBE_SBCSCHAR, 0, 0, 0, 0, 0,                       /* FF28 */
    /* Korean special keys (FF31-) */
    VK_DBE_ALPHANUMERIC, VK_HANGUL, 0, 0, VK_HANJA, 0, 0, 0,    /* FF30 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* FF38 */
    /* unused */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* FF40 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* FF48 */
    /* cursor keys */
    VK_HOME, VK_LEFT, VK_UP, VK_RIGHT,                          /* FF50 */
    VK_DOWN, VK_PRIOR, VK_NEXT, VK_END,
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* FF58 */
    /* misc keys */
    VK_SELECT, VK_SNAPSHOT, VK_EXECUTE, VK_INSERT, 0,0,0, VK_APPS, /* FF60 */
    0, VK_CANCEL, VK_HELP, VK_CANCEL, 0, 0, 0, 0,               /* FF68 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* FF70 */
    /* keypad keys */
    0, 0, 0, 0, 0, 0, 0, VK_NUMLOCK,                            /* FF78 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* FF80 */
    0, 0, 0, 0, 0, VK_RETURN, 0, 0,                             /* FF88 */
    0, 0, 0, 0, 0, VK_HOME, VK_LEFT, VK_UP,                     /* FF90 */
    VK_RIGHT, VK_DOWN, VK_PRIOR, VK_NEXT,                       /* FF98 */
    VK_END, VK_CLEAR, VK_INSERT, VK_DELETE,
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* FFA0 */
    0, 0, VK_MULTIPLY, VK_ADD,                                  /* FFA8 */
    /* Windows always generates VK_DECIMAL for Del/. on keypad while some
     * X11 keyboard layouts generate XK_KP_Separator instead of XK_KP_Decimal
     * in order to produce a locale dependent numeric separator.
     */
    VK_DECIMAL, VK_SUBTRACT, VK_DECIMAL, VK_DIVIDE,
    VK_NUMPAD0, VK_NUMPAD1, VK_NUMPAD2, VK_NUMPAD3,             /* FFB0 */
    VK_NUMPAD4, VK_NUMPAD5, VK_NUMPAD6, VK_NUMPAD7,
    VK_NUMPAD8, VK_NUMPAD9, 0, 0, 0, VK_OEM_NEC_EQUAL,          /* FFB8 */
    /* function keys */
    VK_F1, VK_F2,
    VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F10,    /* FFC0 */
    VK_F11, VK_F12, VK_F13, VK_F14, VK_F15, VK_F16, VK_F17, VK_F18, /* FFC8 */
    VK_F19, VK_F20, VK_F21, VK_F22, VK_F23, VK_F24, 0, 0,       /* FFD0 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* FFD8 */
    /* modifier keys */
    0, VK_LSHIFT, VK_RSHIFT, VK_LCONTROL,                       /* FFE0 */
    VK_RCONTROL, VK_CAPITAL, 0, VK_LMENU,
    VK_RMENU, VK_LMENU, VK_RMENU, VK_LWIN, VK_RWIN, 0, 0, 0,    /* FFE8 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* FFF0 */
    0, 0, 0, 0, 0, 0, 0, VK_DELETE                              /* FFF8 */
};

const WORD xkb_keysym_0xff00_to_scan[256] DECLSPEC_HIDDEN =
{
    /* unused */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF00 */
    /* special keys */
    0x0E, 0x0F, 0x00, /*?*/ 0, 0x00, 0x1C, 0x00, 0x00,           /* FF08 */
    0x00, 0x00, 0x00, 0xE11D, 0x46, 0x54, 0x00, 0x00,            /* FF10 */
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,              /* FF18 */
    /* Japanese special keys */
    0x00, 0x29, 0x7B, 0x79, 0x70, 0x00, 0x00, 0x70,              /* FF20 */
    0x00, 0x00, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF28 */
    /* Korean special keys (FF31-) */
    0x3A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF30 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF38 */
    /* unused */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF40 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF48 */
    /* cursor keys */
    0xE047, 0xE04B, 0xE048, 0xE04D, 0xE050, 0xE049, 0xE051, 0xE04F, /* FF50 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF58 */
    /* misc keys */
    /*?*/ 0, 0xE037, /*?*/ 0, 0xE052, 0x00, 0x00, 0x00, 0xE05D,  /* FF60 */
    /*?*/ 0, /*?*/ 0, 0x63, 0xE046, 0x00, 0x00, 0x00, 0x00,      /* FF68 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF70 */
    /* keypad keys */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x45,              /* FF78 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FF80 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0xE01C, 0x00, 0x00,            /* FF88 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x4B, 0x48,              /* FF90 */
    0x4D, 0x50, 0x49, 0x51, 0x4F, 0x4C, 0x52, 0x53,              /* FF98 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FFA0 */
    0x00, 0x00, 0x37, 0x4E, 0x53, 0x4A, 0x53, 0xE035,            /* FFA8 */
    0x52, 0x4F, 0x50, 0x51, 0x4B, 0x4C, 0x4D, 0x47,              /* FFB0 */
    0x48, 0x49, 0x00, 0x00, 0x00, 0x00,                          /* FFB8 */
    /* function keys */
    0x3B, 0x3C,
    0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44,              /* FFC0 */
    0x57, 0x58, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,              /* FFC8 */
    0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x76, 0x00, 0x00,              /* FFD0 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FFD8 */
    /* modifier keys */
    0x00, 0x2A, 0x36, 0x1D, 0xE01D, 0x3A, 0x00, 0x38,            /* FFE0 */
    0xE038, 0x38, 0xE038, 0xE05B, 0xE05C, 0x00, 0x00, 0x00,      /* FFE8 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              /* FFF0 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE053             /* FFF8 */
};

const WORD xkb_keysym_xfree86_to_vkey[256] DECLSPEC_HIDDEN =
{
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF00 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF08 */
    0, VK_VOLUME_DOWN, VK_VOLUME_MUTE, VK_VOLUME_UP,            /* 1008FF10 */
    VK_MEDIA_PLAY_PAUSE, VK_MEDIA_STOP,
    VK_MEDIA_PREV_TRACK, VK_MEDIA_NEXT_TRACK,
    0, VK_LAUNCH_MAIL, 0, VK_BROWSER_SEARCH,                    /* 1008FF18 */
    0, 0, 0, VK_BROWSER_HOME,
    0, 0, 0, 0, 0, 0, VK_BROWSER_BACK, VK_BROWSER_FORWARD,      /* 1008FF20 */
    VK_BROWSER_STOP, VK_BROWSER_REFRESH, 0, 0, 0, 0, 0, VK_SLEEP, /* 1008FF28 */
    VK_BROWSER_FAVORITES, 0, VK_LAUNCH_MEDIA_SELECT, 0,         /* 1008FF30 */
    0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF38 */
    VK_LAUNCH_APP1, VK_LAUNCH_APP2, 0, 0, 0, 0, 0, 0,           /* 1008FF40 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF48 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF50 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF58 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF60 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF68 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF70 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF78 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF80 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF88 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF90 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF98 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFA0 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFA8 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFB0 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFB8 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFC0 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFC8 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFD0 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFD8 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFE0 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFE8 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFF0 */
    0, 0, 0, 0, 0, 0, 0, 0                                      /* 1008FFF8 */
};

const WORD xkb_keysym_xfree86_to_scan[256] DECLSPEC_HIDDEN =
{
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF00 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF08 */
    0, 0xE02E, 0xE020, 0xE030, 0xE022, 0xE024, 0xE010, 0xE019,  /* 1008FF10 */
    0, 0xE06C, 0, 0xE065, 0, 0, 0, 0xE032,                      /* 1008FF18 */
    0, 0, 0, 0, 0, 0, 0xE06A, 0xE069,                           /* 1008FF20 */
    0xE068, 0xE067, 0, 0, 0, 0, 0, 0xE05F,                      /* 1008FF28 */
    0xE066, 0, 0xE06D, 0, 0, 0, 0, 0,                           /* 1008FF30 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF38 */
    0xE06B, 0xE021, 0, 0, 0, 0, 0, 0,                           /* 1008FF40 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF48 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF50 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF58 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF60 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF68 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF70 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF78 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF80 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF88 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF90 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FF98 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFA0 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFA8 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFB0 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFB8 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFC0 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFC8 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFD0 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFD8 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFE0 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFE8 */
    0, 0, 0, 0, 0, 0, 0, 0,                                     /* 1008FFF0 */
    0, 0, 0, 0, 0, 0, 0, 0                                      /* 1008FFF8 */
};

#endif /* __WINE_WAYLANDDRV_WAYLAND_KEYBOARD_LAYOUT_H */
