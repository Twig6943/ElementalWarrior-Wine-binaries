/*
 * Debugging functions for pixel buffer contents
 *
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

#include "waylanddrv.h"

#include "wine/debug.h"

#include <assert.h>
#include <stdlib.h>

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

/* Dump the contents of a pixel buffer, along with the outlines of damage
 * and window regions, to a netpbm .pam file. */
void dump_pixels(const char *fpattern, int dbgid, unsigned int *pixels,
                 int width, int height, BOOL alpha, HRGN damage, HRGN win_region)
{
    char fname[128] = {0};
    RGNDATA *damage_data;
    RGNDATA *win_region_data;
    FILE *fp;
    int x, y;

    damage_data = get_region_data(damage);
    win_region_data = get_region_data(win_region);

    snprintf(fname, sizeof(fname), fpattern, dbgid);
    TRACE("dumping pixels to %s\n", fname);

    fp = fopen(fname, "w");
    assert(fp && "Failed to open target file for dump pixels. Does the target directory exist?");

    fprintf(fp, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n",
            width, height);

    for (y = 0; y < height; ++y)
    {
        for (x = 0; x < width; ++x)
        {
            BOOL draw_damage = FALSE;
            BOOL draw_win_region = FALSE;
            RECT *rgn_rect;
            RECT *end;

            if (damage_data)
            {
                rgn_rect = (RECT *)damage_data->Buffer;
                end = rgn_rect + damage_data->rdh.nCount;

                /* Draw the outlines of damaged areas. */
                for (;rgn_rect < end; rgn_rect++)
                {
                    if ((y == rgn_rect->top ||
                         y == rgn_rect->bottom - 1) &&
                        x >= rgn_rect->left &&
                        x < (rgn_rect->right))
                    {
                        draw_damage = TRUE;
                        break;
                    }
                    if ((x == rgn_rect->left ||
                         x == rgn_rect->right - 1) &&
                        y >= rgn_rect->top &&
                        y < (rgn_rect->bottom))
                    {
                        draw_damage = TRUE;
                        break;
                    }
                }
            }

            if (win_region_data)
            {
                /* Draw the outlines of window region areas. */
                rgn_rect = (RECT *)win_region_data->Buffer;
                end = rgn_rect + win_region_data->rdh.nCount;

                for (;rgn_rect < end; rgn_rect++)
                {
                    if ((y == rgn_rect->top ||
                         y == rgn_rect->bottom - 1) &&
                        x >= rgn_rect->left &&
                        x < (rgn_rect->right))
                    {
                        draw_win_region = TRUE;
                        break;
                    }
                    if ((x == rgn_rect->left ||
                         x == rgn_rect->right - 1) &&
                        y >= rgn_rect->top &&
                        y < (rgn_rect->bottom))
                    {
                        draw_win_region = TRUE;
                        break;
                    }
                }
            }

            if (draw_damage || draw_win_region)
            {
                unsigned char rgba[4] = {
                    draw_damage ? 0xff : 0x00,
                    draw_win_region ? 0xff : 0x00,
                    0x00, 0xff
                };
                fwrite(&rgba, sizeof(rgba), 1, fp);
            }
            else
            {
                unsigned int *pixel = (unsigned int *)((char *)pixels +
                                                       width * 4 * y + 4 * x);
                unsigned char rgba[4] = {
                    (*pixel & 0x00ff0000) >> 16,
                    (*pixel & 0x0000ff00) >> 8,
                    (*pixel & 0xff),
                    alpha ? (*pixel & 0xff000000) >> 24 : 0xff,
                };

                fwrite(&rgba, sizeof(rgba), 1, fp);
            }
        }
    }

    fflush(fp);
    fclose(fp);

    free(damage_data);
    free(win_region_data);
}
