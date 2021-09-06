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

static const WORD main_key_vkey_qwerty_jp106[MAIN_KEY_LEN] =
{
    /* NOTE: this layout must concur with the scan codes layout above */
    0,'1','2','3','4','5','6','7','8','9','0',VK_OEM_MINUS,VK_OEM_7,VK_OEM_5,
    'Q','W','E','R','T','Y','U','I','O','P',VK_OEM_3,VK_OEM_4,
    'A','S','D','F','G','H','J','K','L',VK_OEM_PLUS,VK_OEM_1,VK_OEM_6,
    VK_OEM_102,'Z','X','C','V','B','N','M',VK_OEM_COMMA,VK_OEM_PERIOD,VK_OEM_2,
};

static const WORD main_key_vkey_qwerty_v2[MAIN_KEY_LEN] =
{
    /* NOTE: this layout must concur with the scan codes layout above */
    VK_OEM_5,'1','2','3','4','5','6','7','8','9','0',VK_OEM_PLUS,VK_OEM_4,0,
    'Q','W','E','R','T','Y','U','I','O','P',VK_OEM_6,VK_OEM_1,
    'A','S','D','F','G','H','J','K','L',VK_OEM_3,VK_OEM_7,VK_OEM_2,
    VK_OEM_102,'Z','X','C','V','B','N','M',VK_OEM_COMMA,VK_OEM_PERIOD,VK_OEM_MINUS,
};

static const WORD main_key_vkey_qwertz[MAIN_KEY_LEN] =
{
    /* NOTE: this layout must concur with the scan codes layout above */
    VK_OEM_3,'1','2','3','4','5','6','7','8','9','0',VK_OEM_MINUS,VK_OEM_PLUS,0,
    'Q','W','E','R','T','Z','U','I','O','P',VK_OEM_4,VK_OEM_6,
    'A','S','D','F','G','H','J','K','L',VK_OEM_1,VK_OEM_7,VK_OEM_5,
    VK_OEM_102,'Y','X','C','V','B','N','M',VK_OEM_COMMA,VK_OEM_PERIOD,VK_OEM_2,
};

static const WORD main_key_vkey_abnt_qwerty[MAIN_KEY_LEN] =
{
    /* NOTE: this layout must concur with the scan codes layout above */
    VK_OEM_3,'1','2','3','4','5','6','7','8','9','0',VK_OEM_MINUS,VK_OEM_PLUS,0,
    'Q','W','E','R','T','Y','U','I','O','P',VK_OEM_4,VK_OEM_6,
    'A','S','D','F','G','H','J','K','L',VK_OEM_1,VK_OEM_8,VK_OEM_5,
    VK_OEM_7,'Z','X','C','V','B','N','M',VK_OEM_COMMA,VK_OEM_PERIOD,VK_OEM_2,VK_OEM_102,
};

static const WORD main_key_vkey_colemak[MAIN_KEY_LEN] =
{
    /* NOTE: this layout must concur with the scan codes layout above */
    VK_OEM_3,'1','2','3','4','5','6','7','8','9','0',VK_OEM_MINUS,VK_OEM_PLUS,0,
    'Q','W','F','P','G','J','L','U','Y',VK_OEM_1,VK_OEM_4,VK_OEM_6,
    'A','R','S','T','D','H','N','E','I','O',VK_OEM_7,VK_OEM_5,
    VK_OEM_102,'Z','X','C','V','B','K','M',VK_OEM_COMMA,VK_OEM_PERIOD,VK_OEM_2,
};

static const WORD main_key_vkey_azerty[MAIN_KEY_LEN] =
{
    /* NOTE: this layout must concur with the scan codes layout above */
    VK_OEM_7,'1','2','3','4','5','6','7','8','9','0',VK_OEM_4,VK_OEM_PLUS,0,
    'A','Z','E','R','T','Y','U','I','O','P',VK_OEM_6,VK_OEM_1,
    'Q','S','D','F','G','H','J','K','L','M',VK_OEM_3,VK_OEM_5,
    VK_OEM_102,'W','X','C','V','B','N',VK_OEM_COMMA,VK_OEM_PERIOD,VK_OEM_2,VK_OEM_8,
};

static const WORD main_key_vkey_dvorak[MAIN_KEY_LEN] =
{
    /* NOTE: this layout must concur with the scan codes layout above */
    VK_OEM_3,'1','2','3','4','5','6','7','8','9','0',VK_OEM_4,VK_OEM_6,0,
    VK_OEM_7,VK_OEM_COMMA,VK_OEM_PERIOD,'P','Y','F','G','C','R','L',VK_OEM_2,VK_OEM_PLUS,
    'A','O','E','U','I','D','H','T','N','S',VK_OEM_MINUS,VK_OEM_5,
    VK_OEM_102,VK_OEM_1,'Q','J','K','X','B','M','W','V','Z',
};

#define K(x) XKB_KEY_##x

static const xkb_keysym_t main_key_symbols_be[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
   {K(twosuperior), K(threesuperior)}, {'&', '1'}, {K(eacute), '2'}, {'"', '3'}, {'\'', '4'}, {'(', '5'}, {K(section), '6'}, {K(egrave), '7'}, {'!', '8'}, {K(ccedilla), '9'}, {K(agrave), '0'}, {')', K(degree)}, {'-', '_'}, {},
    /* Row D: AD01-AD12 */
   {'a', 'A'}, {'z', 'Z'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(dead_circumflex), K(dead_diaeresis)}, {'$', '*'},
    /* Row C: AC01-AC12 */
   {'q', 'Q'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {'m', 'M'}, {K(ugrave), '%'}, {K(mu), K(sterling)},
    /* Row B: LSGT, AB01-AB11 */
   {'<', '>'}, {'w', 'W'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {',', '?'}, {';', '.'}, {':', '/'}, {'=', '+'}, {},
};

static const xkb_keysym_t main_key_symbols_bg_bds[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {'(', ')'}, {'1', '!'}, {'2', '?'}, {'3', '+'}, {'4', '"'}, {'5', '%'}, {'6', '='}, {'7', ':'}, {'8', '/'}, {'9', K(endash)}, {'0', K(numerosign)}, {'-', '$'}, {'.', K(EuroSign)}, {},
    /* Row D: AD01-AD12 */
    {',', K(Cyrillic_yeru)}, {K(Cyrillic_u), K(Cyrillic_U)}, {K(Cyrillic_ie), K(Cyrillic_IE)}, {K(Cyrillic_i), K(Cyrillic_I)}, {K(Cyrillic_sha), K(Cyrillic_SHA)}, {K(Cyrillic_shcha), K(Cyrillic_SHCHA)}, {K(Cyrillic_ka), K(Cyrillic_KA)}, {K(Cyrillic_es), K(Cyrillic_ES)}, {K(Cyrillic_de), K(Cyrillic_DE)}, {K(Cyrillic_ze), K(Cyrillic_ZE)}, {K(Cyrillic_tse), K(Cyrillic_TSE)}, {';', K(section)},
    /* Row C: AC01-AC12 */
    {K(Cyrillic_softsign), 0x100045d}, {K(Cyrillic_ya), K(Cyrillic_YA)}, {K(Cyrillic_a), K(Cyrillic_A)}, {K(Cyrillic_o), K(Cyrillic_O)}, {K(Cyrillic_zhe), K(Cyrillic_ZHE)}, {K(Cyrillic_ghe), K(Cyrillic_GHE)}, {K(Cyrillic_te), K(Cyrillic_TE)}, {K(Cyrillic_en), K(Cyrillic_EN)}, {K(Cyrillic_ve), K(Cyrillic_VE)}, {K(Cyrillic_em), K(Cyrillic_EM)}, {K(Cyrillic_che), K(Cyrillic_CHE)}, {K(doublelowquotemark), K(leftdoublequotemark)},
    /* Row B: LSGT, AB01-AB11 */
    {0x100045d, 0x100040d}, {K(Cyrillic_yu), K(Cyrillic_YU)}, {K(Cyrillic_shorti), K(Cyrillic_SHORTI)}, {K(Cyrillic_hardsign), K(Cyrillic_HARDSIGN)}, {K(Cyrillic_e), K(Cyrillic_E)}, {K(Cyrillic_ef), K(Cyrillic_EF)}, {K(Cyrillic_ha), K(Cyrillic_HA)}, {K(Cyrillic_pe), K(Cyrillic_PE)}, {K(Cyrillic_er), K(Cyrillic_ER)}, {K(Cyrillic_el), K(Cyrillic_EL)}, {K(Cyrillic_be), K(Cyrillic_BE)}, {},
};

static const xkb_keysym_t main_key_symbols_bg_phonetic[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {K(Cyrillic_che), K(Cyrillic_CHE)}, {'1', '!'}, {'2', '@'}, {'3', K(numerosign)}, {'4', '$'}, {'5', '%'}, {'6', K(EuroSign)}, {'7', K(section)}, {'8', '*'}, {'9', '('}, {'0', ')'}, {'-', K(endash)}, {'=', '+'}, {},
    /* Row D: AD01-AD12 */
    {K(Cyrillic_ya), K(Cyrillic_YA)}, {K(Cyrillic_ve), K(Cyrillic_VE)}, {K(Cyrillic_ie), K(Cyrillic_IE)}, {K(Cyrillic_er), K(Cyrillic_ER)}, {K(Cyrillic_te), K(Cyrillic_TE)}, {K(Cyrillic_hardsign), K(Cyrillic_HARDSIGN)}, {K(Cyrillic_u), K(Cyrillic_U)}, {K(Cyrillic_i), K(Cyrillic_I)}, {K(Cyrillic_o), K(Cyrillic_O)}, {K(Cyrillic_pe), K(Cyrillic_PE)}, {K(Cyrillic_sha), K(Cyrillic_SHA)}, {K(Cyrillic_shcha), K(Cyrillic_SHCHA)},
    /* Row C: AC01-AC12 */
    {K(Cyrillic_a), K(Cyrillic_A)}, {K(Cyrillic_es), K(Cyrillic_ES)}, {K(Cyrillic_de), K(Cyrillic_DE)}, {K(Cyrillic_ef), K(Cyrillic_EF)}, {K(Cyrillic_ghe), K(Cyrillic_GHE)}, {K(Cyrillic_ha), K(Cyrillic_HA)}, {K(Cyrillic_shorti), K(Cyrillic_SHORTI)}, {K(Cyrillic_ka), K(Cyrillic_KA)}, {K(Cyrillic_el), K(Cyrillic_EL)}, {';', ':'}, {'\'', '"'}, {K(Cyrillic_yu), K(Cyrillic_YU)},
    /* Row B: LSGT, AB01-AB11 */
    {0x100045d, 0x100040d}, {K(Cyrillic_ze), K(Cyrillic_ZE)}, {K(Cyrillic_softsign), 0x100045d}, {K(Cyrillic_tse), K(Cyrillic_TSE)}, {K(Cyrillic_zhe), K(Cyrillic_ZHE)}, {K(Cyrillic_be), K(Cyrillic_BE)}, {K(Cyrillic_en), K(Cyrillic_EN)}, {K(Cyrillic_em), K(Cyrillic_EM)}, {',', K(doublelowquotemark)}, {'.', K(leftdoublequotemark)}, {'/', '?'}, {},
};

static const xkb_keysym_t main_key_symbols_br_abnt2[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {'\'', '"'}, {'1', '!'}, {'2', '@'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', K(dead_diaeresis)}, {'7', '&'}, {'8', '*'}, {'9', '('}, {'0', ')'}, {'-', '_'}, {'=', '+'}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(dead_acute), K(dead_grave)}, {'[', '{'},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {K(ccedilla), K(Ccedilla)}, {K(dead_tilde), K(dead_circumflex)}, {']', '}'},
    /* Row B: LSGT, AB01-AB11 */
   {'\\', '|'}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', '<'}, {'.', '>'}, {';', ':'}, {'/', '?'},
};

static const xkb_keysym_t main_key_symbols_by[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {K(Cyrillic_io), K(Cyrillic_IO)}, {'1', '!'}, {'2', '"'}, {'3', K(numerosign)}, {'4', ';'}, {'5', '%'}, {'6', ':'}, {'7', '?'}, {'8', '*'}, {'9', '('}, {'0', ')'}, {'-', '_'}, {'=', '+'}, {},
    /* Row D: AD01-AD12 */
    {K(Cyrillic_shorti), K(Cyrillic_SHORTI)}, {K(Cyrillic_tse), K(Cyrillic_TSE)}, {K(Cyrillic_u), K(Cyrillic_U)}, {K(Cyrillic_ka), K(Cyrillic_KA)}, {K(Cyrillic_ie), K(Cyrillic_IE)}, {K(Cyrillic_en), K(Cyrillic_EN)}, {K(Cyrillic_ghe), K(Cyrillic_GHE)}, {K(Cyrillic_sha), K(Cyrillic_SHA)}, {K(Byelorussian_shortu), K(Byelorussian_SHORTU)}, {K(Cyrillic_ze), K(Cyrillic_ZE)}, {K(Cyrillic_ha), K(Cyrillic_HA)}, {'\'', '\''},
    /* Row C: AC01-AC12 */
    {K(Cyrillic_ef), K(Cyrillic_EF)}, {K(Cyrillic_yeru), K(Cyrillic_YERU)}, {K(Cyrillic_ve), K(Cyrillic_VE)}, {K(Cyrillic_a), K(Cyrillic_A)}, {K(Cyrillic_pe), K(Cyrillic_PE)}, {K(Cyrillic_er), K(Cyrillic_ER)}, {K(Cyrillic_o), K(Cyrillic_O)}, {K(Cyrillic_el), K(Cyrillic_EL)}, {K(Cyrillic_de), K(Cyrillic_DE)}, {K(Cyrillic_zhe), K(Cyrillic_ZHE)}, {K(Cyrillic_e), K(Cyrillic_E)}, {'\\', '/'},
    /* Row B: LSGT, AB01-AB11 */
    {'/', '|'}, {K(Cyrillic_ya), K(Cyrillic_YA)}, {K(Cyrillic_che), K(Cyrillic_CHE)}, {K(Cyrillic_es), K(Cyrillic_ES)}, {K(Cyrillic_em), K(Cyrillic_EM)}, {K(Ukrainian_i), K(Ukrainian_I)}, {K(Cyrillic_te), K(Cyrillic_TE)}, {K(Cyrillic_softsign), K(Cyrillic_SOFTSIGN)}, {K(Cyrillic_be), K(Cyrillic_BE)}, {K(Cyrillic_yu), K(Cyrillic_YU)}, {'.', ','}, {},
};

static const xkb_keysym_t main_key_symbols_ca[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {'#', '|'}, {'1', '!'}, {'2', '"'}, {'3', '/'}, {'4', '$'}, {'5', '%'}, {'6', '?'}, {'7', '&'}, {'8', '*'}, {'9', '('}, {'0', ')'}, {'-', '_'}, {'=', '+'}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(dead_circumflex), K(dead_circumflex)}, {K(dead_cedilla), K(dead_diaeresis)},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {';', ':'}, {K(dead_grave), K(dead_grave)}, {'<', '>'},
    /* Row B: LSGT, AB01-AB11 */
    {K(guillemotleft), K(guillemotright)}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', '\''}, {'.', '.'}, {K(eacute), K(Eacute)}, {},
};

static const xkb_keysym_t main_key_symbols_ch_de[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {K(section), K(degree)}, {'1', '+'}, {'2', '"'}, {'3', '*'}, {'4', K(ccedilla)}, {'5', '%'}, {'6', '&'}, {'7', '/'}, {'8', '('}, {'9', ')'}, {'0', '='}, {'\'', '?'}, {K(dead_circumflex), K(dead_grave)}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'z', 'Z'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(udiaeresis), K(egrave)}, {K(dead_diaeresis), '!'},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {K(odiaeresis), K(eacute)}, {K(adiaeresis), K(agrave)}, {'$', K(sterling)},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {'y', 'Y'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', ';'}, {'.', ':'}, {'-', '_'}, {},
};

static const xkb_keysym_t main_key_symbols_ch_fr[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {K(section), K(degree)}, {'1', '+'}, {'2', '"'}, {'3', '*'}, {'4', K(ccedilla)}, {'5', '%'}, {'6', '&'}, {'7', '/'}, {'8', '('}, {'9', ')'}, {'0', '='}, {'\'', '?'}, {K(dead_circumflex), K(dead_grave)}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'z', 'Z'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(egrave), K(udiaeresis)}, {K(dead_diaeresis), '!'},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {K(eacute), K(odiaeresis)}, {K(agrave), K(adiaeresis)}, {'$', K(sterling)},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {'y', 'Y'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', ';'}, {'.', ':'}, {'-', '_'}, {},
};

static const xkb_keysym_t main_key_symbols_cz[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {';', K(dead_abovering)}, {'+', '1'}, {K(ecaron), '2'}, {K(scaron), '3'}, {K(ccaron), '4'}, {K(rcaron), '5'}, {K(zcaron), '6'}, {K(yacute), '7'}, {K(aacute), '8'}, {K(iacute), '9'}, {K(eacute), '0'}, {'=', '%'}, {K(dead_acute), K(dead_caron)}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'z', 'Z'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(uacute), '/'}, {')', '('},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {K(uring), '"'}, {K(section), '!'}, {K(dead_diaeresis), '\''},
    /* Row B: LSGT, AB01-AB11 */
    {'\\', '|'}, {'y', 'Y'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', '?'}, {'.', ':'}, {'-', '_'}, {},
};

static const xkb_keysym_t main_key_symbols_cz_qwerty[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {';', K(dead_abovering)}, {'+', '1'}, {K(ecaron), '2'}, {K(scaron), '3'}, {K(ccaron), '4'}, {K(rcaron), '5'}, {K(zcaron), '6'}, {K(yacute), '7'}, {K(aacute), '8'}, {K(iacute), '9'}, {K(eacute), '0'}, {'=', '%'}, {K(dead_acute), K(dead_caron)}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(uacute), '/'}, {')', '('},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {K(uring), '"'}, {K(section), '!'}, {K(dead_diaeresis), '\''},
    /* Row B: LSGT, AB01-AB11 */
    {'\\', '|'}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', '?'}, {'.', ':'}, {'-', '_'}, {},
};

static const xkb_keysym_t main_key_symbols_de[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {K(dead_circumflex), K(degree)}, {'1', '!'}, {'2', '"'}, {'3', K(section)}, {'4', '$'}, {'5', '%'}, {'6', '&'}, {'7', '/'}, {'8', '('}, {'9', ')'}, {'0', '='}, {K(ssharp), '?'}, {K(dead_acute), K(dead_grave)}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'z', 'Z'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(udiaeresis), K(Udiaeresis)}, {'+', '*'},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {K(odiaeresis), K(Odiaeresis)}, {K(adiaeresis), K(Adiaeresis)}, {'#', '\''},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {'y', 'Y'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', ';'}, {'.', ':'}, {'-', '_'}, {},
};

static const xkb_keysym_t main_key_symbols_dk[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {K(onehalf), K(section)}, {'1', '!'}, {'2', '"'}, {'3', '#'}, {'4', K(currency)}, {'5', '%'}, {'6', '&'}, {'7', '/'}, {'8', '('}, {'9', ')'}, {'0', '='}, {'+', '?'}, {K(dead_acute), K(dead_grave)}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(aring), K(Aring)}, {K(dead_diaeresis), K(dead_circumflex)},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {K(ae), K(AE)}, {K(oslash), K(Oslash)}, {'\'', '*'},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', ';'}, {'.', ':'}, {'-', '_'}, {},
};

static const xkb_keysym_t main_key_symbols_ee[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {K(dead_caron), K(dead_tilde)}, {'1', '!'}, {'2', '"'}, {'3', '#'}, {'4', K(currency)}, {'5', '%'}, {'6', '&'}, {'7', '/'}, {'8', '('}, {'9', ')'}, {'0', '='}, {'+', '?'}, {K(dead_acute), K(dead_grave)}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(udiaeresis), K(Udiaeresis)}, {K(otilde), K(Otilde)},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {K(odiaeresis), K(Odiaeresis)}, {K(adiaeresis), K(Adiaeresis)}, {'\'', '*'},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', ';'}, {'.', ':'}, {'-', '_'}, {},
};

static const xkb_keysym_t main_key_symbols_es[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {K(masculine), K(ordfeminine)}, {'1', '!'}, {'2', '"'}, {'3', K(periodcentered)}, {'4', '$'}, {'5', '%'}, {'6', '&'}, {'7', '/'}, {'8', '('}, {'9', ')'}, {'0', '='}, {'\'', '?'}, {K(exclamdown), K(questiondown)}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(dead_grave), K(dead_circumflex)}, {'+', '*'},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {K(ntilde), K(Ntilde)}, {K(dead_acute), K(dead_diaeresis)}, {K(ccedilla), K(Ccedilla)},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', ';'}, {'.', ':'}, {'-', '_'}, {},
};

static const xkb_keysym_t main_key_symbols_fi[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {K(section), K(onehalf)}, {'1', '!'}, {'2', '"'}, {'3', '#'}, {'4', K(currency)}, {'5', '%'}, {'6', '&'}, {'7', '/'}, {'8', '('}, {'9', ')'}, {'0', '='}, {'+', '?'}, {K(dead_acute), K(dead_grave)}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(aring), K(Aring)}, {K(dead_diaeresis), K(dead_circumflex)},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {K(odiaeresis), K(Odiaeresis)}, {K(adiaeresis), K(Adiaeresis)}, {'\'', '*'},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', ';'}, {'.', ':'}, {'-', '_'}, {},
};

static const xkb_keysym_t main_key_symbols_fr[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {K(twosuperior), '~'}, {'&', '1'}, {K(eacute), '2'}, {'"', '3'}, {'\'', '4'}, {'(', '5'}, {'-', '6'}, {K(egrave), '7'}, {'_', '8'}, {K(ccedilla), '9'}, {K(agrave), '0'}, {')', K(degree)}, {'=', '+'}, {},
    /* Row D: AD01-AD12 */
    {'a', 'A'}, {'z', 'Z'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(dead_circumflex), K(dead_diaeresis)}, {'$', K(sterling)},
    /* Row C: AC01-AC12 */
    {'q', 'Q'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {'m', 'M'}, {K(ugrave), '%'}, {'*', K(mu)},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {'w', 'W'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {',', '?'}, {';', '.'}, {':', '/'}, {'!', K(section)}, {},
};

static const xkb_keysym_t main_key_symbols_gb[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {'`', K(notsign)}, {'1', '!'}, {'2', '"'}, {'3', K(sterling)}, {'4', '$'}, {'5', '%'}, {'6', '^'}, {'7', '&'}, {'8', '*'}, {'9', '('}, {'0', ')'}, {'-', '_'}, {'=', '+'}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {'[', '{'}, {']', '}'},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {';', ':'}, {'\'', '@'}, {'#', '~'},
    /* Row B: LSGT, AB01-AB11 */
    {'\\', '|'}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', '<'}, {'.', '>'}, {'/', '?'}, {},
};

static const xkb_keysym_t main_key_symbols_gr[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {'`', '~'}, {'1', '!'}, {'2', '@'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', '^'}, {'7', '&'}, {'8', '*'}, {'9', '('}, {'0', ')'}, {'-', '_'}, {'=', '+'}, {},
    /* Row D: AD01-AD12 */
    {';', ':'}, {K(Greek_finalsmallsigma), K(Greek_SIGMA)}, {K(Greek_epsilon), K(Greek_EPSILON)}, {K(Greek_rho), K(Greek_RHO)}, {K(Greek_tau), K(Greek_TAU)}, {K(Greek_upsilon), K(Greek_UPSILON)}, {K(Greek_theta), K(Greek_THETA)}, {K(Greek_iota), K(Greek_IOTA)}, {K(Greek_omicron), K(Greek_OMICRON)}, {K(Greek_pi), K(Greek_PI)}, {'[', '{'}, {']', '}'},
    /* Row C: AC01-AC12 */
    {K(Greek_alpha), K(Greek_ALPHA)}, {K(Greek_sigma), K(Greek_SIGMA)}, {K(Greek_delta), K(Greek_DELTA)}, {K(Greek_phi), K(Greek_PHI)}, {K(Greek_gamma), K(Greek_GAMMA)}, {K(Greek_eta), K(Greek_ETA)}, {K(Greek_xi), K(Greek_XI)}, {K(Greek_kappa), K(Greek_KAPPA)}, {K(Greek_lamda), K(Greek_LAMDA)}, {K(dead_acute), K(dead_diaeresis)}, {'\'', '"'}, {'\\', '|'},
    /* Row B: LSGT, AB01-AB11 */
    {K(guillemotleft), K(guillemotright)}, {K(Greek_zeta), K(Greek_ZETA)}, {K(Greek_chi), K(Greek_CHI)}, {K(Greek_psi), K(Greek_PSI)}, {K(Greek_omega), K(Greek_OMEGA)}, {K(Greek_beta), K(Greek_BETA)}, {K(Greek_nu), K(Greek_NU)}, {K(Greek_mu), K(Greek_MU)}, {',', '<'}, {'.', '>'}, {'/', '?'}, {},
};

static const xkb_keysym_t main_key_symbols_hr[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {'`', '~'}, {'1', '!'}, {'2', '"'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', '&'}, {'7', '/'}, {'8', '('}, {'9', ')'}, {'0', '='}, {'\'', '?'}, {'+', '*'}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'z', 'Z'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(scaron), K(Scaron)}, {K(dstroke), K(Dstroke)},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {K(ccaron), K(Ccaron)}, {K(cacute), K(Cacute)}, {K(zcaron), K(Zcaron)},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {'y', 'Y'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', ';'}, {'.', ':'}, {'-', '_'}, {},
};

static const xkb_keysym_t main_key_symbols_hu[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {'0', K(section)}, {'1', '\''}, {'2', '"'}, {'3', '+'}, {'4', '!'}, {'5', '%'}, {'6', '/'}, {'7', '='}, {'8', '('}, {'9', ')'}, {K(odiaeresis), K(Odiaeresis)}, {K(udiaeresis), K(Udiaeresis)}, {K(oacute), K(Oacute)}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'z', 'Z'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(odoubleacute), K(Odoubleacute)}, {K(uacute), K(Uacute)},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {K(eacute), K(Eacute)}, {K(aacute), K(Aacute)}, {K(udoubleacute), K(Udoubleacute)},
    /* Row B: LSGT, AB01-AB11 */
    {K(iacute), K(Iacute)}, {'y', 'Y'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', '?'}, {'.', ':'}, {'-', '_'}, {},
};

static const xkb_keysym_t main_key_symbols_il[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {';', '~'}, {'1', '!'}, {'2', '@'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', '^'}, {'7', '&'}, {'8', '*'}, {'9', ')'}, {'0', '('}, {'-', '_'}, {'=', '+'}, {},
    /* Row D: AD01-AD12 */
    {'/', 'Q'}, {'\'', 'W'}, {K(hebrew_qoph), 'E'}, {K(hebrew_resh), 'R'}, {K(hebrew_aleph), 'T'}, {K(hebrew_tet), 'Y'}, {K(hebrew_waw), 'U'}, {K(hebrew_finalnun), 'I'}, {K(hebrew_finalmem), 'O'}, {K(hebrew_pe), 'P'}, {']', '}'}, {'[', '{'},
    /* Row C: AC01-AC12 */
    {K(hebrew_shin), 'A'}, {K(hebrew_dalet), 'S'}, {K(hebrew_gimel), 'D'}, {K(hebrew_kaph), 'F'}, {K(hebrew_ayin), 'G'}, {K(hebrew_yod), 'H'}, {K(hebrew_chet), 'J'}, {K(hebrew_lamed), 'K'}, {K(hebrew_finalkaph), 'L'}, {K(hebrew_finalpe), ':'}, {',', '"'}, {'\\', '|'},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {K(hebrew_zain), 'Z'}, {K(hebrew_samech), 'X'}, {K(hebrew_bet), 'C'}, {K(hebrew_he), 'V'}, {K(hebrew_nun), 'B'}, {K(hebrew_mem), 'N'}, {K(hebrew_zade), 'M'}, {K(hebrew_taw), '>'}, {K(hebrew_finalzade), '<'}, {'.', '?'}, {},
};

static const xkb_keysym_t main_key_symbols_il_phonetic[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {'=', '+'}, {},
    /* Row D: AD01-AD12 */
    {K(hebrew_qoph), K(hebrew_qoph)}, {K(hebrew_waw), K(hebrew_waw)}, {K(hebrew_aleph), K(hebrew_aleph)}, {K(hebrew_resh), K(hebrew_resh)}, {K(hebrew_taw), K(hebrew_tet)}, {K(hebrew_ayin), K(hebrew_ayin)}, {K(hebrew_waw), K(hebrew_waw)}, {K(hebrew_yod), K(hebrew_yod)}, {K(hebrew_samech), K(hebrew_samech)}, {K(hebrew_pe), K(hebrew_finalpe)}, {}, {},
    /* Row C: AC01-AC12 */
    {K(hebrew_aleph), K(hebrew_aleph)}, {K(hebrew_shin), K(hebrew_shin)}, {K(hebrew_dalet), K(hebrew_dalet)}, {K(hebrew_pe), K(hebrew_finalpe)}, {K(hebrew_gimel), K(hebrew_gimel)}, {K(hebrew_he), K(hebrew_he)}, {K(hebrew_yod), K(hebrew_yod)}, {K(hebrew_kaph), K(hebrew_finalkaph)}, {K(hebrew_lamed), K(hebrew_lamed)}, {}, {}, {'\\', '|'},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {K(hebrew_zain), K(hebrew_zain)}, {K(hebrew_chet), K(hebrew_chet)}, {K(hebrew_zade), K(hebrew_finalzade)}, {K(hebrew_waw), K(hebrew_waw)}, {K(hebrew_bet), K(hebrew_bet)}, {K(hebrew_nun), K(hebrew_finalnun)}, {K(hebrew_mem), K(hebrew_finalmem)}, {}, {}, {}, {},
};

static const xkb_keysym_t main_key_symbols_is[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {K(dead_abovering), K(dead_diaeresis)}, {'1', '!'}, {'2', '"'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', '&'}, {'7', '/'}, {'8', '('}, {'9', ')'}, {'0', '='}, {K(odiaeresis), K(Odiaeresis)}, {'-', '_'}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(eth), K(ETH)}, {'\'', '?'},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {K(ae), K(AE)}, {K(dead_acute), K(dead_acute)}, {'+', '*'},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', ';'}, {'.', ':'}, {K(thorn), K(THORN)}, {},
};

static const xkb_keysym_t main_key_symbols_it[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {'\\', '|'}, {'1', '!'}, {'2', '"'}, {'3', K(sterling)}, {'4', '$'}, {'5', '%'}, {'6', '&'}, {'7', '/'}, {'8', '('}, {'9', ')'}, {'0', '='}, {'\'', '?'}, {K(igrave), '^'}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(egrave), K(eacute)}, {'+', '*'},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {K(ograve), K(ccedilla)}, {K(agrave), K(degree)}, {K(ugrave), K(section)},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', ';'}, {'.', ':'}, {'-', '_'}, {},
};

static const xkb_keysym_t main_key_symbols_jp_106[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {K(Zenkaku_Hankaku), K(Kanji)}, {'1', '!'}, {'2', '"'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', '&'}, {'7', '\''}, {'8', '('}, {'9', ')'}, {'0', '~'}, {'-', '='}, {'^', '~'}, {'\\', '|'},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {'@', '`'}, {'[', '{'},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {';', '+'}, {':', '*'}, {']', '}'},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', '<'}, {'.', '>'}, {'/', '?'}, {'\\', '_'},
};

static const xkb_keysym_t main_key_symbols_jp_kana86[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {K(Zenkaku_Hankaku), K(Kanji)}, {'1', '!'}, {'2', '"'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', '&'}, {'7', '\''}, {'8', '('}, {'9', ')'}, {'0', K(kana_WO)}, {'-', '='}, {'^', '~'}, {K(yen), '|'},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {'@', '`'}, {'[', '{'},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {';', '+'}, {':', '*'}, {']', '}'},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', '<'}, {'.', '>'}, {'/', '?'}, {'\\', '_'},
};

static const xkb_keysym_t main_key_symbols_jp_mac[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {K(Zenkaku_Hankaku), K(Kanji)}, {K(kana_NU)}, {K(kana_FU)}, {K(kana_A), K(kana_a)}, {K(kana_U), K(kana_u)}, {K(kana_E), K(kana_e)}, {K(kana_O), K(kana_o)}, {K(kana_YA), K(kana_ya)}, {K(kana_YU), K(kana_yu)}, {K(kana_YO), K(kana_yo)}, {K(kana_WA), K(kana_WO)}, {K(kana_HO)}, {K(kana_HE)}, {K(prolongedsound)},
    /* Row D: AD01-AD12 */
    {K(kana_TA)}, {K(kana_TE)}, {K(kana_I), K(kana_i)}, {K(kana_SU)}, {K(kana_KA)}, {K(kana_N)}, {K(kana_NA)}, {K(kana_NI)}, {K(kana_RA)}, {K(kana_SE)}, {K(voicedsound)}, {K(semivoicedsound), K(kana_openingbracket)},
    /* Row C: AC01-AC12 */
    {K(kana_CHI)}, {K(kana_TO)}, {K(kana_SHI)}, {K(kana_HA)}, {K(kana_KI)}, {K(kana_KU)}, {K(kana_MA)}, {K(kana_NO)}, {K(kana_RI)}, {K(kana_RE)}, {K(kana_KE)}, {K(kana_MU), K(kana_closingbracket)},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {K(kana_TSU), K(kana_tsu)}, {K(kana_SA)}, {K(kana_SO)}, {K(kana_HI)}, {K(kana_KO)}, {K(kana_MI)}, {K(kana_MO)}, {K(kana_NE), K(kana_comma)}, {K(kana_RU), K(kana_fullstop)}, {K(kana_ME), K(kana_conjunctive)}, {K(kana_RO)},
};

static const xkb_keysym_t main_key_symbols_lt[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {'`', '~'}, {K(aogonek), K(Aogonek)}, {K(ccaron), K(Ccaron)}, {K(eogonek), K(Eogonek)}, {K(eabovedot), K(Eabovedot)}, {K(iogonek), K(Iogonek)}, {K(scaron), K(Scaron)}, {K(uogonek), K(Uogonek)}, {K(umacron), K(Umacron)}, {K(doublelowquotemark), '('}, {K(leftdoublequotemark), ')'}, {'-', '_'}, {K(zcaron), K(Zcaron)}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {'[', '{'}, {']', '}'},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {';', ':'}, {'\'', '"'}, {'\\', '|'},
    /* Row B: LSGT, AB01-AB11 */
    {K(endash), K(EuroSign)}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', '<'}, {'.', '>'}, {'/', '?'}, {},
};

static const xkb_keysym_t main_key_symbols_nl[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {'@', K(section)}, {'1', '!'}, {'2', '"'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', '&'}, {'7', '_'}, {'8', '('}, {'9', ')'}, {'0', '\''}, {'/', '?'}, {K(degree), K(dead_tilde)}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(dead_diaeresis), K(dead_circumflex)}, {'*', '|'},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {'+', K(plusminus)}, {K(dead_acute), K(dead_grave)}, {'<', '>'},
    /* Row B: LSGT, AB01-AB11 */
    {']', '['}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', ';'}, {'.', ':'}, {'-', '='}, {},
};

static const xkb_keysym_t main_key_symbols_no[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {'|', K(section)}, {'1', '!'}, {'2', '"'}, {'3', '#'}, {'4', K(currency)}, {'5', '%'}, {'6', '&'}, {'7', '/'}, {'8', '('}, {'9', ')'}, {'0', '='}, {'+', '?'}, {'\\', K(dead_grave)}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(aring), K(Aring)}, {K(dead_diaeresis), K(dead_circumflex)},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {K(oslash), K(Oslash)}, {K(ae), K(AE)}, {'\'', '*'},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', ';'}, {'.', ':'}, {'-', '_'}, {},
};

static const xkb_keysym_t main_key_symbols_pl[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
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

static const xkb_keysym_t main_key_symbols_pl_dvp[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {'$', '~'}, {'&', '%'}, {'[', '7'}, {'{', '5'}, {'}', '3'}, {'(', '1'}, {'=', '9'}, {'*', '0'}, {')', '2'}, {'+', '4'}, {']', '6'}, {'!', '8'}, {'#', '`'}, {},
    /* Row D: AD01-AD12 */
    {';', ':'}, {',', '<'}, {'.', '>'}, {'p', 'P'}, {'y', 'Y'}, {'f', 'F'}, {'g', 'G'}, {'c', 'C'}, {'r', 'R'}, {'l', 'L'}, {'/', '?'}, {'@', '^'},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'o', 'O'}, {'e', 'E'}, {'u', 'U'}, {'i', 'I'}, {'d', 'D'}, {'h', 'H'}, {'t', 'T'}, {'n', 'N'}, {'s', 'S'}, {'-', '_'}, {'\\', '|'},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {'\'', '"'}, {'q', 'Q'}, {'j', 'J'}, {'k', 'K'}, {'x', 'X'}, {'b', 'B'}, {'m', 'M'}, {'w', 'W'}, {'v', 'V'}, {'z', 'Z'}, {},
};

static const xkb_keysym_t main_key_symbols_pt[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {'\\', '|'}, {'1', '!'}, {'2', '"'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', '&'}, {'7', '/'}, {'8', '('}, {'9', ')'}, {'0', '='}, {'\'', '?'}, {K(guillemotleft), K(guillemotright)}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {'+', '*'}, {K(dead_acute), K(dead_grave)},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {K(ccedilla), K(Ccedilla)}, {K(masculine), K(ordfeminine)}, {K(dead_tilde), K(dead_circumflex)},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', ';'}, {'.', ':'}, {'-', '_'}, {},
};

static const xkb_keysym_t main_key_symbols_ru[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {K(Cyrillic_io), K(Cyrillic_IO)}, {'1', '!'}, {'2', '"'}, {'3', K(numerosign)}, {'4', ';'}, {'5', '%'}, {'6', ':'}, {'7', '?'}, {'8', '*'}, {'9', '('}, {'0', ')'}, {'-', '_'}, {'=', '+'}, {},
    /* Row D: AD01-AD12 */
    {K(Cyrillic_shorti), K(Cyrillic_SHORTI)}, {K(Cyrillic_tse), K(Cyrillic_TSE)}, {K(Cyrillic_u), K(Cyrillic_U)}, {K(Cyrillic_ka), K(Cyrillic_KA)}, {K(Cyrillic_ie), K(Cyrillic_IE)}, {K(Cyrillic_en), K(Cyrillic_EN)}, {K(Cyrillic_ghe), K(Cyrillic_GHE)}, {K(Cyrillic_sha), K(Cyrillic_SHA)}, {K(Cyrillic_shcha), K(Cyrillic_SHCHA)}, {K(Cyrillic_ze), K(Cyrillic_ZE)}, {K(Cyrillic_ha), K(Cyrillic_HA)}, {K(Cyrillic_hardsign), K(Cyrillic_HARDSIGN)},
    /* Row C: AC01-AC12 */
    {K(Cyrillic_ef), K(Cyrillic_EF)}, {K(Cyrillic_yeru), K(Cyrillic_YERU)}, {K(Cyrillic_ve), K(Cyrillic_VE)}, {K(Cyrillic_a), K(Cyrillic_A)}, {K(Cyrillic_pe), K(Cyrillic_PE)}, {K(Cyrillic_er), K(Cyrillic_ER)}, {K(Cyrillic_o), K(Cyrillic_O)}, {K(Cyrillic_el), K(Cyrillic_EL)}, {K(Cyrillic_de), K(Cyrillic_DE)}, {K(Cyrillic_zhe), K(Cyrillic_ZHE)}, {K(Cyrillic_e), K(Cyrillic_E)}, {'\\', '/'},
    /* Row B: LSGT, AB01-AB11 */
    {'/', '|'}, {K(Cyrillic_ya), K(Cyrillic_YA)}, {K(Cyrillic_che), K(Cyrillic_CHE)}, {K(Cyrillic_es), K(Cyrillic_ES)}, {K(Cyrillic_em), K(Cyrillic_EM)}, {K(Cyrillic_i), K(Cyrillic_I)}, {K(Cyrillic_te), K(Cyrillic_TE)}, {K(Cyrillic_softsign), K(Cyrillic_SOFTSIGN)}, {K(Cyrillic_be), K(Cyrillic_BE)}, {K(Cyrillic_yu), K(Cyrillic_YU)}, {'.', ','}, {},
};

static const xkb_keysym_t main_key_symbols_ru_phonetic[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {K(Cyrillic_yu), K(Cyrillic_YU)}, {'1', '!'}, {'2', '@'}, {'3', K(Cyrillic_io)}, {'4', K(Cyrillic_IO)}, {'5', K(Cyrillic_hardsign)}, {'6', K(Cyrillic_HARDSIGN)}, {'7', '&'}, {'8', '*'}, {'9', '('}, {'0', ')'}, {'-', '_'}, {K(Cyrillic_che), K(Cyrillic_CHE)}, {},
    /* Row D: AD01-AD12 */
    {K(Cyrillic_ya), K(Cyrillic_YA)}, {K(Cyrillic_ve), K(Cyrillic_VE)}, {K(Cyrillic_ie), K(Cyrillic_IE)}, {K(Cyrillic_er), K(Cyrillic_ER)}, {K(Cyrillic_te), K(Cyrillic_TE)}, {K(Cyrillic_yeru), K(Cyrillic_YERU)}, {K(Cyrillic_u), K(Cyrillic_U)}, {K(Cyrillic_i), K(Cyrillic_I)}, {K(Cyrillic_o), K(Cyrillic_O)}, {K(Cyrillic_pe), K(Cyrillic_PE)}, {K(Cyrillic_sha), K(Cyrillic_SHA)}, {K(Cyrillic_shcha), K(Cyrillic_SHCHA)},
    /* Row C: AC01-AC12 */
    {K(Cyrillic_a), K(Cyrillic_A)}, {K(Cyrillic_es), K(Cyrillic_ES)}, {K(Cyrillic_de), K(Cyrillic_DE)}, {K(Cyrillic_ef), K(Cyrillic_EF)}, {K(Cyrillic_ghe), K(Cyrillic_GHE)}, {K(Cyrillic_ha), K(Cyrillic_HA)}, {K(Cyrillic_shorti), K(Cyrillic_SHORTI)}, {K(Cyrillic_ka), K(Cyrillic_KA)}, {K(Cyrillic_el), K(Cyrillic_EL)}, {';', ':'}, {'\'', '"'}, {K(Cyrillic_e), K(Cyrillic_E)},
    /* Row B: LSGT, AB01-AB11 */
    {'|', K(brokenbar)}, {K(Cyrillic_ze), K(Cyrillic_ZE)}, {K(Cyrillic_softsign), K(Cyrillic_SOFTSIGN)}, {K(Cyrillic_tse), K(Cyrillic_TSE)}, {K(Cyrillic_zhe), K(Cyrillic_ZHE)}, {K(Cyrillic_be), K(Cyrillic_BE)}, {K(Cyrillic_en), K(Cyrillic_EN)}, {K(Cyrillic_em), K(Cyrillic_EM)}, {',', '<'}, {'.', '>'}, {'/', '?'}, {},
};

static const xkb_keysym_t main_key_symbols_rs[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] = {
    /* Row E: TLDE, AE01-AE13 */
    {'`', '~'}, {'1', '!'}, {'2', '"'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', '&'}, {'7', '/'}, {'8', '('}, {'9', ')'}, {'0', '='}, {'\'', '?'}, {'+', '*'}, {},
    /* Row D: AD01-AD12 */
    {K(Cyrillic_lje), K(Cyrillic_LJE)}, {K(Cyrillic_nje), K(Cyrillic_NJE)}, {K(Cyrillic_ie), K(Cyrillic_IE)}, {K(Cyrillic_er), K(Cyrillic_ER)}, {K(Cyrillic_te), K(Cyrillic_TE)}, {K(Cyrillic_ze), K(Cyrillic_ZE)}, {K(Cyrillic_u), K(Cyrillic_U)}, {K(Cyrillic_i), K(Cyrillic_I)}, {K(Cyrillic_o), K(Cyrillic_O)}, {K(Cyrillic_pe), K(Cyrillic_PE)}, {K(Cyrillic_sha), K(Cyrillic_SHA)}, {K(Serbian_dje), K(Serbian_DJE)},
    /* Row C: AC01-AC12 */
    {K(Cyrillic_a), K(Cyrillic_A)}, {K(Cyrillic_es), K(Cyrillic_ES)}, {K(Cyrillic_de), K(Cyrillic_DE)}, {K(Cyrillic_ef), K(Cyrillic_EF)}, {K(Cyrillic_ghe), K(Cyrillic_GHE)}, {K(Cyrillic_ha), K(Cyrillic_HA)}, {K(Cyrillic_je), K(Cyrillic_JE)}, {K(Cyrillic_ka), K(Cyrillic_KA)}, {K(Cyrillic_el), K(Cyrillic_EL)}, {K(Cyrillic_che), K(Cyrillic_CHE)}, {K(Serbian_tshe), K(Serbian_TSHE)}, {K(Cyrillic_zhe), K(Cyrillic_ZHE)},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {K(Cyrillic_zhe), K(Cyrillic_ZHE)}, {K(Cyrillic_dzhe), K(Cyrillic_DZHE)}, {K(Cyrillic_tse), K(Cyrillic_TSE)}, {K(Cyrillic_ve), K(Cyrillic_VE)}, {K(Cyrillic_be), K(Cyrillic_BE)}, {K(Cyrillic_en), K(Cyrillic_EN)}, {K(Cyrillic_em), K(Cyrillic_EM)}, {',', ';'}, {'.', ':'}, {'-', '_'}, {},
};

static const xkb_keysym_t main_key_symbols_se[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {K(section), K(onehalf)}, {'1', '!'}, {'2', '"'}, {'3', '#'}, {'4', K(currency)}, {'5', '%'}, {'6', '&'}, {'7', '/'}, {'8', '('}, {'9', ')'}, {'0', '='}, {'+', '?'}, {K(dead_acute), K(dead_grave)}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(aring), K(Aring)}, {K(dead_diaeresis), K(dead_circumflex)},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {K(odiaeresis), K(Odiaeresis)}, {K(adiaeresis), K(Adiaeresis)}, {'\'', '*'},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', ';'}, {'.', ':'}, {'-', '_'}, {},
};

static const xkb_keysym_t main_key_symbols_si[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {K(cedilla), K(diaeresis)}, {'1', '!'}, {'2', '"'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', '&'}, {'7', '/'}, {'8', '('}, {'9', ')'}, {'0', '='}, {'\'', '?'}, {'+', '*'}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'z', 'Z'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(scaron), K(Scaron)}, {K(dstroke), K(Dstroke)},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {K(ccaron), K(Ccaron)}, {K(cacute), K(Cacute)}, {K(zcaron), K(Zcaron)},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {'y', 'Y'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', ';'}, {'.', ':'}, {'-', '_'}, {},
};

static const xkb_keysym_t main_key_symbols_sk[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {';', K(dead_abovering)}, {'+', '1'}, {K(lcaron), '2'}, {K(scaron), '3'}, {K(ccaron), '4'}, {K(tcaron), '5'}, {K(zcaron), '6'}, {K(yacute), '7'}, {K(aacute), '8'}, {K(iacute), '9'}, {K(eacute), '0'}, {'=', '%'}, {K(dead_acute), K(dead_caron)}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'z', 'Z'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {K(uacute), '/'}, {K(adiaeresis), '('},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {K(ocircumflex), '"'}, {K(section), '!'}, {K(ncaron), ')'},
    /* Row B: LSGT, AB01-AB11 */
    {'\\', '|'}, {'y', 'Y'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', '?'}, {'.', ':'}, {'-', '_'}, {},
};

static const xkb_keysym_t main_key_symbols_th[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {'_', '%'}, {K(Thai_lakkhangyao), '+'}, {'/', K(Thai_leknung)}, {'-', K(Thai_leksong)}, {K(Thai_phosamphao), K(Thai_leksam)}, {K(Thai_thothung), K(Thai_leksi)}, {K(Thai_sarau), K(Thai_sarauu)}, {K(Thai_saraue), K(Thai_baht)}, {K(Thai_khokhwai), K(Thai_lekha)}, {K(Thai_totao), K(Thai_lekhok)}, {K(Thai_chochan), K(Thai_lekchet)}, {K(Thai_khokhai), K(Thai_lekpaet)}, {K(Thai_chochang), K(Thai_lekkao)}, {},
    /* Row D: AD01-AD12 */
    {K(Thai_maiyamok), K(Thai_leksun)}, {K(Thai_saraaimaimalai), '"'}, {K(Thai_saraam), K(Thai_dochada)}, {K(Thai_phophan), K(Thai_thonangmontho)}, {K(Thai_saraa), K(Thai_thothong)}, {K(Thai_maihanakat), K(Thai_nikhahit)}, {K(Thai_saraii), K(Thai_maitri)}, {K(Thai_rorua), K(Thai_nonen)}, {K(Thai_nonu), K(Thai_paiyannoi)}, {K(Thai_yoyak), K(Thai_yoying)}, {K(Thai_bobaimai), K(Thai_thothan)}, {K(Thai_loling), ','},
    /* Row C: AC01-AC12 */
    {K(Thai_fofan), K(Thai_ru)}, {K(Thai_hohip), K(Thai_khorakhang)}, {K(Thai_kokai), K(Thai_topatak)}, {K(Thai_dodek), K(Thai_sarao)}, {K(Thai_sarae), K(Thai_chochoe)}, {K(Thai_maitho), K(Thai_maitaikhu)}, {K(Thai_maiek), K(Thai_maichattawa)}, {K(Thai_saraaa), K(Thai_sorusi)}, {K(Thai_sosua), K(Thai_sosala)}, {K(Thai_wowaen), K(Thai_soso)}, {K(Thai_ngongu), '.'}, {K(Thai_khokhuat), K(Thai_khokhon)},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {K(Thai_phophung), '('}, {K(Thai_popla), ')'}, {K(Thai_saraae), K(Thai_choching)}, {K(Thai_oang), K(Thai_honokhuk)}, {K(Thai_sarai), K(Thai_phinthu)}, {K(Thai_sarauee), K(Thai_thanthakhat)}, {K(Thai_thothahan), '?'}, {K(Thai_moma), K(Thai_thophuthao)}, {K(Thai_saraaimaimuan), K(Thai_lochula)}, {K(Thai_fofa), K(Thai_lu)}, {},
};

static const xkb_keysym_t main_key_symbols_tr[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {'"', K(eacute)}, {'1', '!'}, {'2', '\''}, {'3', '^'}, {'4', '+'}, {'5', '%'}, {'6', '&'}, {'7', '/'}, {'8', '('}, {'9', ')'}, {'0', '='}, {'*', '?'}, {'-', '_'}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {K(idotless), 'I'}, {'o', 'O'}, {'p', 'P'}, {K(gbreve), K(Gbreve)}, {K(udiaeresis), K(Udiaeresis)},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {K(scedilla), K(Scedilla)}, {'i', K(Iabovedot)}, {',', ';'},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {K(odiaeresis), K(Odiaeresis)}, {K(ccedilla), K(Ccedilla)}, {'.', ':'}, {},
};

static const xkb_keysym_t main_key_symbols_tr_f[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {'+', '*'}, {'1', '!'}, {'2', '"'}, {'3', '^'}, {'4', '$'}, {'5', '%'}, {'6', '&'}, {'7', '\''}, {'8', '('}, {'9', ')'}, {'0', '='}, {'/', '?'}, {'-', '_'}, {},
    /* Row D: AD01-AD12 */
    {'f', 'F'}, {'g', 'G'}, {K(gbreve), K(Gbreve)}, {K(idotless), 'I'}, {'o', 'O'}, {'d', 'D'}, {'r', 'R'}, {'n', 'N'}, {'h', 'H'}, {'p', 'P'}, {'q', 'Q'}, {'w', 'W'},
    /* Row C: AC01-AC12 */
    {'u', 'U'}, {'i', K(Iabovedot)}, {'e', 'E'}, {'a', 'A'}, {K(udiaeresis), K(Udiaeresis)}, {'t', 'T'}, {'k', 'K'}, {'m', 'M'}, {'l', 'L'}, {'y', 'Y'}, {K(scedilla), K(Scedilla)}, {'x', 'X'},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {'j', 'J'}, {K(odiaeresis), K(Odiaeresis)}, {'v', 'V'}, {'c', 'C'}, {K(ccedilla), K(Ccedilla)}, {'z', 'Z'}, {'s', 'S'}, {'b', 'B'}, {'.', ':'}, {',', ';'}, {},
};

static const xkb_keysym_t main_key_symbols_ua[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {'\'', 0x10002bc}, {'1', '!'}, {'2', '"'}, {'3', K(numerosign)}, {'4', ';'}, {'5', '%'}, {'6', ':'}, {'7', '?'}, {'8', '*'}, {'9', '('}, {'0', ')'}, {'-', '_'}, {'=', '+'}, {},
    /* Row D: AD01-AD12 */
    {K(Cyrillic_shorti), K(Cyrillic_SHORTI)}, {K(Cyrillic_tse), K(Cyrillic_TSE)}, {K(Cyrillic_u), K(Cyrillic_U)}, {K(Cyrillic_ka), K(Cyrillic_KA)}, {K(Cyrillic_ie), K(Cyrillic_IE)}, {K(Cyrillic_en), K(Cyrillic_EN)}, {K(Cyrillic_ghe), K(Cyrillic_GHE)}, {K(Cyrillic_sha), K(Cyrillic_SHA)}, {K(Cyrillic_shcha), K(Cyrillic_SHCHA)}, {K(Cyrillic_ze), K(Cyrillic_ZE)}, {K(Cyrillic_ha), K(Cyrillic_HA)}, {K(Ukrainian_yi), K(Ukrainian_YI)},
    /* Row C: AC01-AC12 */
    {K(Cyrillic_ef), K(Cyrillic_EF)}, {K(Ukrainian_i), K(Ukrainian_I)}, {K(Cyrillic_ve), K(Cyrillic_VE)}, {K(Cyrillic_a), K(Cyrillic_A)}, {K(Cyrillic_pe), K(Cyrillic_PE)}, {K(Cyrillic_er), K(Cyrillic_ER)}, {K(Cyrillic_o), K(Cyrillic_O)}, {K(Cyrillic_el), K(Cyrillic_EL)}, {K(Cyrillic_de), K(Cyrillic_DE)}, {K(Cyrillic_zhe), K(Cyrillic_ZHE)}, {K(Ukrainian_ie), K(Ukrainian_IE)}, {K(Ukrainian_ghe_with_upturn), K(Ukrainian_GHE_WITH_UPTURN)},
    /* Row B: LSGT, AB01-AB11 */
    {'/', '|'}, {K(Cyrillic_ya), K(Cyrillic_YA)}, {K(Cyrillic_che), K(Cyrillic_CHE)}, {K(Cyrillic_es), K(Cyrillic_ES)}, {K(Cyrillic_em), K(Cyrillic_EM)}, {K(Cyrillic_i), K(Cyrillic_I)}, {K(Cyrillic_te), K(Cyrillic_TE)}, {K(Cyrillic_softsign), K(Cyrillic_SOFTSIGN)}, {K(Cyrillic_be), K(Cyrillic_BE)}, {K(Cyrillic_yu), K(Cyrillic_YU)}, {'.', ','}, {},
};

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


static const xkb_keysym_t main_key_symbols_us_colemak[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {'`', '~'}, {'1', '!'}, {'2', '@'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', '^'}, {'7', '&'}, {'8', '*'}, {'9', '('}, {'0', ')'}, {'-', '_'}, {'=', '+'}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'f', 'F'}, {'p', 'P'}, {'g', 'G'}, {'j', 'J'}, {'l', 'L'}, {'u', 'U'}, {'y', 'Y'}, {';', ':'}, {'[', '{'}, {']', '}'},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'r', 'R'}, {'s', 'S'}, {'t', 'T'}, {'d', 'D'}, {'h', 'H'}, {'n', 'N'}, {'e', 'E'}, {'i', 'I'}, {'o', 'O'}, {'\'', '"'}, {'\\', '|'},
    /* Row B: LSGT, AB01-AB11 */
    {'-', '_'}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'k', 'K'}, {'m', 'M'}, {',', '<'}, {'.', '>'}, {'/', '?'}, {},
};

static const xkb_keysym_t main_key_symbols_us_dvorak[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {'`', '~'}, {'1', '!'}, {'2', '@'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', '^'}, {'7', '&'}, {'8', '*'}, {'9', '('}, {'0', ')'}, {'[', '{'}, {']', '}'}, {},
    /* Row D: AD01-AD12 */
    {'\'', '"'}, {',', '<'}, {'.', '>'}, {'p', 'P'}, {'y', 'Y'}, {'f', 'F'}, {'g', 'G'}, {'c', 'C'}, {'r', 'R'}, {'l', 'L'}, {'/', '?'}, {'=', '+'},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'o', 'O'}, {'e', 'E'}, {'u', 'U'}, {'i', 'I'}, {'d', 'D'}, {'h', 'H'}, {'t', 'T'}, {'n', 'N'}, {'s', 'S'}, {'-', '_'}, {'\\', '|'},
    /* Row B: LSGT, AB01-AB11 */
    {'<', '>'}, {';', ':'}, {'q', 'Q'}, {'j', 'J'}, {'k', 'K'}, {'x', 'X'}, {'b', 'B'}, {'m', 'M'}, {'w', 'W'}, {'v', 'V'}, {'z', 'Z'}, {},
};

static const xkb_keysym_t main_key_symbols_us_intl[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    /* Row E: TLDE, AE01-AE13 */
    {K(dead_grave), K(dead_tilde)}, {'1', '!'}, {'2', '@'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', K(dead_circumflex)}, {'7', '&'}, {'8', '*'}, {'9', '('}, {'0', ')'}, {'-', '_'}, {'=', '+'}, {},
    /* Row D: AD01-AD12 */
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, {'[', '{'}, {']', '}'},
    /* Row C: AC01-AC12 */
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {';', ':'}, {K(dead_acute), K(dead_diaeresis)}, {'\\', '|'},
    /* Row B: LSGT, AB01-AB11 */
    {'\\', '|'}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', '<'}, {'.', '>'}, {'/', '?'}, {},
};

#undef K

/*** The VNC keyboard layout is a special case */

static const WORD main_key_scan_vnc[MAIN_KEY_LEN] =
{
    0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,0x0C,0x0D,
    0x1A,0x1B,0x27,0x28,0x29,0x33,
    0x34,0x35,0x2B,0x1E,0x30,0x2E,
    0x20,0x12,0x21,0x22,0x23,0x17,
    0x24,0x25,0x26,0x32,0x31,0x18,
    0x19,0x10,0x13,0x1F,0x14,0x16,
    0x2F,0x11,0x2D,0x15,0x2C,0x56
};

static const WORD main_key_vkey_vnc[MAIN_KEY_LEN] =
{
    '1','2','3','4','5','6',
    '7','8','9','0',VK_OEM_MINUS,VK_OEM_PLUS,
    VK_OEM_4,VK_OEM_6,VK_OEM_1,VK_OEM_7,VK_OEM_3,VK_OEM_COMMA,
    VK_OEM_PERIOD,VK_OEM_2,VK_OEM_5, 'A','B','C',
    'D','E','F','G','H','I',
    'J','K','L','M','N','O',
    'P','Q','R','S','T','U',
    'V','W','X','Y','Z', VK_OEM_102
};

static const xkb_keysym_t main_key_symbols_vnc[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN] =
{
    {'1', '!'}, {'2', '@'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', '^'},
    {'7', '&'}, {'8', '*'}, {'9', '('}, {'0', ')'}, {'-', '_'}, {'=', '+'},
    {'[', '{'}, {']', '}'}, {';', ':'}, {'\'', '"'}, {'`', '~'}, {',', '<'},
    {'.', '>'}, {'/', '?'}, {'\\', '|'}, {'a', 'A'}, {'b', 'B'}, {'c', 'C'},
    {'d', 'D'}, {'e', 'E'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'i', 'I'},
    {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {'m', 'M'}, {'n', 'N'}, {'o', 'O'},
    {'p', 'P'}, {'q', 'Q'}, {'r', 'R'}, {'s', 'S'}, {'t', 'T'}, {'u', 'U'},
    {'v', 'V'}, {'w', 'W'}, {'x', 'X'}, {'y', 'Y'}, {'z', 'Z'}
};

/*** Layout table. Add your keyboard mappings to this list */
static struct {
    LCID lcid; /* input locale identifier, look for LOCALE_ILANGUAGE
                 in the appropriate dlls/kernel/nls/.nls file */
    const char *name;
    const xkb_keysym_t (*symbols)[MAIN_KEY_LEN][MAIN_KEY_SYMBOLS_LEN];
    const WORD (*scan)[MAIN_KEY_LEN]; /* scan codes mapping */
    const WORD (*vkey)[MAIN_KEY_LEN]; /* virtual key codes mapping */
} main_key_tab[]={
    {0x0402, "bg_bds", &main_key_symbols_bg_bds, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0402, "bg_phonetic", &main_key_symbols_bg_phonetic, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0405, "cz", &main_key_symbols_cz, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0405, "cz_qwerty", &main_key_symbols_cz_qwerty, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0406, "dk", &main_key_symbols_dk, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0407, "de", &main_key_symbols_de, &main_key_scan_ps2_set1, &main_key_vkey_qwertz},
    {0x0408, "gr", &main_key_symbols_gr, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0409, "us", &main_key_symbols_us, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0409, "us_colemak", &main_key_symbols_us_colemak, &main_key_scan_ps2_set1, &main_key_vkey_colemak},
    {0x0409, "us_dvorak", &main_key_symbols_us_dvorak, &main_key_scan_ps2_set1, &main_key_vkey_dvorak},
    {0x0409, "us_intl", &main_key_symbols_us_intl, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0409, "vnc", &main_key_symbols_vnc, &main_key_scan_vnc, &main_key_vkey_vnc},
    {0x040a, "es", &main_key_symbols_es, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x040b, "fi", &main_key_symbols_fi, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x040c, "fr", &main_key_symbols_fr, &main_key_scan_ps2_set1, &main_key_vkey_azerty},
    {0x040d, "il", &main_key_symbols_il, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x040d, "il_phonetic", &main_key_symbols_il_phonetic, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x040e, "hu", &main_key_symbols_hu, &main_key_scan_ps2_set1, &main_key_vkey_qwertz},
    {0x040f, "is", &main_key_symbols_is, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0410, "it", &main_key_symbols_it, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0411, "jp_106", &main_key_symbols_jp_106, &main_key_scan_ps2_set1, &main_key_vkey_qwerty_jp106},
    {0x0411, "jp_kana86", &main_key_symbols_jp_kana86, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0411, "jp_mac", &main_key_symbols_jp_mac, &main_key_scan_ps2_set1, &main_key_vkey_qwerty_jp106},
    {0x0413, "nl", &main_key_symbols_nl, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0414, "no", &main_key_symbols_no, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0415, "pl_dvp", &main_key_symbols_pl_dvp, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0416, "br_abnt2", &main_key_symbols_br_abnt2, &main_key_scan_ps2_set1, &main_key_vkey_abnt_qwerty},
    {0x0419, "ru", &main_key_symbols_ru, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0419, "ru_phonetic", &main_key_symbols_ru_phonetic, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x041a, "hr", &main_key_symbols_hr, &main_key_scan_ps2_set1, &main_key_vkey_qwertz},
    {0x041b, "sk", &main_key_symbols_sk, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x041d, "se", &main_key_symbols_se, &main_key_scan_ps2_set1, &main_key_vkey_qwerty_v2},
    {0x041e, "th", &main_key_symbols_th, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x041f, "tr_f", &main_key_symbols_tr_f, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x041f, "tr", &main_key_symbols_tr, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0422, "ua", &main_key_symbols_ua, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0423, "by", &main_key_symbols_by, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0424, "si", &main_key_symbols_si, &main_key_scan_ps2_set1, &main_key_vkey_qwertz},
    {0x0425, "ee", &main_key_symbols_ee, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0427, "lt", &main_key_symbols_lt, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0807, "ch_de", &main_key_symbols_ch_de, &main_key_scan_ps2_set1, &main_key_vkey_qwertz},
    {0x0809, "gb", &main_key_symbols_gb, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x080c, "be", &main_key_symbols_be, &main_key_scan_ps2_set1, &main_key_vkey_azerty},
    {0x0816, "pt", &main_key_symbols_pt, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0c0c, "ca", &main_key_symbols_ca, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x0c1a, "rs", &main_key_symbols_rs, &main_key_scan_ps2_set1, &main_key_vkey_qwerty},
    {0x100c, "ch_fr", &main_key_symbols_ch_fr, &main_key_scan_ps2_set1, &main_key_vkey_qwertz},
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
