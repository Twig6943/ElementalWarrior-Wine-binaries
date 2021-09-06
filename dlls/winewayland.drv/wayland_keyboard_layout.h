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

static const UINT xkb_keycode_to_vkey_us[] =
{
    0, 0, 0, 0, 0, 0, 0, 0,
    0,                   /* KEY_RESERVED  0 */
    VK_ESCAPE,           /* KEY_ESC   1 */
    '1',                 /* KEY_1   2 */
    '2',                 /* KEY_2   3 */
    '3',                 /* KEY_3   4 */
    '4',                 /* KEY_4   5 */
    '5',                 /* KEY_5   6 */
    '6',                 /* KEY_6   7 */
    '7',                 /* KEY_7   8 */
    '8',                 /* KEY_8   9 */
    '9',                 /* KEY_9   10 */
    '0',                 /* KEY_0   11 */
    VK_OEM_MINUS,        /* KEY_MINUS  12 */
    VK_OEM_PLUS,         /* KEY_EQUAL  13 */
    VK_BACK,             /* KEY_BACKSPACE 14 */
    VK_TAB,              /* KEY_TAB   15 */
    'Q',                 /* KEY_Q   16 */
    'W',                 /* KEY_W   17 */
    'E',                 /* KEY_E   18 */
    'R',                 /* KEY_R   19 */
    'T',                 /* KEY_T   20 */
    'Y',                 /* KEY_Y   21 */
    'U',                 /* KEY_U   22 */
    'I',                 /* KEY_I   23 */
    'O',                 /* KEY_O   24 */
    'P',                 /* KEY_P   25 */
    VK_OEM_4,            /* KEY_LEFTBRACE  26 */
    VK_OEM_6,            /* KEY_RIGHTBRACE  27 */
    VK_RETURN,           /* KEY_ENTER  28 */
    VK_LCONTROL,         /* KEY_LEFTCTRL  29 */
    'A',                 /* KEY_A   30 */
    'S',                 /* KEY_S   31 */
    'D',                 /* KEY_D   32 */
    'F',                 /* KEY_F   33 */
    'G',                 /* KEY_G   34 */
    'H',                 /* KEY_H   35 */
    'J',                 /* KEY_J   36 */
    'K',                 /* KEY_K   37 */
    'L',                 /* KEY_L   38 */
    VK_OEM_1,            /* KEY_SEMICOLON  39 */
    VK_OEM_7,            /* KEY_APOSTROPHE  40 */
    VK_OEM_3,            /* KEY_GRAVE  41 */
    VK_LSHIFT,           /* KEY_LEFTSHIFT  42 */
    VK_OEM_5,            /* KEY_BACKSLASH  43 */
    'Z',                 /* KEY_Z   44 */
    'X',                 /* KEY_X   45 */
    'C',                 /* KEY_C   46 */
    'V',                 /* KEY_V   47 */
    'B',                 /* KEY_B   48 */
    'N',                 /* KEY_N   49 */
    'M',                 /* KEY_M   50 */
    VK_OEM_COMMA,        /* KEY_COMMA  51 */
    VK_OEM_PERIOD,       /* KEY_DOT   52 */
    VK_OEM_2,            /* KEY_SLASH  53 */
    VK_RSHIFT,           /* KEY_RIGHTSHIFT  54 */
    VK_MULTIPLY,         /* KEY_KPASTERISK  55 */
    VK_LMENU,            /* KEY_LEFTALT  56 */
    VK_SPACE,            /* KEY_SPACE  57 */
    VK_CAPITAL,          /* KEY_CAPSLOCK  58 */
    VK_F1,               /* KEY_F1   59 */
    VK_F2,               /* KEY_F2   60 */
    VK_F3,               /* KEY_F3   61 */
    VK_F4,               /* KEY_F4   62 */
    VK_F5,               /* KEY_F5   63 */
    VK_F6,               /* KEY_F6   64 */
    VK_F7,               /* KEY_F7   65 */
    VK_F8,               /* KEY_F8   66 */
    VK_F9,               /* KEY_F9   67 */
    VK_F10,              /* KEY_F10   68 */
    VK_NUMLOCK,          /* KEY_NUMLOCK  69 */
    VK_SCROLL,           /* KEY_SCROLLLOCK  70 */
    VK_HOME,             /* KEY_KP7   71 */
    VK_UP,               /* KEY_KP8   72 */
    VK_PRIOR,            /* KEY_KP9   73 */
    VK_SUBTRACT,         /* KEY_KPMINUS  74 */
    VK_LEFT,             /* KEY_KP4   75 */
    VK_CLEAR,            /* KEY_KP5   76 */
    VK_RIGHT,            /* KEY_KP6   77 */
    VK_ADD,              /* KEY_KPPLUS  78 */
    VK_END,              /* KEY_KP1   79 */
    VK_DOWN,             /* KEY_KP2   80 */
    VK_NEXT,             /* KEY_KP3   81 */
    VK_INSERT,           /* KEY_KP0   82 */
    VK_DELETE,           /* KEY_KPDOT  83 */
    0,                   /* 84 */
    0,                   /* KEY_ZENKAKUHANKAKU 85 */
    VK_OEM_102,          /* KEY_102ND  86 */
    VK_F11,              /* KEY_F11   87 */
    VK_F12,              /* KEY_F12   88 */
    0,                   /* KEY_RO   89 */
    0,                   /* KEY_KATAKANA  90 */
    0,                   /* KEY_HIRAGANA  91 */
    0,                   /* KEY_HENKAN  92 */
    0,                   /* KEY_KATAKANAHIRAGANA 93 */
    0,                   /* KEY_MUHENKAN  94 */
    0,                   /* KEY_KPJPCOMMA  95 */
    VK_RETURN,           /* KEY_KPENTER  96 */
    VK_RCONTROL,         /* KEY_RIGHTCTRL  97 */
    VK_DIVIDE,           /* KEY_KPSLASH  98 */
    VK_SNAPSHOT,         /* KEY_SYSRQ  99 */
    VK_RMENU,            /* KEY_RIGHTALT  100 */
    0,                   /* KEY_LINEFEED  101 */
    VK_HOME,             /* KEY_HOME  102 */
    VK_UP,               /* KEY_UP   103 */
    VK_PRIOR,            /* KEY_PAGEUP  104 */
    VK_LEFT,             /* KEY_LEFT  105 */
    VK_RIGHT,            /* KEY_RIGHT  106 */
    VK_END,              /* KEY_END   107 */
    VK_DOWN,             /* KEY_DOWN  108 */
    VK_NEXT,             /* KEY_PAGEDOWN  109 */
    VK_INSERT,           /* KEY_INSERT  110 */
    VK_DELETE,           /* KEY_DELETE  111 */
    0,                   /* KEY_MACRO  112 */
    VK_VOLUME_MUTE,      /* KEY_MUTE  113 */
    VK_VOLUME_DOWN,      /* KEY_VOLUMEDOWN  114 */
    VK_VOLUME_UP,        /* KEY_VOLUMEUP  115 */
    0,                   /* KEY_POWER  116  */
    0,                   /* KEY_KPEQUAL  117 */
    0,                   /* KEY_KPPLUSMINUS  118 */
    VK_PAUSE,            /* KEY_PAUSE  119 */
    0,                   /* KEY_SCALE  120  */
    0,                   /* KEY_KPCOMMA  121 */
    0,                   /* KEY_HANGEUL  122 */
    0,                   /* KEY_HANJA  123 */
    0,                   /* KEY_YEN   124 */
    VK_LWIN,             /* KEY_LEFTMETA  125 */
    VK_RWIN,             /* KEY_RIGHTMETA  126 */
    0,                   /* KEY_COMPOSE  127 */
    0,                   /* KEY_STOP  128  */
    0,                   /* KEY_AGAIN  129 */
    0,                   /* KEY_PROPS  130  */
    0,                   /* KEY_UNDO  131  */
    0,                   /* KEY_FRONT  132 */
    0,                   /* KEY_COPY  133  */
    0,                   /* KEY_OPEN  134  */
    0,                   /* KEY_PASTE  135  */
    0,                   /* KEY_FIND  136  */
    0,                   /* KEY_CUT   137  */
    0,                   /* KEY_HELP  138  */
    0,                   /* KEY_MENU  139  */
    0,                   /* KEY_CALC  140  */
    0,                   /* KEY_SETUP  141 */
    0,                   /* KEY_SLEEP  142  */
    0,                   /* KEY_WAKEUP  143  */
    0,                   /* KEY_FILE  144  */
    0,                   /* KEY_SENDFILE  145 */
    0,                   /* KEY_DELETEFILE  146 */
    0,                   /* KEY_XFER  147 */
    0,                   /* KEY_PROG1  148 */
    0,                   /* KEY_PROG2  149 */
    0,                   /* KEY_WWW   150  */
    0,                   /* KEY_MSDOS  151 */
    0,                   /* KEY_COFFEE  152 */
    0,                   /* KEY_ROTATE_DISPLAY 153  */
    0,                   /* KEY_CYCLEWINDOWS 154 */
    0,                   /* KEY_MAIL  155 */
    0,                   /* KEY_BOOKMARKS  156  */
    0,                   /* KEY_COMPUTER  157 */
    0,                   /* KEY_BACK  158  */
    0,                   /* KEY_FORWARD  159  */
    0,                   /* KEY_CLOSECD  160 */
    0,                   /* KEY_EJECTCD  161 */
    0,                   /* KEY_EJECTCLOSECD 162 */
    VK_MEDIA_NEXT_TRACK, /* KEY_NEXTSONG  163 */
    VK_MEDIA_PLAY_PAUSE, /* KEY_PLAYPAUSE  164 */
    VK_MEDIA_PREV_TRACK, /* KEY_PREVIOUSSONG 165 */
    0,                   /* KEY_STOPCD  166 */
    0,                   /* KEY_RECORD  167 */
    0,                   /* KEY_REWIND  168 */
    0,                   /* KEY_PHONE  169  */
    0,                   /* KEY_ISO   170 */
    0,                   /* KEY_CONFIG  171  */
    0,                   /* KEY_HOMEPAGE  172  */
    0,                   /* KEY_REFRESH  173  */
    0,                   /* KEY_EXIT  174  */
    0,                   /* KEY_MOVE  175 */
    0,                   /* KEY_EDIT  176 */
    0,                   /* KEY_SCROLLUP  177 */
    0,                   /* KEY_SCROLLDOWN  178 */
    0,                   /* KEY_KPLEFTPAREN  179 */
    0,                   /* KEY_KPRIGHTPAREN 180 */
    0,                   /* KEY_NEW   181  */
    0,                   /* KEY_REDO  182  */
    VK_F13,              /* KEY_F13   183 */
    VK_F14,              /* KEY_F14   184 */
    VK_F15,              /* KEY_F15   185 */
    VK_F16,              /* KEY_F16   186 */
    VK_F17,              /* KEY_F17   187 */
    VK_F18,              /* KEY_F18   188 */
    VK_F19,              /* KEY_F19   189 */
    VK_F20,              /* KEY_F20   190 */
    VK_F21,              /* KEY_F21   191 */
    VK_F22,              /* KEY_F22   192 */
    VK_F23,              /* KEY_F23   193 */
    VK_F24,              /* KEY_F24   194 */
    0,                   /* 195 */
    0,                   /* 196 */
    0,                   /* 197 */
    0,                   /* 198 */
    0,                   /* 199 */
    0,                   /* KEY_PLAYCD  200 */
    0,                   /* KEY_PAUSECD  201 */
    0,                   /* KEY_PROG3  202 */
    0,                   /* KEY_PROG4  203 */
    0,                   /* KEY_DASHBOARD  204  */
    0,                   /* KEY_SUSPEND  205 */
    0,                   /* KEY_CLOSE  206  */
    VK_PLAY,             /* KEY_PLAY  207 */
    0,                   /* KEY_FASTFORWARD  208 */
    0,                   /* KEY_BASSBOOST  209 */
    VK_PRINT,            /* KEY_PRINT  210  */
    0,                   /* KEY_HP   211 */
    0,                   /* KEY_CAMERA  212 */
    0,                   /* KEY_SOUND  213 */
    0,                   /* KEY_QUESTION  214  */
    0,                   /* KEY_EMAIL  215 */
    0,                   /* KEY_CHAT  216 */
    0,                   /* KEY_SEARCH  217 */
    0,                   /* KEY_CONNECT  218 */
    0,                   /* KEY_FINANCE  219  */
    0,                   /* KEY_SPORT  220 */
    0,                   /* KEY_SHOP  221 */
    0,                   /* KEY_ALTERASE  222 */
    0,                   /* KEY_CANCEL  223  */
    0,                   /* KEY_BRIGHTNESSDOWN 224 */
    0,                   /* KEY_BRIGHTNESSUP 225 */
    0,                   /* KEY_MEDIA  226 */
    0,                   /* KEY_SWITCHVIDEOMODE 227  */
    0,                   /* KEY_KBDILLUMTOGGLE 228 */
    0,                   /* KEY_KBDILLUMDOWN 229 */
    0,                   /* KEY_KBDILLUMUP  230 */
    0,                   /* KEY_SEND  231  */
    0,                   /* KEY_REPLY  232  */
    0,                   /* KEY_FORWARDMAIL  233  */
    0,                   /* KEY_SAVE  234  */
    0,                   /* KEY_DOCUMENTS  235 */
    0,                   /* KEY_BATTERY  236 */
    0,                   /* KEY_BLUETOOTH  237 */
    0,                   /* KEY_WLAN  238 */
    0,                   /* KEY_UWB   239  */
    0,                   /* KEY_UNKNOWN  240 */
    0,                   /* KEY_VIDEO_NEXT  241  */
    0,                   /* KEY_VIDEO_PREV  242  */
    0,                   /* KEY_BRIGHTNESS_CYCLE 243  */
    0,                   /* KEY_BRIGHTNESS_AUTO/ZERO 244 */
    0,                   /* KEY_DISPLAY_OFF  245  */
    0,                   /* KEY_WWAN  246  */
    0,                   /* KEY_RFKILL  247  */
    0,                   /* KEY_MICMUTE  248  */
};

#endif /* __WINE_WAYLANDDRV_WAYLAND_KEYBOARD_LAYOUT_H */
