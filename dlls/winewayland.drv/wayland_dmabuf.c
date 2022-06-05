/*
 * Wayland dmabuf buffers
 *
 * Copyright 2022 Alexandros Frantzis for Collabora Ltd
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

#include <drm_fourcc.h>
#include <stdlib.h>

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

/**********************************************************************
 *          dmabuf private helpers
 */

static BOOL dmabuf_format_has_modifier(struct wayland_dmabuf_format *format, uint64_t modifier)
{
    uint64_t *mod;

    wl_array_for_each(mod, &format->modifiers)
        if (*mod == modifier) return TRUE;

    return FALSE;
}

static struct wayland_dmabuf_format *dmabuf_format_array_find_format(struct wl_array *formats,
                                                                     uint32_t format)
{
    struct wayland_dmabuf_format *dmabuf_format;
    BOOL format_found = FALSE;

    wl_array_for_each(dmabuf_format, formats)
    {
        if (dmabuf_format->format == format)
        {
            format_found = TRUE;
            break;
        }
    }

    if (!format_found) dmabuf_format = NULL;

    return dmabuf_format;
}

static BOOL dmabuf_format_array_add_format_modifier(struct wl_array *formats,
                                                    uint32_t format,
                                                    uint64_t modifier)
{
    struct wayland_dmabuf_format *dmabuf_format;
    uint64_t *mod;

    if ((dmabuf_format = dmabuf_format_array_find_format(formats, format)))
    {
        /* Avoid a possible duplicate, e.g., if compositor sends both format and
         * modifier event with a DRM_FORMAT_MOD_INVALID. */
        if (dmabuf_format_has_modifier(dmabuf_format, modifier))
            goto out;
    }
    else
    {
        if (!(dmabuf_format = wl_array_add(formats, sizeof(*dmabuf_format))))
            goto out;
        dmabuf_format->format = format;
        wl_array_init(&dmabuf_format->modifiers);
    }

    if (!(mod = wl_array_add(&dmabuf_format->modifiers, sizeof(uint64_t))))
    {
        dmabuf_format = NULL;
        goto out;
    }

    *mod = modifier;

out:
    return dmabuf_format != NULL;
}

static void dmabuf_format_array_release(struct wl_array *formats)
{
    struct wayland_dmabuf_format *format;

    wl_array_for_each(format, formats)
        wl_array_release(&format->modifiers);

    wl_array_release(formats);
}

/**********************************************************************
 *          zwp_linux_dmabuf_v1 handling
 */

static void dmabuf_format(void *data, struct zwp_linux_dmabuf_v1 *zwp_dmabuf, uint32_t format)
{
    struct wayland_dmabuf *dmabuf = data;

    if (!dmabuf_format_array_add_format_modifier(&dmabuf->formats, format, DRM_FORMAT_MOD_INVALID))
        WARN("Could not add format 0x%08x\n", format);
}

static void dmabuf_modifiers(void *data, struct zwp_linux_dmabuf_v1 *zwp_dmabuf, uint32_t format,
                             uint32_t mod_hi, uint32_t mod_lo)
{
    struct wayland_dmabuf *dmabuf = data;
    const uint64_t modifier = (uint64_t)mod_hi << 32 | mod_lo;

    if (!dmabuf_format_array_add_format_modifier(&dmabuf->formats, format, modifier))
        WARN("Could not add format/modifier 0x%08x/0x%" PRIx64 "\n", format, modifier);
}

static const struct zwp_linux_dmabuf_v1_listener dmabuf_listener = {
    dmabuf_format,
    dmabuf_modifiers
};

/***********************************************************************
 *           wayland_dmabuf_init
 */
void wayland_dmabuf_init(struct wayland_dmabuf *dmabuf,
                         struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf_v1)
{
    dmabuf->zwp_linux_dmabuf_v1 = zwp_linux_dmabuf_v1;
    wl_array_init(&dmabuf->formats);
    zwp_linux_dmabuf_v1_add_listener(zwp_linux_dmabuf_v1, &dmabuf_listener, dmabuf);
}

/***********************************************************************
 *           wayland_dmabuf_deinit
 */
void wayland_dmabuf_deinit(struct wayland_dmabuf *dmabuf)
{
    dmabuf_format_array_release(&dmabuf->formats);

    if (dmabuf->zwp_linux_dmabuf_v1)
        zwp_linux_dmabuf_v1_destroy(dmabuf->zwp_linux_dmabuf_v1);
}

/**********************************************************************
 *          wayland_dmabuf_buffer_from_native
 *
 * Creates a wayland dmabuf buffer from the specified native buffer.
 */
struct wayland_dmabuf_buffer *wayland_dmabuf_buffer_create_from_native(struct wayland *wayland,
                                                                       struct wayland_native_buffer *native)
{
    struct wayland_dmabuf_buffer *dmabuf_buffer;
    struct zwp_linux_buffer_params_v1 *params;
    int i;

    dmabuf_buffer = calloc(1, sizeof(*dmabuf_buffer));
    if (!dmabuf_buffer)
        goto err;

    params = zwp_linux_dmabuf_v1_create_params(wayland->dmabuf.zwp_linux_dmabuf_v1);
    for (i = 0; i < native->plane_count; i++)
    {
        zwp_linux_buffer_params_v1_add(params,
                                       native->fds[i],
                                       i,
                                       native->offsets[i],
                                       native->strides[i],
                                       native->modifier >> 32,
                                       native->modifier & 0xffffffff);
    }

    dmabuf_buffer->wl_buffer =
        zwp_linux_buffer_params_v1_create_immed(params,
                                                native->width,
                                                native->height,
                                                native->format,
                                                0);

    zwp_linux_buffer_params_v1_destroy(params);

    return dmabuf_buffer;

err:
    if (dmabuf_buffer)
        wayland_dmabuf_buffer_destroy(dmabuf_buffer);
    return NULL;
}

/**********************************************************************
 *          wayland_dmabuf_buffer_destroy
 *
 * Destroys a dmabuf buffer.
 */
void wayland_dmabuf_buffer_destroy(struct wayland_dmabuf_buffer *dmabuf_buffer)
{
    TRACE("%p\n", dmabuf_buffer);

    if (dmabuf_buffer->wl_buffer)
        wl_buffer_destroy(dmabuf_buffer->wl_buffer);

    free(dmabuf_buffer);
}

/**********************************************************************
 *          wayland_dmabuf_buffer_steal_wl_buffer_and_destroy
 *
 * Steal the wl_buffer from a dmabuf buffer and destroy the dmabuf buffer.
 */
struct wl_buffer *wayland_dmabuf_buffer_steal_wl_buffer_and_destroy(struct wayland_dmabuf_buffer *dmabuf_buffer)
{
    struct wl_buffer *wl_buffer;

    wl_buffer = dmabuf_buffer->wl_buffer;
    dmabuf_buffer->wl_buffer = NULL;

    wayland_dmabuf_buffer_destroy(dmabuf_buffer);

    return wl_buffer;
}
