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
#include <unistd.h>
#include <sys/mman.h>

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

/**********************************************************************
 *          dmabuf private helpers
 */

static BOOL dmabuf_has_feedback_support(struct wayland_dmabuf *dmabuf)
{
    return dmabuf->version >= ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK_SINCE_VERSION;
}

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

static void dmabuf_feedback_tranche_init(struct wayland_dmabuf_feedback_tranche *tranche)
{
    memset(tranche, 0, sizeof(*tranche));
    wl_array_init(&tranche->formats);
}

/* Moves src tranche to dst, and resets src. */
static void dmabuf_feedback_tranche_move(struct wayland_dmabuf_feedback_tranche *dst,
                                         struct wayland_dmabuf_feedback_tranche *src)
{
    memcpy(dst, src, sizeof(*dst));
    dmabuf_feedback_tranche_init(src);
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

/**********************************************************************
 *          default feedback handling
 */

static void dmabuf_feedback_main_device(void *data,
                                        struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                                        struct wl_array *device)
{
    struct wayland_dmabuf_feedback *feedback = data;

    if (device->size != sizeof(feedback->main_device))
        return;

    memcpy(&feedback->main_device, device->data, device->size);
}

static void dmabuf_feedback_format_table(void *data,
                                         struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                                         int32_t fd, uint32_t size)
{
    struct wayland_dmabuf_feedback *feedback = data;

    feedback->format_table_entries = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (feedback->format_table_entries == MAP_FAILED)
    {
        WARN("Failed to mmap format table entries. fd %d size %u.\n", fd, size);
        feedback->format_table_entries = NULL;
        close(fd);
        return;
    }

    feedback->format_table_size = size;
    close(fd);
}

static void dmabuf_feedback_tranche_target_device(void *data,
                                                  struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                                                  struct wl_array *device)
{
    struct wayland_dmabuf_feedback *feedback = data;

    memcpy(&feedback->pending_tranche.device, device->data, sizeof(dev_t));
}

static void dmabuf_feedback_tranche_formats(void *data,
                                            struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                                            struct wl_array *indices)
{
    struct wayland_dmabuf_feedback *feedback = data;
    struct wayland_dmabuf_feedback_format_table_entry *table_entries = feedback->format_table_entries;
    uint16_t *index;

    if (!table_entries)
    {
        WARN("Could not add formats/modifiers to tranche due to missing format table\n");
        return;
    }

    wl_array_for_each(index, indices)
    {
        if (!dmabuf_format_array_add_format_modifier(&feedback->pending_tranche.formats,
                                                     table_entries[*index].format,
                                                     table_entries[*index].modifier))
        {
            WARN("Could not add format/modifier 0x%08x/0x%" PRIx64 "\n",
                 table_entries[*index].format,
                 table_entries[*index].modifier);
        }
    }
}

static void dmabuf_feedback_tranche_flags(void *data,
                                          struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                                          uint32_t flags)
{
    struct wayland_dmabuf_feedback *feedback = data;

    feedback->pending_tranche.flags = flags;
}

static void dmabuf_feedback_tranche_done(void *data,
                                         struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1)
{
    struct wayland_dmabuf_feedback *feedback = data;
    struct wayland_dmabuf_feedback_tranche *tranche;

    if (feedback->pending_tranche.formats.size == 0 ||
        !(tranche = wl_array_add(&feedback->tranches, sizeof(*tranche))))
    {
        WARN("Failed to add tranche with target device %ju\n",
             (uintmax_t)feedback->pending_tranche.device);
        dmabuf_format_array_release(&feedback->pending_tranche.formats);
        dmabuf_feedback_tranche_init(&feedback->pending_tranche);
        return;
    }
    dmabuf_feedback_tranche_move(tranche, &feedback->pending_tranche);
}

static void dmabuf_feedback_done(void *data,
                                 struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1)
{
    /* ignore event */
}

static const struct zwp_linux_dmabuf_feedback_v1_listener dmabuf_feedback_listener =
{
    .main_device = dmabuf_feedback_main_device,
    .format_table = dmabuf_feedback_format_table,
    .tranche_target_device = dmabuf_feedback_tranche_target_device,
    .tranche_formats = dmabuf_feedback_tranche_formats,
    .tranche_flags = dmabuf_feedback_tranche_flags,
    .tranche_done = dmabuf_feedback_tranche_done,
    .done = dmabuf_feedback_done,
};

static void dmabuf_feedback_destroy(struct wayland_dmabuf_feedback *feedback)
{
    struct wayland_dmabuf_feedback_tranche *tranche;

    dmabuf_format_array_release(&feedback->pending_tranche.formats);

    wl_array_for_each(tranche, &feedback->tranches)
        dmabuf_format_array_release(&tranche->formats);
    wl_array_release(&feedback->tranches);

    free(feedback);
}

static struct wayland_dmabuf_feedback *dmabuf_feedback_create(void)
{
    struct wayland_dmabuf_feedback *feedback;

    feedback = calloc(1, sizeof(*feedback));
    if (!feedback) return NULL;

    wl_array_init(&feedback->tranches);
    dmabuf_feedback_tranche_init(&feedback->pending_tranche);

    return feedback;
}

/**********************************************************************
 *          per-surface feedback handling
 */

static void surface_dmabuf_feedback_main_device(void *data,
                                                struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                                                struct wl_array *device)
{
    struct wayland_dmabuf_surface_feedback *surface_feedback = data;

    dmabuf_feedback_main_device(surface_feedback->pending_feedback, zwp_linux_dmabuf_feedback_v1, device);
}

static void surface_dmabuf_feedback_format_table(void *data,
                                                 struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                                                 int32_t fd, uint32_t size)
{
    struct wayland_dmabuf_surface_feedback *surface_feedback = data;

    dmabuf_feedback_format_table(surface_feedback->pending_feedback, zwp_linux_dmabuf_feedback_v1, fd, size);
}

static void surface_dmabuf_feedback_tranche_target_device(void *data,
                                                          struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                                                          struct wl_array *device)
{
    struct wayland_dmabuf_surface_feedback *surface_feedback = data;

    dmabuf_feedback_tranche_target_device(surface_feedback->pending_feedback, zwp_linux_dmabuf_feedback_v1, device);
}

static void surface_dmabuf_feedback_tranche_formats(void *data,
                                                    struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                                                    struct wl_array *indices)
{
    struct wayland_dmabuf_surface_feedback *surface_feedback = data;

    if (!surface_feedback->pending_feedback->format_table_entries &&
        (!(surface_feedback->pending_feedback->format_table_entries = surface_feedback->feedback->format_table_entries)))
    {
        WARN("Could not add formats/modifiers to tranche due to missing format table\n");
        return;
    }
    dmabuf_feedback_tranche_formats(surface_feedback->pending_feedback, zwp_linux_dmabuf_feedback_v1, indices);
}

static void surface_dmabuf_feedback_tranche_flags(void *data,
                                                  struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1,
                                                  uint32_t flags)
{
    struct wayland_dmabuf_surface_feedback *surface_feedback = data;

    surface_feedback->pending_feedback->pending_tranche.flags = flags;
}

static void surface_dmabuf_feedback_tranche_done(void *data,
                                                 struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1)
{
    struct wayland_dmabuf_surface_feedback *surface_feedback = data;

    dmabuf_feedback_tranche_done(surface_feedback->pending_feedback, zwp_linux_dmabuf_feedback_v1);
}

static void surface_dmabuf_feedback_done(void *data,
                                         struct zwp_linux_dmabuf_feedback_v1 *zwp_linux_dmabuf_feedback_v1)
{
    struct wayland_dmabuf_surface_feedback *surface_feedback = data;

    if (!surface_feedback->pending_feedback->format_table_entries)
    {
        WARN("Invalid format table: Ignoring feedback events.\n");
        dmabuf_feedback_destroy(surface_feedback->pending_feedback);
        goto out;
    }

    wayland_dmabuf_surface_feedback_lock(surface_feedback);

    if (surface_feedback->feedback)
        dmabuf_feedback_destroy(surface_feedback->feedback);

    surface_feedback->feedback = surface_feedback->pending_feedback;
    surface_feedback->surface_needs_update = TRUE;

    wayland_dmabuf_surface_feedback_unlock(surface_feedback);

out:
    surface_feedback->pending_feedback = dmabuf_feedback_create();
}

static const struct zwp_linux_dmabuf_feedback_v1_listener surface_dmabuf_feedback_listener =
{
    .main_device = surface_dmabuf_feedback_main_device,
    .format_table = surface_dmabuf_feedback_format_table,
    .tranche_target_device = surface_dmabuf_feedback_tranche_target_device,
    .tranche_formats = surface_dmabuf_feedback_tranche_formats,
    .tranche_flags = surface_dmabuf_feedback_tranche_flags,
    .tranche_done = surface_dmabuf_feedback_tranche_done,
    .done = surface_dmabuf_feedback_done,
};

/***********************************************************************
 *           wayland_dmabuf_init
 */
void wayland_dmabuf_init(struct wayland_dmabuf *dmabuf,
                         struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf_v1)
{
    dmabuf->version = wl_proxy_get_version((struct wl_proxy *)zwp_linux_dmabuf_v1);
    dmabuf->zwp_linux_dmabuf_v1 = zwp_linux_dmabuf_v1;
    wl_array_init(&dmabuf->formats);

    /* linux-dmabuf feedback events deprecate format/modifier events from
     * previous versions. Listen to pre-v4 events only if v4 is not supported. */
    if (dmabuf_has_feedback_support(dmabuf))
    {
        if (!(dmabuf->default_feedback = dmabuf_feedback_create()))
        {
            WARN("Could not create default dmabuf feedback: Memory allocation failure.\n");
            return;
        }
        dmabuf->zwp_linux_dmabuf_feedback_v1 =
            zwp_linux_dmabuf_v1_get_default_feedback(dmabuf->zwp_linux_dmabuf_v1);
        zwp_linux_dmabuf_feedback_v1_add_listener(dmabuf->zwp_linux_dmabuf_feedback_v1,
                                                  &dmabuf_feedback_listener,
                                                  dmabuf->default_feedback);
    }
    else
        zwp_linux_dmabuf_v1_add_listener(zwp_linux_dmabuf_v1, &dmabuf_listener, dmabuf);
}

/***********************************************************************
 *           wayland_dmabuf_deinit
 */
void wayland_dmabuf_deinit(struct wayland_dmabuf *dmabuf)
{
    if (dmabuf->zwp_linux_dmabuf_feedback_v1)
    {
        dmabuf_feedback_destroy(dmabuf->default_feedback);
        zwp_linux_dmabuf_feedback_v1_destroy(dmabuf->zwp_linux_dmabuf_feedback_v1);
    }

    dmabuf_format_array_release(&dmabuf->formats);

    if (dmabuf->zwp_linux_dmabuf_v1)
        zwp_linux_dmabuf_v1_destroy(dmabuf->zwp_linux_dmabuf_v1);
}

/***********************************************************************
 *           wayland_dmabuf_surface_feedback_create
 */
struct wayland_dmabuf_surface_feedback *wayland_dmabuf_surface_feedback_create(struct wayland_dmabuf *dmabuf,
                                                                               struct wl_surface *wl_surface)
{
    struct wayland_dmabuf_surface_feedback *surface_feedback = NULL;

    if (!(surface_feedback = calloc(1, sizeof(*surface_feedback))) ||
        !(surface_feedback->pending_feedback = dmabuf_feedback_create()))
    {
        WARN("Failed to create surface feedback: Memory allocation error.");
        free(surface_feedback);
        return NULL;
    }

    wayland_mutex_init(&surface_feedback->mutex, PTHREAD_MUTEX_RECURSIVE,
                       __FILE__ ": wayland_dmabuf_surface_feedback");

    surface_feedback->zwp_linux_dmabuf_feedback_v1 =
        zwp_linux_dmabuf_v1_get_surface_feedback(dmabuf->zwp_linux_dmabuf_v1, wl_surface);
    zwp_linux_dmabuf_feedback_v1_add_listener(surface_feedback->zwp_linux_dmabuf_feedback_v1,
                                              &surface_dmabuf_feedback_listener,
                                              surface_feedback);

    return surface_feedback;
}

/***********************************************************************
 *           wayland_dmabuf_surface_feedback_destroy
 */
void wayland_dmabuf_surface_feedback_destroy(struct wayland_dmabuf_surface_feedback *surface_feedback)
{
    if (surface_feedback->feedback)
        dmabuf_feedback_destroy(surface_feedback->feedback);

    dmabuf_feedback_destroy(surface_feedback->pending_feedback);
    zwp_linux_dmabuf_feedback_v1_destroy(surface_feedback->zwp_linux_dmabuf_feedback_v1);
    wayland_mutex_destroy(&surface_feedback->mutex);
    free(surface_feedback);
}

/***********************************************************************
 *           wayland_dmabuf_surface_feedback_lock
 */
void wayland_dmabuf_surface_feedback_lock(struct wayland_dmabuf_surface_feedback *surface_feedback)
{
    if (surface_feedback) wayland_mutex_lock(&surface_feedback->mutex);
}

/***********************************************************************
 *           wayland_dmabuf_surface_feedback_unlock
 */
void wayland_dmabuf_surface_feedback_unlock(struct wayland_dmabuf_surface_feedback *surface_feedback)
{
    wayland_mutex_unlock(&surface_feedback->mutex);
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
