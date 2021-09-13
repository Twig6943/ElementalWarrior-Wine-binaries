/*
 * Wayland output handling
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

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include "waylanddrv.h"

#include "wine/debug.h"

#include <math.h>
#include <stdlib.h>

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

struct default_mode { int32_t width; int32_t height; };
static const struct default_mode default_modes[] = {
    { 320,  200}, /* CGA 16:10 */
    { 320,  240}, /* QVGA 4:3 */
    { 400,  300}, /* qSVGA 4:3 */
    { 480,  320}, /* HVGA 3:2 */
    { 512,  384}, /* MAC 4:3 */
    { 640,  360}, /* nHD 16:9 */
    { 640,  400}, /* VESA-0100h 16:10 */
    { 640,  480}, /* VGA 4:3 */
    { 720,  480}, /* WVGA 3:2 */
    { 720,  576}, /* PAL 5:4 */
    { 768,  480}, /* WVGA 16:10 */
    { 768,  576}, /* PAL* 4:3 */
    { 800,  600}, /* SVGA 4:3 */
    { 854,  480}, /* FWVGA 16:9 */
    { 960,  540}, /* qHD 16:9 */
    { 960,  640}, /* DVGA 3:2 */
    {1024,  576}, /* WSVGA 16:9 */
    {1024,  640}, /* WSVGA 16:10 */
    {1024,  768}, /* XGA 4:3 */
    {1152,  864}, /* XGA+ 4:3 */
    {1280,  720}, /* HD 16:9 */
    {1280,  768}, /* WXGA 5:3 */
    {1280,  800}, /* WXGA 16:10 */
    {1280,  960}, /* SXGA- 4:3 */
    {1280, 1024}, /* SXGA 5:4 */
    {1366,  768}, /* FWXGA 16:9 */
    {1400, 1050}, /* SXGA+ 4:3 */
    {1440,  900}, /* WSXGA 16:10 */
    {1600,  900}, /* HD+ 16:9 */
    {1600, 1200}, /* UXGA 4:3 */
    {1680, 1050}, /* WSXGA+ 16:10 */
    {1920, 1080}, /* FHD 16:9 */
    {1920, 1200}, /* WUXGA 16:10 */
    {2048, 1152}, /* QWXGA 16:9 */
    {2048, 1536}, /* QXGA 4:3 */
    {2560, 1440}, /* QHD 16:9 */
    {2560, 1600}, /* WQXGA 16:10 */
    {2560, 2048}, /* QSXGA 5:4 */
    {2880, 1620}, /* 3K 16:9 */
    {3200, 1800}, /* QHD+ 16:9 */
    {3200, 2400}, /* QUXGA 4:3 */
    {3840, 2160}, /* 4K 16:9 */
    {3840, 2400}, /* WQUXGA 16:10 */
    {5120, 2880}, /* 5K 16:9 */
    {7680, 4320}, /* 8K 16:9 */
};

static const int32_t default_refresh = 60000;

/**********************************************************************
 *          Output handling
 */

/* Compare mode with the set of provided mode parameters and return -1 if the
 * mode compares less than the parameters, 0 if the mode compares equal to the
 * parameters, and 1 if the mode compares greater than the parameters.
 *
 * The comparison is based on comparing the width, height, bpp and refresh
 * in that order.
 */
static int wayland_output_mode_cmp(struct wayland_output_mode *mode,
                                   int32_t width, int32_t height,
                                   int32_t refresh, int bpp)
{
    if (mode->width < width) return -1;
    if (mode->width > width) return 1;
    if (mode->height < height) return -1;
    if (mode->height > height) return 1;
    if (mode->bpp < bpp) return -1;
    if (mode->bpp > bpp) return 1;
    if (mode->refresh < refresh) return -1;
    if (mode->refresh > refresh) return 1;
    return 0;
}

static void wayland_output_add_mode(struct wayland_output *output,
                                    int32_t width, int32_t height,
                                    int32_t refresh, int bpp,
                                    BOOL current, BOOL native)
{
    struct wayland_output_mode *mode;
    struct wl_list *insert_after_link = output->mode_list.prev;

    /* Update mode if it's already in list, otherwise find the insertion point
     * to maintain the sorted order. */
    wl_list_for_each(mode, &output->mode_list, link)
    {
        int cmp = wayland_output_mode_cmp(mode, width, height, refresh, bpp);
        if (cmp == 0) /* mode == new */
        {
            /* Upgrade modes from virtual to native, never the reverse. */
            if (native) mode->native = TRUE;
            if (current)
            {
                output->current_mode = mode;
                output->current_wine_mode = mode;
            }
            return;
        }
        else if (cmp == 1) /* mode > new */
        {
            insert_after_link = mode->link.prev;
            break;
        }
    }

    mode = calloc(1, sizeof(*mode));

    mode->width = width;
    mode->height = height;
    mode->refresh = refresh;
    mode->bpp = bpp;
    mode->native = native;

    if (current)
    {
        output->current_mode = mode;
        output->current_wine_mode = mode;
    }

    wl_list_insert(insert_after_link, &mode->link);
}

static void wayland_output_add_mode_all_bpp(struct wayland_output *output,
                                            int32_t width, int32_t height,
                                            int32_t refresh, BOOL current,
                                            BOOL native)
{
    wayland_output_add_mode(output, width, height, refresh, 32, current, native);
    wayland_output_add_mode(output, width, height, refresh, 16, FALSE, native);
    wayland_output_add_mode(output, width, height, refresh, 8, FALSE, native);
}

static void wayland_output_add_default_modes(struct wayland_output *output)
{
    int i;
    struct wayland_output_mode *mode, *tmp;
    int32_t max_width = 0;
    int32_t max_height = 0;
    int32_t current_refresh =
        output->current_mode ? output->current_mode->refresh : default_refresh;

    /* Remove all existing virtual modes and get the maximum native
     * mode size. */
    wl_list_for_each_safe(mode, tmp, &output->mode_list, link)
    {
        if (!mode->native)
        {
            wl_list_remove(&mode->link);
            free(mode);
        }
        else
        {
            max_width = mode->width > max_width ? mode->width : max_width;
            max_height = mode->height > max_height ? mode->height : max_height;
        }
    }

    for (i = 0; i < ARRAY_SIZE(default_modes); i++)
    {
        int32_t width = default_modes[i].width;
        int32_t height = default_modes[i].height;

        /* Skip if this mode is larger than the largest native mode. */
        if (width > max_width || height > max_height)
        {
            TRACE("Skipping mode %dx%d (max: %dx%d)\n",
                    width, height, max_width, max_height);
            continue;
        }

        wayland_output_add_mode_all_bpp(output, width, height, current_refresh,
                                        FALSE, FALSE);
    }
}

static int wayland_output_cmp_x(struct wayland_output *a, struct wayland_output *b)
{
    if (a->logical_x < b->logical_x) return -1;
    if (a->logical_x > b->logical_x) return 1;
    if (a->logical_y < b->logical_y) return -1;
    if (a->logical_y > b->logical_y) return 1;
    return 0;
}

static int wayland_output_cmp_y(struct wayland_output *a, struct wayland_output *b)
{
    if (a->logical_y < b->logical_y) return -1;
    if (a->logical_y > b->logical_y) return 1;
    if (a->logical_x < b->logical_x) return -1;
    if (a->logical_x > b->logical_x) return 1;
    return 0;
}

static struct wayland_output** wayland_output_list_sorted(struct wl_list *output_list,
                                                          int (*cmp)(struct wayland_output *,
                                                                     struct wayland_output *))
{
    int num_outputs = wl_list_length(output_list);
    int num_sorted = 0;
    struct wayland_output **sorted;
    struct wayland_output *o;

    sorted = malloc(sizeof(*sorted) * (num_outputs + 1));
    if (!sorted)
    {
        ERR("Couldn't allocate space for sorted outputs\n");
        return NULL;
    }

    wl_list_for_each(o, output_list, link)
    {
        int j = num_sorted;
        while (j > 0 && cmp(o, sorted[j - 1]) < 0)
        {
            sorted[j] = sorted[j - 1];
            j--;
        }
        sorted[j] = o;
        num_sorted++;
    }

    sorted[num_outputs] = NULL;
    return sorted;
}

static void wayland_output_list_update_physical_coords(struct wl_list *output_list)
{
    struct wayland_output **sorted_x, **sorted_y;
    struct wayland_output **cur_p, **prev_p;
    struct wayland_output *cur, *prev;

    /* Set default physical coordinates. */
    wl_list_for_each(cur, output_list, link)
    {
        cur->x = cur->logical_x;
        cur->y = cur->logical_y;
    }

    /* Sort and process the outputs from left to right. */
    cur_p = sorted_x = wayland_output_list_sorted(output_list, wayland_output_cmp_x);
    if (!sorted_x) return;

    while ((cur = *cur_p))
    {
        /* Update output->x based on other outputs that are to to the left. */
        prev_p = sorted_x;
        while ((prev = *prev_p) != cur)
        {
            if (cur->logical_x == prev->logical_x + prev->logical_w &&
                prev->current_mode)
            {
                int new_x = prev->x + prev->current_mode->width;
                if (new_x > cur->x) cur->x = new_x;
            }
            prev_p++;
        }

        cur_p++;
    }

    free(sorted_x);

    /* Now sort and process the outputs from top to bottom. */
    cur_p = sorted_y = wayland_output_list_sorted(output_list, wayland_output_cmp_y);
    if (!sorted_y) return;

    while ((cur = *cur_p))
    {
        /* Update output->y based on other outputs that are above. */
        prev_p = sorted_y;
        while ((prev = *prev_p) != cur)
        {
            if (cur->logical_y == prev->logical_y + prev->logical_h &&
                prev->current_mode)
            {
                int new_y = prev->y + prev->current_mode->height;
                if (new_y > cur->y) cur->y = new_y;
            }
            prev_p++;
        }

        cur_p++;
    }

    free(sorted_y);
}

static void wayland_output_clear_modes(struct wayland_output *output)
{
    struct wayland_output_mode *mode, *tmp;

    wl_list_for_each_safe(mode, tmp, &output->mode_list, link)
    {
        wl_list_remove(&mode->link);
        free(mode);
    }
}

static void wayland_output_update_scale(struct wayland_output *output)
{
    double inferred_scale = 0.0;

    if (output->logical_w != 0 && output->logical_h != 0 &&
        output->current_mode)
    {
        double scale_x = (double)output->current_mode->width / output->logical_w;
        double scale_y = (double)output->current_mode->height / output->logical_h;
        if (fabs(scale_x - scale_y) > 0.01)
            WARN("different scale_x=%f scale_y=%f", scale_x, scale_y);
        inferred_scale = max(scale_x, scale_y);
    }

    if (inferred_scale == 0.0 ||
        (inferred_scale == 1.0 && output->compositor_scale != 1.0))
    {
        output->scale = output->compositor_scale;
        TRACE("using scale=%.2f reported by compositor\n", output->scale);
    }
    else
    {
        output->scale = inferred_scale;
        TRACE("using scale=%.2f inferred from physical and logical sizes\n",
              output->scale);
    }
}

static void wayland_output_done(struct wayland_output *output)
{
    struct wayland_output_mode *mode;
    struct wayland_output *o;

    TRACE("output->name=%s\n", output->name);

    wayland_output_add_default_modes(output);
    wayland_output_list_update_physical_coords(&output->wayland->output_list);
    wayland_output_update_scale(output);

    wl_list_for_each(mode, &output->mode_list, link)
    {
        TRACE("mode %dx%d @ %d %s\n",
              mode->width, mode->height, mode->refresh,
              output->current_mode == mode ? "*" : "");
    }

    wl_list_for_each(o, &output->wayland->output_list, link)
    {
        if (!o->current_mode) continue;
        TRACE("output->name=%s scale=%.2f logical=%d,%d+%dx%d physical=%d,%d+%dx%d\n",
              o->name, o->scale,
              o->logical_x, output->logical_y, o->logical_w, o->logical_h,
              o->x, o->y, o->current_mode->width, o->current_mode->height);
    }

    if (wayland_is_process(output->wayland))
    {
        /* Temporarily release the per-process instance lock, so that
         * wayland_init_display_devices can perform more fine grained locking
         * to avoid deadlocks. */
        wayland_process_release();
        wayland_init_display_devices();
        wayland_process_acquire();
    }
    else
    {
        wayland_update_outputs_from_process(output->wayland);
    }
}

static void output_handle_geometry(void *data, struct wl_output *wl_output,
                                   int32_t x, int32_t y,
                                   int32_t physical_width, int32_t physical_height,
                                   int32_t subpixel,
                                   const char *make, const char *model,
                                   int32_t output_transform)
{
}

static void output_handle_mode(void *data, struct wl_output *wl_output,
                               uint32_t flags, int32_t width, int32_t height,
                               int32_t refresh)
{
    struct wayland_output *output = data;

    /* Windows apps don't expect a zero refresh rate, so use a default value. */
    if (refresh == 0) refresh = default_refresh;

    wayland_output_add_mode_all_bpp(output, width, height, refresh,
                                    (flags & WL_OUTPUT_MODE_CURRENT),
                                    TRUE);
}

static void output_handle_done(void *data, struct wl_output *wl_output)
{
    struct wayland_output *output = data;
    if (!output->zxdg_output_v1 ||
        zxdg_output_v1_get_version(output->zxdg_output_v1) >= 3)
    {
        wayland_output_done(output);
    }
}

static void output_handle_scale(void *data, struct wl_output *wl_output,
                                int32_t scale)
{
    struct wayland_output *output = data;
    TRACE("output=%p scale=%d\n", output, scale);
    output->compositor_scale = scale;
}

static const struct wl_output_listener output_listener = {
    output_handle_geometry,
    output_handle_mode,
    output_handle_done,
    output_handle_scale
};

static void zxdg_output_v1_handle_logical_position(void *data,
                                                   struct zxdg_output_v1 *zxdg_output_v1,
                                                   int32_t x,
                                                   int32_t y)
{
    struct wayland_output *output = data;
    TRACE("logical_x=%d logical_y=%d\n", x, y);
    output->logical_x = x;
    output->logical_y = y;
}

static void zxdg_output_v1_handle_logical_size(void *data,
                                               struct zxdg_output_v1 *zxdg_output_v1,
                                               int32_t width,
                                               int32_t height)
{
    struct wayland_output *output = data;
    TRACE("logical_w=%d logical_h=%d\n", width, height);
    output->logical_w = width;
    output->logical_h = height;
}

static void zxdg_output_v1_handle_done(void *data,
                                       struct zxdg_output_v1 *zxdg_output_v1)
{
    if (zxdg_output_v1_get_version(zxdg_output_v1) < 3)
    {
        struct wayland_output *output = data;
        wayland_output_done(output);
    }
}

static void zxdg_output_v1_handle_name(void *data,
                                       struct zxdg_output_v1 *zxdg_output_v1,
                                       const char *name)
{
    struct wayland_output *output = data;

    free(output->name);
    output->name = strdup(name);
}

static void zxdg_output_v1_handle_description(void *data,
                                              struct zxdg_output_v1 *zxdg_output_v1,
                                              const char *description)
{
}

static const struct zxdg_output_v1_listener zxdg_output_v1_listener = {
    zxdg_output_v1_handle_logical_position,
    zxdg_output_v1_handle_logical_size,
    zxdg_output_v1_handle_done,
    zxdg_output_v1_handle_name,
    zxdg_output_v1_handle_description,
};

/**********************************************************************
 *          wayland_output_create
 *
 *  Creates a wayland_output and adds it to the output list.
 */
BOOL wayland_output_create(struct wayland *wayland, uint32_t id, uint32_t version)
{
    struct wayland_output *output = calloc(1, sizeof(*output));

    if (!output)
    {
        ERR("Couldn't allocate space for wayland_output\n");
        goto err;
    }

    output->wayland = wayland;
    output->wl_output = wl_registry_bind(wayland->wl_registry, id,
                                         &wl_output_interface,
                                         version < 2 ? version : 2);
    output->global_id = id;
    wl_output_add_listener(output->wl_output, &output_listener, output);

    wl_list_init(&output->mode_list);
    wl_list_init(&output->link);

    output->compositor_scale = 1.0;
    output->scale = 1.0;

    /* Have a fallback in case xdg_output is not supported or name is not sent. */
    output->name = malloc(20);
    if (output->name)
    {
        snprintf(output->name, 20, "WaylandOutput%d",
                 wayland->next_fallback_output_id++);
    }
    else
    {
        ERR("Couldn't allocate space for output name\n");
        goto err;
    }

    if (wayland->zxdg_output_manager_v1)
        wayland_output_use_xdg_extension(output);

    wl_list_insert(output->wayland->output_list.prev, &output->link);

    return TRUE;

err:
    if (output) wayland_output_destroy(output);
    return FALSE;
}

/**********************************************************************
 *          wayland_output_destroy
 *
 *  Destroys a wayland_output.
 */
void wayland_output_destroy(struct wayland_output *output)
{
    wayland_output_clear_modes(output);
    wl_list_remove(&output->link);
    free(output->name);
    if (output->zxdg_output_v1)
        zxdg_output_v1_destroy(output->zxdg_output_v1);
    wl_output_destroy(output->wl_output);

    free(output);
}

/**********************************************************************
 *          wayland_output_use_xdg_extension
 *
 *  Use the zxdg_output_v1 extension to get output information.
 */
void wayland_output_use_xdg_extension(struct wayland_output *output)
{
    output->zxdg_output_v1 =
        zxdg_output_manager_v1_get_xdg_output(output->wayland->zxdg_output_manager_v1,
                                              output->wl_output);
    zxdg_output_v1_add_listener(output->zxdg_output_v1, &zxdg_output_v1_listener,
                                output);
}

/**********************************************************************
 *          wayland_update_outputs_from_process
 *
 * Update the information in the outputs of this instance, using the
 * information in the process wayland instance.
 */
void wayland_update_outputs_from_process(struct wayland *wayland)
{
    struct wayland_output *output;
    struct wayland_output *process_output;
    struct wayland *process_wayland = wayland_process_acquire();

    TRACE("wayland=%p process_wayland=%p\n", wayland, process_wayland);

    wl_list_for_each(output, &wayland->output_list, link)
    {
        wl_list_for_each(process_output, &process_wayland->output_list, link)
        {
            if (!strcmp(output->name, process_output->name))
            {
                lstrcpyW(output->wine_name, process_output->wine_name);
                break;
            }
        }
    }

    wayland_process_release();
}

/**********************************************************************
 *          wayland_output_get_by_wine_name
 *
 *  Returns the wayland_output with the specified Wine name (or NULL
 *  if not present).
 */
struct wayland_output *wayland_output_get_by_wine_name(struct wayland *wayland,
                                                       LPCWSTR wine_name)
{
    struct wayland_output *output;

    wl_list_for_each(output, &wayland->output_list, link)
    {
        if (!wcsicmp(wine_name, output->wine_name))
            return output;
    }

    return NULL;
}
