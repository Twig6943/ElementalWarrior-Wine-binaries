/*
 * Wayland remote (cross-process) rendering
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

#include "ntstatus.h"
#define WIN32_NO_STATUS

#include "waylanddrv.h"

#include "wine/debug.h"
#include "wine/server.h"

#include <inttypes.h>
#include <sys/mman.h>
#include <unistd.h>

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

enum wayland_remote_surface_message
{
    WAYLAND_REMOTE_SURFACE_MESSAGE_CREATE,
    WAYLAND_REMOTE_SURFACE_MESSAGE_DESTROY,
    WAYLAND_REMOTE_SURFACE_MESSAGE_COMMIT,
    WAYLAND_REMOTE_SURFACE_MESSAGE_DISPATCH_EVENTS,
};

struct wayland_remote_surface
{
    struct wl_list link;
    int ref;
    enum wayland_remote_surface_type type;
    struct wl_event_queue *wl_event_queue;
    struct wayland_surface *wayland_surface;
    struct wl_list buffer_list;
    struct wl_list throttle_list;
};

struct wayland_remote_buffer
{
    struct wl_list link;
    HWND hwnd;
    struct wl_buffer *wl_buffer;
    HANDLE released_event;
};

struct wayland_remote_throttle
{
    struct wl_list link;
    struct wl_callback *wl_callback;
    HANDLE event;
};

struct params_type
{
    enum wayland_remote_surface_type type;
};

struct params_buffer
{
    struct params_type params_type;
    enum wayland_remote_buffer_type buffer_type;
    int plane_count;
    HANDLE fds[4];
    uint32_t strides[4];
    uint32_t offsets[4];
    int width, height;
    int format;
    uint64_t modifier;
    HANDLE released_event;
    HANDLE throttle_event;
};

struct wayland_remote_surface_proxy
{
    HWND hwnd;
    enum wayland_remote_surface_type type;
};

static struct wayland_mutex wayland_remote_surface_mutex =
{
    PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP, 0, 0, __FILE__ ": wayland_remote_surface_mutex"
};

static struct wl_list wayland_remote_surfaces = { &wayland_remote_surfaces, &wayland_remote_surfaces };
static struct wl_list wayland_remote_buffers = { &wayland_remote_buffers, &wayland_remote_buffers};

static void wayland_remote_buffer_destroy(struct wayland_remote_buffer *remote_buffer)
{
    TRACE("remote_buffer=%p released_event=%p\n",
          remote_buffer, remote_buffer->released_event);
    if (remote_buffer->released_event)
    {
        wl_list_remove(&remote_buffer->link);
        NtSetEvent(remote_buffer->released_event, NULL);
        NtClose(remote_buffer->released_event);
    }
    else
    {
        /* Detached remote buffers are stored in the global
         * wayland_remote_buffers list, and require locking. */
        wayland_mutex_lock(&wayland_remote_surface_mutex);
        wl_list_remove(&remote_buffer->link);
        wayland_mutex_unlock(&wayland_remote_surface_mutex);
    }
    wl_buffer_destroy(remote_buffer->wl_buffer);
    free(remote_buffer);
}

static void remote_buffer_release(void *data, struct wl_buffer *buffer)
{
    struct wayland_remote_buffer *remote_buffer =
        (struct wayland_remote_buffer *) data;

    TRACE("released_event=%p\n", remote_buffer->released_event);
    wayland_remote_buffer_destroy(remote_buffer);
}

static const struct wl_buffer_listener remote_buffer_listener = {
    remote_buffer_release
};

static struct wayland_remote_buffer *wayland_remote_buffer_create(struct wayland_remote_surface *remote,
                                                                  struct wl_buffer *wl_buffer,
                                                                  HANDLE released_event)
{
    struct wayland_remote_buffer *remote_buffer = calloc(1, sizeof(*remote_buffer));
    if (!remote_buffer)
    {
        ERR("Failed to allocate memory for remote buffer\n");
        return NULL;
    }

    remote_buffer->hwnd = remote->wayland_surface->hwnd;
    remote_buffer->wl_buffer = wl_buffer;

    if (released_event)
    {
        /* Non-detached buffers are dispatched from remote surface event queue
         * so that we can dispatch events on demand (see
         * WAYLAND_REMOTE_SURFACE_MESSAGE_DISPATCH_EVENTS). */
        wl_proxy_set_queue((struct wl_proxy *) remote_buffer->wl_buffer,
                           remote->wl_event_queue);
        wl_list_insert(&remote->buffer_list, &remote_buffer->link);
        remote_buffer->released_event = released_event;
    }
    else
    {
        /* Detached buffers are dispatched from the default thread queue and
         * are stored in wayland_remote_buffers, in order to not be destroyed
         * along with their remote surface. We don't need to explicitly lock to
         * insert to this list at this point, since having a remote surface
         * implies a locked wayland_remote_surface_mutex. */
        wl_list_insert(&wayland_remote_buffers, &remote_buffer->link);
    }

    wl_buffer_add_listener(remote_buffer->wl_buffer,
                           &remote_buffer_listener, remote_buffer);

    return remote_buffer;
}

static void wayland_remote_throttle_destroy(struct wayland_remote_throttle *remote_throttle)
{
    wl_list_remove(&remote_throttle->link);

    wl_callback_destroy(remote_throttle->wl_callback);

    if (remote_throttle->event)
    {
        NtSetEvent(remote_throttle->event, NULL);
        NtClose(remote_throttle->event);
    }

    free(remote_throttle);
}

static void throttle_callback(void *data, struct wl_callback *callback, uint32_t time)
{
    struct wayland_remote_throttle *remote_throttle = data;

    TRACE("throttle_event=%p\n", remote_throttle->event);

    wayland_remote_throttle_destroy(remote_throttle);
}

static const struct wl_callback_listener throttle_listener = {
    throttle_callback
};

static struct wayland_remote_throttle *wayland_remote_throttle_create(struct wayland_remote_surface *remote,
                                                                      struct wl_callback *wl_callback,
                                                                      HANDLE throttle_event)
{
    struct wayland_remote_throttle *remote_throttle = calloc(1, sizeof(*remote_throttle));
    if (!remote_throttle)
    {
        ERR("Failed to allocate memory for remote throttle\n");
        return NULL;
    }
    remote_throttle->wl_callback = wl_callback;
    remote_throttle->event = throttle_event;

    wl_proxy_set_queue((struct wl_proxy *) remote_throttle->wl_callback,
                        remote->wl_event_queue);
    wl_callback_add_listener(remote_throttle->wl_callback, &throttle_listener,
                             remote_throttle);
    wl_list_insert(&remote->throttle_list, &remote_throttle->link);

    return remote_throttle;
}

static void wayland_remote_surface_destroy(struct wayland_remote_surface *remote)
{
    struct wayland_remote_buffer *buffer, *buffer_tmp;
    struct wayland_remote_throttle *throttle, *throttle_tmp;

    TRACE("remote=%p\n", remote);

    wl_list_remove(&remote->link);

    wl_list_for_each_safe(buffer, buffer_tmp, &remote->buffer_list, link)
        wayland_remote_buffer_destroy(buffer);

    wl_list_for_each_safe(throttle, throttle_tmp, &remote->throttle_list, link)
        wayland_remote_throttle_destroy(throttle);

    if (remote->wl_event_queue) wl_event_queue_destroy(remote->wl_event_queue);
    if (remote->wayland_surface)
    {
        switch (remote->type)
        {
        case WAYLAND_REMOTE_SURFACE_TYPE_NORMAL:
            wayland_surface_unref(remote->wayland_surface);
            break;
        case WAYLAND_REMOTE_SURFACE_TYPE_GLVK:
            wayland_surface_unref_glvk(remote->wayland_surface);
            break;
        default:
            ERR("Invalid surface type %d\n", remote->type);
            break;
        }
    }
    wayland_mutex_unlock(&wayland_remote_surface_mutex);
    free(remote);
}

static struct wayland_remote_surface *wayland_remote_surface_create(struct wayland_surface *wayland_surface,
                                                                    enum wayland_remote_surface_type type)
{
    struct wayland_remote_surface *remote;

    remote = calloc(1, sizeof(*remote));
    if (!remote)
    {
        ERR("Failed to allocate memory for remote surface hwnd=%p type=%d\n",
            wayland_surface->hwnd, type);
        goto err;
    }

    remote->ref = 1;
    remote->type = type;
    wl_list_init(&remote->buffer_list);
    wl_list_init(&remote->throttle_list);

    remote->wl_event_queue = wl_display_create_queue(wayland_surface->wayland->wl_display);
    if (!remote->wl_event_queue)
    {
        ERR("Failed to create wl_event_queue for remote surface hwnd=%p type=%d\n",
            wayland_surface->hwnd, type);
        goto err;
    }

    switch (type)
    {
    case WAYLAND_REMOTE_SURFACE_TYPE_NORMAL:
        wayland_surface_ref(wayland_surface);
        break;
    case WAYLAND_REMOTE_SURFACE_TYPE_GLVK:
        if (!wayland_surface_create_or_ref_glvk(wayland_surface))
        {
            ERR("Failed to create GL/VK for remote surface hwnd=%p type=%d\n",
                wayland_surface->hwnd, type);
            goto err;
        }
        break;
    default:
        ERR("Invalid surface type %d\n", type);
        goto err;
    }

    remote->wayland_surface = wayland_surface;

    wayland_mutex_lock(&wayland_remote_surface_mutex);
    wl_list_insert(&wayland_remote_surfaces, &remote->link);

    return remote;

err:
    if (remote) wayland_remote_surface_destroy(remote);
    return NULL;
}

static struct wayland_remote_surface *wayland_remote_surface_get(HWND hwnd,
                                                                 enum wayland_remote_surface_type type)
{
    struct wayland_remote_surface *remote;

    wayland_mutex_lock(&wayland_remote_surface_mutex);
    wl_list_for_each(remote, &wayland_remote_surfaces, link)
    {
        if (remote->wayland_surface->hwnd == hwnd && remote->type == type)
            return remote;
    }
    wayland_mutex_unlock(&wayland_remote_surface_mutex);

    return NULL;
}

static void wayland_remote_surface_release(struct wayland_remote_surface *remote)
{
    if (remote) wayland_mutex_unlock(&wayland_remote_surface_mutex);
}

static void wayland_remote_surface_ref(struct wayland_remote_surface *remote)
{
    remote->ref++;
}

static void wayland_remote_surface_unref(struct wayland_remote_surface *remote)
{
    remote->ref--;
    if (remote->ref == 0)
        wayland_remote_surface_destroy(remote);
    else
        wayland_remote_surface_release(remote);
}

static BOOL wayland_remote_surface_commit(struct wayland_remote_surface *remote,
                                          struct wayland_remote_buffer *remote_buffer,
                                          HANDLE throttle_event)
{
    BOOL ret = FALSE;
    struct wl_surface *wl_surface;

    wayland_mutex_lock(&remote->wayland_surface->mutex);

    TRACE("remote=%p wayland_surface=%p glvk=%p drawing_allowed=%d\n",
          remote, remote->wayland_surface, remote->wayland_surface->glvk,
          remote->wayland_surface->drawing_allowed);

    switch (remote->type)
    {
    case WAYLAND_REMOTE_SURFACE_TYPE_NORMAL:
        wl_surface = remote->wayland_surface->wl_surface;
        break;
    case WAYLAND_REMOTE_SURFACE_TYPE_GLVK:
        wl_surface = remote->wayland_surface->glvk ?
                     remote->wayland_surface->glvk->wl_surface : NULL;
        break;
    default:
        ERR("Invalid surface type %d\n", remote->type);
        goto out;
    }

    if (remote->wayland_surface->drawing_allowed && wl_surface)
    {
        wayland_surface_ensure_mapped(remote->wayland_surface);
        wl_surface_attach(wl_surface, remote_buffer->wl_buffer, 0, 0);
        wl_surface_damage_buffer(wl_surface, 0, 0, INT32_MAX, INT32_MAX);
        if (throttle_event &&
            !wayland_remote_throttle_create(remote, wl_surface_frame(wl_surface),
                                            throttle_event))
        {
            NtSetEvent(throttle_event, NULL);
            NtClose(throttle_event);
        }
        wl_surface_commit(wl_surface);
        ret = TRUE;
    }

out:
    wayland_mutex_unlock(&remote->wayland_surface->mutex);

    return ret;
}

static void *map_shm_from_handle(HANDLE params, size_t size)
{
    int shm_fd = -1;
    void *data = NULL;

    if (wine_server_handle_to_fd(params, FILE_READ_DATA, &shm_fd, NULL) != STATUS_SUCCESS)
    {
        ERR("Failed to get SHM fd from Wine handle.\n");
        goto out;
    }

    data = mmap(NULL, size, PROT_READ, MAP_SHARED, shm_fd, 0);
    if (data == MAP_FAILED)
    {
        ERR("Failed to map SHM fd.\n");
        data = NULL;
    }

out:
    if (shm_fd >= 0) close(shm_fd);

    return data;
}

static void wayland_remote_surface_update_wayland_surface(struct wayland_remote_surface *remote,
                                                          struct wayland_surface *wayland_surface)
{
    switch (remote->type)
    {
    case WAYLAND_REMOTE_SURFACE_TYPE_NORMAL:
        wayland_surface_ref(wayland_surface);
        wayland_surface_unref(remote->wayland_surface);
        break;
    case WAYLAND_REMOTE_SURFACE_TYPE_GLVK:
        if (!wayland_surface_create_or_ref_glvk(wayland_surface)) return;
        wayland_surface_unref_glvk(remote->wayland_surface);
        break;
    default:
        ERR("Invalid surface type %d\n", remote->type);
        return;
    }
    remote->wayland_surface = wayland_surface;
}

static void wayland_remote_surface_handle_create(struct wayland_remote_surface *remote,
                                                 struct wayland_surface *wayland_surface,
                                                 struct params_type *params)
{
    TRACE("hwnd=%p type=%d\n", wayland_surface->hwnd, params->type);

    if (remote)
    {
        wayland_remote_surface_ref(remote);
        return;
    }

    remote = wayland_remote_surface_create(wayland_surface, params->type);
    if (!remote)
    {
        ERR("Failed to create remote surface for hwnd=%p type=%d\n",
            wayland_surface->hwnd, params->type);
        return;
    }

    wayland_remote_surface_release(remote);
}

static void wayland_remote_surface_handle_destroy(struct wayland_remote_surface *remote,
                                                  struct wayland_surface *wayland_surface,
                                                  struct params_type *params)
{
    TRACE("hwnd=%p type=%d\n", wayland_surface->hwnd, params->type);

    if (!remote)
    {
        WARN("Remote surface for hwnd=%p type=%d does not exist\n",
             wayland_surface->hwnd, params->type);
        return;
    }

    wayland_remote_surface_unref(remote);
}

static BOOL _wayland_native_buffer_init_params(struct wayland_native_buffer *native,
                                               struct params_buffer *params)
{
    int i;

    native->plane_count = params->plane_count;
    native->width = params->width;
    native->height = params->height;
    native->format = params->format;
    native->modifier = params->modifier;

    for (i = 0; i < native->plane_count; i++)
        native->fds[i] = -1;

    for (i = 0; i < native->plane_count; i++)
    {
        NTSTATUS ret;

        ret = wine_server_handle_to_fd(params->fds[i], GENERIC_READ | SYNCHRONIZE,
                                       &native->fds[i], NULL);
        if (ret != STATUS_SUCCESS)
        {
            ERR("Failed to get fd from handle ret=%#x\n", (int)ret);
            goto err;
        }

        native->strides[i] = params->strides[i];
        native->offsets[i] = params->offsets[i];
    }

    return TRUE;

err:
    wayland_native_buffer_deinit(native);
    return FALSE;
}

static void wayland_remote_surface_handle_commit(struct wayland_remote_surface *remote,
                                                 struct wayland_surface *wayland_surface,
                                                 struct params_buffer *params)
{
    struct wayland_native_buffer native;
    struct wl_buffer *wl_buffer = NULL;
    struct wayland_remote_buffer *remote_buffer = NULL;

    TRACE("hwnd=%p type=%d\n", wayland_surface->hwnd, params->params_type.type);

    if (!remote)
    {
        WARN("Remote surface for hwnd=%p type=%d does not exist\n",
             wayland_surface->hwnd, params->params_type.type);
        goto err;
    }

    if (!_wayland_native_buffer_init_params(&native, params))
    {
        ERR("Failed to initialize native buffer\n");
        goto err;
    }

    switch (params->buffer_type)
    {
    case WAYLAND_REMOTE_BUFFER_TYPE_SHM:
        {
            struct wayland_shm_buffer *shm_buffer =
                wayland_shm_buffer_create_from_native(remote->wayland_surface->wayland,
                                                      &native);
            if (shm_buffer)
                wl_buffer = wayland_shm_buffer_steal_wl_buffer_and_destroy(shm_buffer);
        }
        break;
    case WAYLAND_REMOTE_BUFFER_TYPE_DMABUF:
        {
            struct wayland_dmabuf_buffer *dmabuf_buffer =
                wayland_dmabuf_buffer_create_from_native(remote->wayland_surface->wayland,
                                                         &native);
            if (dmabuf_buffer)
                wl_buffer = wayland_dmabuf_buffer_steal_wl_buffer_and_destroy(dmabuf_buffer);
        }
        break;
    default:
        ERR("Invalid buffer type %d\n", params->buffer_type);
        goto err;
    }

    wayland_native_buffer_deinit(&native);
    if (!wl_buffer)
    {
        ERR("Failed to create wl_buffer\n");
        goto err;
    }

    remote_buffer = wayland_remote_buffer_create(remote, wl_buffer, params->released_event);

    if (!wayland_remote_surface_commit(remote, remote_buffer, params->throttle_event))
        goto err;

    return;

err:
    if (params->released_event)
    {
        NtSetEvent(params->released_event, NULL);
        NtClose(params->released_event);
    }
    if (params->throttle_event)
    {
        NtSetEvent(params->throttle_event, NULL);
        NtClose(params->throttle_event);
    }
    if (remote_buffer) wayland_remote_buffer_destroy(remote_buffer);
}

static void wayland_remote_surface_handle_dispatch_events(struct wayland_remote_surface *remote,
                                                          struct wayland_surface *wayland_surface,
                                                          struct params_type *params)
{

    TRACE("hwnd=%p type=%d\n", wayland_surface->hwnd, params->type);

    if (!remote)
    {
        WARN("Remote surface for hwnd=%p type=%d does not exist\n",
             wayland_surface->hwnd, params->type);
        return;
    }

    wayland_dispatch_queue(remote->wl_event_queue, 0);
}

/**********************************************************************
 *          wayland_remote_surface_handle_message
 *
 *  Handles a message sent to our remote surface infrastructure.
 */
void wayland_remote_surface_handle_message(struct wayland_surface *wayland_surface,
                                           WPARAM message, LPARAM params_long)
{
    HANDLE params_handle = LongToHandle(params_long);
    void *params = NULL;
    size_t params_size;
    struct wayland_remote_surface *remote = NULL;

    TRACE("message=%ld params=%p\n", (long)message, params_handle);

    switch (message)
    {
    case WAYLAND_REMOTE_SURFACE_MESSAGE_CREATE:
    case WAYLAND_REMOTE_SURFACE_MESSAGE_DESTROY:
    case WAYLAND_REMOTE_SURFACE_MESSAGE_DISPATCH_EVENTS:
        params_size = sizeof(struct params_type);
        break;
    case WAYLAND_REMOTE_SURFACE_MESSAGE_COMMIT:
        params_size = sizeof(struct params_buffer);
        break;
    default:
        goto out;
    }

    params = map_shm_from_handle(params_handle, sizeof(struct params_type));
    if (!params) goto out;

    remote = wayland_remote_surface_get(wayland_surface->hwnd,
                                        ((struct params_type *) params)->type);
    if (remote)
        wayland_remote_surface_update_wayland_surface(remote, wayland_surface);

    switch (message)
    {
    case WAYLAND_REMOTE_SURFACE_MESSAGE_CREATE:
        wayland_remote_surface_handle_create(remote, wayland_surface, params);
        break;
    case WAYLAND_REMOTE_SURFACE_MESSAGE_DESTROY:
        wayland_remote_surface_handle_destroy(remote, wayland_surface, params);
        remote = NULL;
        break;
    case WAYLAND_REMOTE_SURFACE_MESSAGE_COMMIT:
        wayland_remote_surface_handle_commit(remote, wayland_surface, params);
        break;
    case WAYLAND_REMOTE_SURFACE_MESSAGE_DISPATCH_EVENTS:
        wayland_remote_surface_handle_dispatch_events(remote, wayland_surface, params);
        break;
    }

out:
    if (remote) wayland_remote_surface_release(remote);
    if (params) munmap(params, params_size);
    if (params_handle) NtClose(params_handle);
}

/**********************************************************************
 *          wayland_destroy_remote_surfaces
 *
 *  Destroys remote surfaces targeting a window.
 */
void wayland_destroy_remote_surfaces(HWND hwnd)
{
    struct wayland_remote_surface *remote, *tmp;
    struct wayland_remote_buffer *remote_buf, *tmp_buf;

    TRACE("hwnd=%p\n", hwnd);

    wayland_mutex_lock(&wayland_remote_surface_mutex);
    /* Destroy any detached remote buffers for the window. */
    wl_list_for_each_safe(remote_buf, tmp_buf, &wayland_remote_buffers, link)
    {
        if (remote_buf->hwnd == hwnd)
            wayland_remote_buffer_destroy(remote_buf);
    }
    /* Destroy any remote surfaces for the window. */
    wl_list_for_each_safe(remote, tmp, &wayland_remote_surfaces, link)
    {
        if (remote->wayland_surface->hwnd == hwnd)
        {
            /* wayland_remote_surface_destroy() unlocks the surface mutex,
             * since it assumes that that the passed remote was acquired
             * with wayland_remote_surface_get(). Lock the mutex manually
             * to maintain the proper lock count. */
            wayland_mutex_lock(&wayland_remote_surface_mutex);
            wayland_remote_surface_destroy(remote);
        }
    }
    wayland_mutex_unlock(&wayland_remote_surface_mutex);
}

static HANDLE remote_handle_from_local(HANDLE local_handle, HWND remote_hwnd)
{
    HANDLE remote_handle = 0;
    HANDLE remote_process = 0;
    DWORD remote_process_id;
    OBJECT_ATTRIBUTES attr = { .Length = sizeof(OBJECT_ATTRIBUTES) };
    CLIENT_ID cid;

    if (!NtUserGetWindowThread(remote_hwnd, &remote_process_id)) return 0;

    cid.UniqueProcess = ULongToHandle(remote_process_id);

    if (NtOpenProcess(&remote_process, PROCESS_DUP_HANDLE, &attr, &cid) ||
        !remote_process)
    {
        ERR("Failed to open process with id %#x\n", (UINT)remote_process_id);
        return 0;
    }

    if (NtDuplicateObject(GetCurrentProcess(), local_handle, remote_process,
                          &remote_handle, 0, 0, DUPLICATE_SAME_ACCESS))
    {
        ERR("Failed to duplicate handle in remote process\n");
    }

    NtClose(remote_process);

    return remote_handle;
}

static HANDLE remote_handle_from_fd(int fd, HWND remote_hwnd)
{
    HANDLE local_fd_handle = 0;
    HANDLE remote_fd_handle = 0;

    if (wine_server_fd_to_handle(fd, GENERIC_READ | SYNCHRONIZE, 0,
                                 &local_fd_handle) != STATUS_SUCCESS)
    {
        ERR("Failed to get handle from fd\n");
        goto out;
    }

    remote_fd_handle = remote_handle_from_local(local_fd_handle, remote_hwnd);

out:
    if (local_fd_handle) NtClose(local_fd_handle);

    return remote_fd_handle;
}

/**********************************************************************
 *          wayland_remote_surface_proxy_create
 *
 *  Creates a proxy for rendering to a remote surface.
 */
struct wayland_remote_surface_proxy *wayland_remote_surface_proxy_create(HWND hwnd,
                                                                         enum wayland_remote_surface_type type)
{
    int params_fd;
    struct params_type *params;
    HANDLE remote_params_handle;
    struct wayland_remote_surface_proxy *proxy;

    TRACE("hwnd=%p type=%d\n", hwnd, type);

    proxy = calloc(1, sizeof(*proxy));
    if (!proxy) return NULL;

    proxy->hwnd = hwnd;
    proxy->type = type;

    params_fd = wayland_shmfd_create("wayland-remote-surface-create-glvk", sizeof(*params));
    if (params_fd < 0) goto err;
    params = mmap(NULL, sizeof(*params), PROT_WRITE, MAP_SHARED, params_fd, 0);
    if (params == MAP_FAILED) goto err;
    params->type = proxy->type;
    munmap(params, sizeof(*params));

    remote_params_handle = remote_handle_from_fd(params_fd, hwnd);
    if (!remote_params_handle) goto err;

    NtUserPostMessage(proxy->hwnd, WM_WAYLAND_REMOTE_SURFACE,
                      WAYLAND_REMOTE_SURFACE_MESSAGE_CREATE,
                      HandleToLong(remote_params_handle));

    close(params_fd);

    TRACE("hwnd=%p type=%d => proxy=%p\n", hwnd, type, proxy);

    return proxy;

err:
    if (params_fd >= 0) close(params_fd);
    if (proxy) free(proxy);
    return NULL;
}

/**********************************************************************
 *          wayland_remote_surface_proxy_destroy
 *
 *  Destroys a proxy to a remote surface.
 */
void wayland_remote_surface_proxy_destroy(struct wayland_remote_surface_proxy *proxy)
{
    int params_fd;
    struct params_type *params;
    HANDLE remote_params_handle;

    TRACE("proxy=%p hwnd=%p type=%d\n", proxy, proxy->hwnd, proxy->type);

    params_fd = wayland_shmfd_create("wayland-remote-surface-destroy", sizeof(*params));
    if (params_fd < 0) goto out;
    params = mmap(NULL, sizeof(*params), PROT_WRITE, MAP_SHARED, params_fd, 0);
    if (params == MAP_FAILED) goto out;
    params->type = proxy->type;
    munmap(params, sizeof(*params));

    remote_params_handle = remote_handle_from_fd(params_fd, proxy->hwnd);
    if (!remote_params_handle) goto out;

    NtUserPostMessage(proxy->hwnd, WM_WAYLAND_REMOTE_SURFACE,
                      WAYLAND_REMOTE_SURFACE_MESSAGE_DESTROY,
                      HandleToLong(remote_params_handle));

out:
    if (params_fd >= 0) close(params_fd);
    free(proxy);
}

/**********************************************************************
 *          wayland_remote_surface_proxy_commit
 *
 *  Commits a dmabuf to the surface targeted by the remote surface proxy.
 *
 *  Returns a handle to an Event that will be set when the committed buffer
 *  can be reused.
 */
BOOL wayland_remote_surface_proxy_commit(struct wayland_remote_surface_proxy *proxy,
                                         struct wayland_native_buffer *native,
                                         enum wayland_remote_buffer_type buffer_type,
                                         enum wayland_remote_buffer_commit commit,
                                         HANDLE *buffer_released_event_out,
                                         HANDLE *throttle_event_out)
{
    int params_fd;
    struct params_buffer *params = MAP_FAILED;
    HANDLE local_released_event = 0;
    HANDLE local_throttle_event = 0;
    HANDLE remote_params_handle;
    OBJECT_ATTRIBUTES attr = { .Length = sizeof(attr), .Attributes = OBJ_OPENIF };
    int i;

    TRACE("proxy=%p hwnd=%p type=%d commit=%d\n",
          proxy, proxy->hwnd, proxy->type, commit);

    /* Create buffer params */
    params_fd = wayland_shmfd_create("wayland-remote-surface-commit", sizeof(*params));
    if (params_fd < 0) goto err;
    params = mmap(NULL, sizeof(*params), PROT_WRITE, MAP_SHARED, params_fd, 0);
    if (params == MAP_FAILED) goto err;

    /* Populate buffer params */
    params->params_type.type = proxy->type;
    params->buffer_type = buffer_type;
    params->plane_count = native->plane_count;
    for (i = 0; i < native->plane_count; i++)
    {
        params->fds[i] = remote_handle_from_fd(native->fds[i], proxy->hwnd);
        if (!params->fds[i]) goto err;
        params->strides[i] = native->strides[i];
        params->offsets[i] = native->offsets[i];
    }
    params->width = native->width;
    params->height = native->height;
    params->format = native->format;
    params->modifier = native->modifier;

    if (commit != WAYLAND_REMOTE_BUFFER_COMMIT_DETACHED)
    {
        if (NtCreateEvent(&local_released_event, EVENT_ALL_ACCESS, &attr, NotificationEvent, FALSE) ||
            !local_released_event)
        {
            goto err;
        }
        params->released_event = remote_handle_from_local(local_released_event, proxy->hwnd);
        if (!params->released_event) goto err;
    }

    if (commit == WAYLAND_REMOTE_BUFFER_COMMIT_THROTTLED)
    {
        if (NtCreateEvent(&local_throttle_event, EVENT_ALL_ACCESS, &attr, NotificationEvent, FALSE) ||
            !local_throttle_event)
        {
            goto err;
        }
        params->throttle_event = remote_handle_from_local(local_throttle_event, proxy->hwnd);
        if (!params->throttle_event) goto err;
    }

    /* Create remote handle for params and post message. */
    remote_params_handle = remote_handle_from_fd(params_fd, proxy->hwnd);
    if (!remote_params_handle) goto err;

    TRACE("proxy=%p hwnd=%p type=%d commit=%d => local_released=%p "
          "remote_released=%p, local_throttle=%p remote_throttle=%p\n",
          proxy, proxy->hwnd, proxy->type, commit, local_released_event,
          params->released_event, local_throttle_event, params->throttle_event);

    NtUserPostMessage(proxy->hwnd, WM_WAYLAND_REMOTE_SURFACE,
                      WAYLAND_REMOTE_SURFACE_MESSAGE_COMMIT,
                      HandleToLong(remote_params_handle));

    munmap(params, sizeof(*params));
    close(params_fd);

    if (buffer_released_event_out)
        *buffer_released_event_out = local_released_event;
    else if (local_released_event)
        NtClose(local_released_event);

    if (throttle_event_out)
        *throttle_event_out = local_throttle_event;
    else if (local_throttle_event)
        NtClose(local_throttle_event);

    return TRUE;

err:
    if (params != MAP_FAILED)
    {
        for (i = 0; i < native->plane_count; i++)
            if (params->fds[i]) NtClose(params->fds[i]);
        if (params->released_event) NtClose(params->released_event);
        munmap(params, sizeof(*params));
    }
    if (params_fd >= 0) close(params_fd);
    if (local_released_event) NtClose(local_released_event);
    if (local_throttle_event) NtClose(local_throttle_event);

    return FALSE;
}

/**********************************************************************
 *          wayland_remote_surface_proxy_dispatch_events
 *
 *  Dispatches events (e.g., buffer release events) from the remote surface.
 */
BOOL wayland_remote_surface_proxy_dispatch_events(struct wayland_remote_surface_proxy *proxy)
{
    int params_fd;
    struct params_type *params;
    HANDLE remote_params_handle;
    BOOL ret = FALSE;

    TRACE("proxy=%p hwnd=%p type=%d\n", proxy, proxy->hwnd, proxy->type);

    params_fd = wayland_shmfd_create("wayland-remote-surface-dispatch", sizeof(*params));
    if (params_fd < 0) goto out;
    params = mmap(NULL, sizeof(*params), PROT_WRITE, MAP_SHARED, params_fd, 0);
    if (params == MAP_FAILED) goto out;
    params->type = proxy->type;
    munmap(params, sizeof(*params));

    remote_params_handle = remote_handle_from_fd(params_fd, proxy->hwnd);
    if (!remote_params_handle) goto out;

    ret = NtUserPostMessage(proxy->hwnd, WM_WAYLAND_REMOTE_SURFACE,
                            WAYLAND_REMOTE_SURFACE_MESSAGE_DISPATCH_EVENTS,
                            HandleToLong(remote_params_handle));

out:
    if (params_fd >= 0) close(params_fd);
    return ret;
}
