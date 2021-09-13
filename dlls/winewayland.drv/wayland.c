/*
 * Wayland core handling
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

#include "winuser.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/timerfd.h>
#include <unistd.h>

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

struct wl_display *process_wl_display = NULL;
static struct wayland *process_wayland = NULL;
static struct wayland_mutex process_wayland_mutex =
{
    PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP, 0, 0, __FILE__ ": process_wayland_mutex"
};
static struct wayland_mutex thread_wayland_mutex =
{
    PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP, 0, 0, __FILE__ ": thread_wayland_mutex"
};

static struct wl_list thread_wayland_list = {&thread_wayland_list, &thread_wayland_list};

struct wayland_callback
{
   struct wl_list link;
   uintptr_t id;
   wayland_callback_func func;
   void *data;
   uint64_t target_time_ms;
};

struct wayland_wakeup
{
    struct wl_list link;
    uintptr_t id;
    uint64_t target_time_ms;
};
static struct wl_list wayland_wakeup_list = {&wayland_wakeup_list, &wayland_wakeup_list};
static int wayland_wakeup_timerfd = -1;

/**********************************************************************
 *          Wakeup handling
 */

static void wayland_add_wakeup_for_callback(struct wayland_callback *cb)
{
    struct wayland_wakeup *wakeup;

    wakeup = calloc(1, sizeof(*wakeup));
    wakeup->target_time_ms = cb->target_time_ms;
    wakeup->id = cb->id;

    wayland_mutex_lock(&process_wayland_mutex);
    wl_list_insert(&wayland_wakeup_list, &wakeup->link);
    wayland_mutex_unlock(&process_wayland_mutex);
}

static void wayland_remove_wakeup(uintptr_t id)
{
    struct wayland_wakeup *wakeup;

    wayland_mutex_lock(&process_wayland_mutex);

    wl_list_for_each(wakeup, &wayland_wakeup_list, link)
    {
        if (wakeup->id == id)
        {
            wl_list_remove(&wakeup->link);
            free(wakeup);
            break;
        }
    }

    wayland_mutex_unlock(&process_wayland_mutex);
}

static void wayland_remove_past_wakeups(void)
{
    struct wayland_wakeup *wakeup, *tmp;
    uint64_t time_now_ms;
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    time_now_ms = ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);

    wayland_mutex_lock(&process_wayland_mutex);

    wl_list_for_each_safe(wakeup, tmp, &wayland_wakeup_list, link)
    {
        if (wakeup->target_time_ms <= time_now_ms)
        {
            wl_list_remove(&wakeup->link);
            free(wakeup);
        }
    }

    wayland_mutex_unlock(&process_wayland_mutex);
}

static void wayland_reschedule_wakeup_timerfd(void)
{
    uint64_t min = 0;
    struct itimerspec its = {0};
    struct wayland_wakeup *wakeup;

    wayland_mutex_lock(&process_wayland_mutex);

    wl_list_for_each(wakeup, &wayland_wakeup_list, link)
    {
        uint64_t wakeup_time = wakeup->target_time_ms;
        if (min == 0 || wakeup_time < min)
            min = wakeup_time;
    }

    TRACE("time=%llu\n", (long long unsigned)min);

    its.it_value.tv_sec = min / 1000;
    its.it_value.tv_nsec = (min % 1000) * 1000000;

    timerfd_settime(wayland_wakeup_timerfd, TFD_TIMER_ABSTIME, &its, NULL);

    wayland_mutex_unlock(&process_wayland_mutex);
}

/**********************************************************************
 *          xdg_wm_base handling
 */

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *shell, uint32_t serial)
{
    xdg_wm_base_pong(shell, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    xdg_wm_base_ping,
};

/**********************************************************************
 *          Seat handling
 */

static void seat_handle_capabilities(void *data, struct wl_seat *seat,
                                     enum wl_seat_capability caps)
{
    struct wayland *wayland = data;

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !wayland->pointer.wl_pointer)
    {
        wayland_pointer_init(&wayland->pointer, wayland, wl_seat_get_pointer(seat));
    }
    else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && wayland->pointer.wl_pointer)
    {
        wayland_pointer_deinit(&wayland->pointer);
    }

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !wayland->keyboard.wl_keyboard)
    {
        wayland_keyboard_init(&wayland->keyboard, wayland, wl_seat_get_keyboard(seat));
    }
    else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && wayland->keyboard.wl_keyboard)
    {
        wayland_keyboard_deinit(&wayland->keyboard);
    }
}

static void seat_handle_name(void *data, struct wl_seat *seat, const char *name)
{
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities,
    seat_handle_name,
};

/**********************************************************************
 *          Registry handling
 */

static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t id, const char *interface,
                                   uint32_t version)
{
    struct wayland *wayland = data;

    TRACE("interface=%s version=%d\n id=%u\n", interface, version, id);

    if (strcmp(interface, "wl_output") == 0)
    {
        if (!wayland_output_create(wayland, id, version))
            ERR("Failed to create wayland_output for global id=%u\n", id);
    }
    else if (strcmp(interface, "zxdg_output_manager_v1") == 0)
    {
        struct wayland_output *output;

        wayland->zxdg_output_manager_v1 =
            wl_registry_bind(registry, id, &zxdg_output_manager_v1_interface,
                             version < 3 ? version : 3);

        /* Add zxdg_output_v1 to existing outputs. */
        wl_list_for_each(output, &wayland->output_list, link)
            wayland_output_use_xdg_extension(output);
    }
    else if (strcmp(interface, "wl_shm") == 0)
    {
        wayland->wl_shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
    }

    /* The per-process wayland instance only handles output related
     * and wl_shm globals. */
    if (wayland_is_process(wayland)) return;

    if (strcmp(interface, "wl_compositor") == 0)
    {
        wayland->wl_compositor =
            wl_registry_bind(registry, id, &wl_compositor_interface, 4);
    }
    else if (strcmp(interface, "wl_subcompositor") == 0)
    {
        wayland->wl_subcompositor =
            wl_registry_bind(registry, id, &wl_subcompositor_interface, 1);
    }
    else if (strcmp(interface, "xdg_wm_base") == 0)
    {
        /* Bind version 2 so that compositors (e.g., sway) can properly send tiled
         * states, instead of falling back to (ab)using the maximized state. */
        wayland->xdg_wm_base =
            wl_registry_bind(registry, id, &xdg_wm_base_interface,
                             version < 2 ? version : 2);
        xdg_wm_base_add_listener(wayland->xdg_wm_base, &xdg_wm_base_listener, wayland);
    }
    else if (strcmp(interface, "wl_seat") == 0)
    {
        wayland->wl_seat = wl_registry_bind(registry, id, &wl_seat_interface,
                                            version < 5 ? version : 5);
        wl_seat_add_listener(wayland->wl_seat, &seat_listener, wayland);
    }
    else if (strcmp(interface, "wp_viewporter") == 0)
    {
        wayland->wp_viewporter = wl_registry_bind(registry, id, &wp_viewporter_interface, 1);
    }
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry,
                                          uint32_t id)
{
    struct wayland *wayland = data;
    struct wayland_output *output, *tmp;

    TRACE("id=%d\n", id);

    wl_list_for_each_safe(output, tmp, &wayland->output_list, link)
    {
        if (output->global_id == id)
        {
            struct wayland_surface *surface;

            TRACE("removing output->name=%s\n", output->name);

            /* Remove the output from surfaces, as some compositors don't send
             * a leave event if the output is disconnected. */
            wl_list_for_each(surface, &wayland->surface_list, link)
                wayland_surface_leave_output(surface, output);

            wayland_output_destroy(output);
            if (wayland_is_process(wayland))
            {
                /* Temporarily release the per-process instance lock, so that
                 * wayland_init_display_devices can perform more fine grained
                 * locking to avoid deadlocks. */
                wayland_process_release();
                wayland_init_display_devices();
                wayland_process_acquire();
            }
            return;
        }
    }
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove
};

/**********************************************************************
 *          wayland_init
 *
 *  Initialise a wayland instance.
 */
BOOL wayland_init(struct wayland *wayland)
{
    struct wl_display *wl_display_wrapper;
    int flags;

    TRACE("wayland=%p wl_display=%p\n", wayland, process_wl_display);

    wl_list_init(&wayland->thread_link);
    wayland->event_notification_pipe[0] = -1;
    wayland->event_notification_pipe[1] = -1;

    wayland->process_id = GetCurrentProcessId();
    wayland->thread_id = GetCurrentThreadId();
    wayland->wl_display = process_wl_display;

    if (!wayland->wl_display)
    {
        ERR("Failed to connect to wayland compositor\n");
        return FALSE;
    }

    if (!(wayland->wl_event_queue = wl_display_create_queue(wayland->wl_display)))
    {
        ERR("Failed to create event queue\n");
        return FALSE;
    }

    if (!(wl_display_wrapper = wl_proxy_create_wrapper(wayland->wl_display)))
    {
        ERR("Failed to create proxy wrapper for wl_display\n");
        return FALSE;
    }
    wl_proxy_set_queue((struct wl_proxy *) wl_display_wrapper, wayland->wl_event_queue);

    wayland->wl_registry = wl_display_get_registry(wl_display_wrapper);
    wl_proxy_wrapper_destroy(wl_display_wrapper);
    if (!wayland->wl_registry)
    {
        ERR("Failed to get to wayland registry\n");
        return FALSE;
    }

    wl_list_init(&wayland->output_list);
    wl_list_init(&wayland->detached_shm_buffer_list);
    wl_list_init(&wayland->callback_list);
    wl_list_init(&wayland->surface_list);

    /* Populate registry */
    wl_registry_add_listener(wayland->wl_registry, &registry_listener, wayland);

    /* We need three roundtrips. One to get and bind globals, one to handle all
     * initial events produced from registering the globals and one more to
     * handle potential third-order registrations. */
    if (wayland_is_process(wayland)) wayland_process_acquire();
    wl_display_roundtrip_queue(wayland->wl_display, wayland->wl_event_queue);
    wl_display_roundtrip_queue(wayland->wl_display, wayland->wl_event_queue);
    wl_display_roundtrip_queue(wayland->wl_display, wayland->wl_event_queue);
    if (wayland_is_process(wayland)) wayland_process_release();

    if (wayland_is_process(wayland))
    {
        if (option_use_system_cursors)
            wayland_cursor_theme_init(wayland);
    }
    else
    {
        /* Thread wayland instances have notification pipes to inform them when
         * there might be new events in their queues. The read part of the pipe
         * is also used as the wine server queue fd. */
        if (pipe2(wayland->event_notification_pipe, O_CLOEXEC) == -1)
            return FALSE;
        /* Make just the read end non-blocking */
        if ((flags = fcntl(wayland->event_notification_pipe[0], F_GETFL)) == -1)
            return FALSE;
        if (fcntl(wayland->event_notification_pipe[0], F_SETFL, flags | O_NONBLOCK) == -1)
            return FALSE;
        /* Keep a list of all thread wayland instances. */
        wayland_mutex_lock(&thread_wayland_mutex);
        wl_list_insert(&thread_wayland_list, &wayland->thread_link);
        wayland_mutex_unlock(&thread_wayland_mutex);
    }

    wayland->initialized = TRUE;

    return TRUE;
}

/**********************************************************************
 *          wayland_deinit
 *
 *  Deinitialise a wayland instance, releasing all associated resources.
 */
void wayland_deinit(struct wayland *wayland)
{
    struct wayland_output *output, *output_tmp;
    struct wayland_shm_buffer *shm_buffer, *shm_buffer_tmp;
    struct wayland_callback *callback, *callback_tmp;

    TRACE("%p\n", wayland);

    wayland_mutex_lock(&thread_wayland_mutex);
    wl_list_remove(&wayland->thread_link);
    wayland_mutex_unlock(&thread_wayland_mutex);

    wl_list_for_each_safe(callback, callback_tmp, &wayland->callback_list, link)
    {
        wayland_remove_wakeup(callback->id);
        wl_list_remove(&callback->link);
        free(callback);
    }
    wayland_reschedule_wakeup_timerfd();

    /* Keep getting the first surface in the list and destroy it, which also
     * removes it from the list. We use this somewhat unusual iteration method
     * because even wl_list_for_each_safe() is not safe against removals of
     * arbitrary elements from the list during iteration. */
    while (wayland->surface_list.next != &wayland->surface_list)
    {
        struct wayland_surface *surface =
            wl_container_of(wayland->surface_list.next, surface, link);
        wayland_surface_destroy(surface);
    }

    if (wayland->event_notification_pipe[0] >= 0)
        close(wayland->event_notification_pipe[0]);
    if (wayland->event_notification_pipe[1] >= 0)
        close(wayland->event_notification_pipe[1]);

    wl_list_for_each_safe(output, output_tmp, &wayland->output_list, link)
        wayland_output_destroy(output);

    wl_list_for_each_safe(shm_buffer, shm_buffer_tmp,
                          &wayland->detached_shm_buffer_list, link)
        wayland_shm_buffer_destroy(shm_buffer);

    if (wayland->pointer.wl_pointer)
        wayland_pointer_deinit(&wayland->pointer);

    if (wayland->keyboard.wl_keyboard)
        wayland_keyboard_deinit(&wayland->keyboard);

    if (wayland->wl_seat)
        wl_seat_destroy(wayland->wl_seat);

    if (wayland->wp_viewporter)
        wp_viewporter_destroy(wayland->wp_viewporter);

    if (wayland->wl_shm)
        wl_shm_destroy(wayland->wl_shm);

    if (wayland->zxdg_output_manager_v1)
        zxdg_output_manager_v1_destroy(wayland->zxdg_output_manager_v1);

    if (wayland->xdg_wm_base)
        xdg_wm_base_destroy(wayland->xdg_wm_base);

    if (wayland->wl_subcompositor)
        wl_subcompositor_destroy(wayland->wl_subcompositor);

    if (wayland->wl_compositor)
        wl_compositor_destroy(wayland->wl_compositor);

    if (wayland->wl_registry)
        wl_registry_destroy(wayland->wl_registry);

    if (wayland->wl_event_queue)
        wl_event_queue_destroy(wayland->wl_event_queue);

    wl_display_flush(wayland->wl_display);

    memset(wayland, 0, sizeof(*wayland));
}

/**********************************************************************
 *          wayland_process_init
 *
 *  Initialise the per process wayland objects.
 *
 */
BOOL wayland_process_init(void)
{
    process_wl_display = wl_display_connect(NULL);
    if (!process_wl_display)
        return FALSE;

    process_wayland = calloc(1, sizeof(*process_wayland));
    if (!process_wayland)
        return FALSE;

    wayland_wakeup_timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (!wayland_wakeup_timerfd)
        return FALSE;

    return wayland_init(process_wayland);
}

/**********************************************************************
 *          wayland_is_process
 *
 *  Checks whether a wayland instance is the per-process one.
 */
BOOL wayland_is_process(struct wayland *wayland)
{
    return wayland == process_wayland;
}

/**********************************************************************
 *          wayland_process_acquire
 *
 *  Acquires the per-process wayland instance.
 */
struct wayland *wayland_process_acquire(void)
{
    wayland_mutex_lock(&process_wayland_mutex);
    return process_wayland;
}

/**********************************************************************
 *          wayland_process_release
 *
 *  Releases the per-process wayland instance.
 */
void wayland_process_release(void)
{
    wayland_mutex_unlock(&process_wayland_mutex);
}

static void wayland_notify_threads(void)
{
    struct wayland *w;
    int ret;

    wayland_mutex_lock(&thread_wayland_mutex);

    wl_list_for_each(w, &thread_wayland_list, thread_link)
    {
        while ((ret = write(w->event_notification_pipe[1], "a", 1)) != 1)
        {
            if (ret == -1 && errno != EINTR)
            {
                ERR("failed to write to notification pipe: %s\n", strerror(errno));
                break;
            }
        }
    }

    wayland_mutex_unlock(&thread_wayland_mutex);
}

/**********************************************************************
 *          wayland_notify_wine_monitor_change
 *
 * Notify all wayland instances about a change in the state of wine monitors.
 * The notification is synchronous, this function returns after all wayland
 * instances have handled the event, except if it a thread is slow to process
 * the message, and thus likely to be blocked by this synchronous operation.
 */
void wayland_notify_wine_monitor_change(void)
{
    struct wayland *w;

    wayland_mutex_lock(&thread_wayland_mutex);

    /* Each thread maintains its own output information, so we need to notify
     * all threads about the change. We can't guarantee that all threads will
     * have windows to which we could potentially send the notification message
     * to, so we use the internal send function to target the threads directly.
     * We can't use PostThreadMessage since we require synchronous message
     * handling. */
    wl_list_for_each(w, &thread_wayland_list, thread_link)
    {
        LRESULT res;
        TRACE("notifying thread %04x\n", (UINT)w->thread_id);
        /* Use a timeout of 50ms to avoid blocking indefinitely if the
         * target thread is not processing (and to avoid deadlocks). */
        res = __wine_send_internal_message_timeout(w->process_id, w->thread_id,
                                                   WM_WAYLAND_MONITOR_CHANGE,
                                                   0, 0, 0, 50, NULL);
        /* If we weren't able to synchronously send the message, post it. */
        if (!res)
            NtUserPostThreadMessage(w->thread_id, WM_WAYLAND_MONITOR_CHANGE, 0, 0);
    }

    wayland_mutex_unlock(&thread_wayland_mutex);
}

/**********************************************************************
 *          wayland_dispatch_queue
 *
 * Dispatch events from the specified queue. If the queue is empty,
 * wait for timeout_ms for events to arrive and then dispatch any events in
 * the queue.
 *
 * Returns the number of events dispatched, -1 on error
 */
int wayland_dispatch_queue(struct wl_event_queue *queue, int timeout_ms)
{
    /* We need to poll up to two fds and notify threads of potential events:
     * 1. wl_display fd: events from the compositor
     * 2. wayland_wakeup_timerfd (per-process instance only): internally
     *    scheduled callbacks */
    struct pollfd pfd[2] = {0};
    BOOL is_process_queue = queue == process_wayland->wl_event_queue;
    int ret;

    TRACE("waiting for events with timeout=%d ...\n", timeout_ms);

    pfd[0].fd = wl_display_get_fd(process_wl_display);

    if (wl_display_prepare_read_queue(process_wl_display, queue) == -1)
    {
        if (is_process_queue) wayland_process_acquire();
        if ((ret = wl_display_dispatch_queue_pending(process_wl_display, queue)) == -1)
            TRACE("... failed wl_display_dispatch_queue_pending errno=%d\n", errno);
        if (is_process_queue) wayland_process_release();
        TRACE("... done early\n");
        return ret;
    }

    while (TRUE)
    {
        ret = wl_display_flush(process_wl_display);

        if (ret != -1 || errno != EAGAIN)
            break;

        pfd[0].events = POLLOUT;
        while ((ret = poll(pfd, 1, timeout_ms)) == -1 && errno == EINTR) continue;

        if (ret == -1)
        {
            TRACE("... failed poll out errno=%d\n", errno);
            wl_display_cancel_read(process_wl_display);
            return -1;
        }
    }

    if (ret < 0 && errno != EPIPE)
    {
        wl_display_cancel_read(process_wl_display);
        return -1;
    }

    if (is_process_queue)
    {
        pfd[1].events = POLLIN;
        pfd[1].fd = wayland_wakeup_timerfd;
    }

    pfd[0].events = POLLIN;
    pfd[0].revents = 0;
    while ((ret = poll(pfd, pfd[1].events ? 2 : 1, timeout_ms)) == -1 && errno == EINTR)
        continue;

    if (!(pfd[0].revents & POLLIN))
        wl_display_cancel_read(process_wl_display);

    if (ret == 0)
    {
        TRACE("... done => 0 events (timeout)\n");
        return 0;
    }

    if (ret == -1)
    {
        TRACE("... failed poll errno=%d\n", errno);
        return -1;
    }

    /* Handle wl_display fd input. */
    if (pfd[0].revents & POLLIN)
    {
        if (wl_display_read_events(process_wl_display) == -1)
        {
            TRACE("... failed wl_display_read_events errno=%d\n", errno);
            return -1;
        }
        if (is_process_queue) wayland_process_acquire();
        ret = wl_display_dispatch_queue_pending(process_wl_display, queue);
        if (is_process_queue) wayland_process_release();
        if (ret == -1)
        {
            TRACE("... failed wl_display_dispatch_queue_pending errno=%d\n", errno);
            return -1;
        }
    }

    /* Handle timerfd input. */
    if (pfd[1].revents & POLLIN)
    {
        uint64_t num_expirations;
        int nread;
        while ((nread = read(pfd[1].fd, &num_expirations, sizeof(uint64_t))) == -1 &&
                errno == EINTR)
        {
            continue;
        }
        if (nread < sizeof(uint64_t))
        {
            TRACE("... failed reading timerfd errno=%d\n", errno);
            return -1;
        }
        wayland_remove_past_wakeups();
        wayland_reschedule_wakeup_timerfd();
    }

    /* We may have read and queued events in queues other than the specified
     * one, so we need to notify threads (see wayland_read_events). */
    wayland_notify_threads();

    TRACE("... done => %d events\n", ret);

    return ret;
}

/**********************************************************************
 *          wayland_read_events_and_dispatch_process
 *
 * Read wayland events from the compositor, place them in their proper
 * event queues, dispatch any events for the per-process wayland instance,
 * and notify threads about the possibility of new per-thread wayland instance
 * events (without dispatching them).
 *
 * Returns whether the operation succeeded.
 */
BOOL wayland_read_events_and_dispatch_process(void)
{
    return (wayland_dispatch_queue(process_wayland->wl_event_queue, -1) != -1);
}

static void wayland_add_callback(struct wayland *wayland, struct wayland_callback *cb)
{
    struct wayland_callback *cb_iter;

    /* Keep callbacks ordered by target time and previous scheduling order */
    wl_list_for_each(cb_iter, &wayland->callback_list, link)
    {
        if (cb_iter->target_time_ms > cb->target_time_ms)
        {
            wl_list_insert(cb_iter->link.prev, &cb->link);
            break;
        }
    }

    if (wl_list_empty(&cb->link))
        wl_list_insert(wayland->callback_list.prev, &cb->link);
}

/**********************************************************************
 *          wayland_schedule_thread_callback
 *
 * Schedule a callback to be run in the context of the current thread after
 * the specified delay. If there is an existing callback with the specified
 * id, it is replaced with the new one.
 */
void wayland_schedule_thread_callback(uintptr_t id, int delay_ms,
                                      void (*callback)(void *), void *data)
{
    struct wayland *wayland = thread_wayland();
    struct timespec ts;
    struct wayland_callback *cb_iter, *cb = NULL;
    uint64_t target_ms;

    if (!wayland) return;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    target_ms = ts.tv_sec * 1000 + (ts.tv_nsec / 1000000) + delay_ms;

    TRACE("id=%p delay_ms=%d target_ms=%llu callback=%p data=%p\n",
          (void*)id, delay_ms, (long long unsigned)target_ms, callback, data);

    /* If we have a callback with the same id, we remove it from the list so
     * that it can be re-added at the appropriate position later in this
     * function. */
    wl_list_for_each(cb_iter, &wayland->callback_list, link)
    {
        if (cb_iter->id == id)
        {
            wl_list_remove(&cb_iter->link);
            cb = cb_iter;
            break;
        }
    }

    if (!cb) cb = calloc(1, sizeof(*cb));
    cb->id = id;
    cb->func = callback;
    cb->data = data;
    cb->target_time_ms = target_ms;
    wl_list_init(&cb->link);

    wayland_add_callback(wayland, cb);
    wayland_add_wakeup_for_callback(cb);
    wayland_reschedule_wakeup_timerfd();
}

/**********************************************************************
 *          wayland_cancel_thread_callback
 *
 * Cancel a callback previously scheduled in this the current thread.
 */
void wayland_cancel_thread_callback(uintptr_t id)
{
    struct wayland *wayland = thread_wayland();
    struct wayland_callback *cb;

    if (!wayland) return;

    TRACE("id=%p\n", (void*)id);

    wl_list_for_each(cb, &wayland->callback_list, link)
    {
        if (cb->id == id)
        {
            wl_list_remove(&cb->link);
            free(cb);
            break;
        }
    }
}

static void wayland_dispatch_thread_callbacks(struct wayland *wayland)
{
    struct wayland_callback *cb, *tmp;
    struct wl_list tmp_list;
    uint64_t time_now_ms;
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    time_now_ms = ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);

    /* Invoking a callback may result in scheduling additional callbacks.
     * This can corrupt our callback list iteration, so we first move
     * the current callbacks to a separate list and iterate over that. */
    wl_list_init(&tmp_list);
    wl_list_insert_list(&tmp_list, &wayland->callback_list);
    wl_list_init(&wayland->callback_list);

    /* Call all triggered callbacks and free them. */
    wl_list_for_each_safe(cb, tmp, &tmp_list, link)
    {
        if (time_now_ms < cb->target_time_ms) break;
        TRACE("invoking callback id=%p func=%p target_time_ms=%llu\n",
              (void*)cb->id, cb->func, (long long unsigned)cb->target_time_ms);
        cb->func(cb->data);
        wl_list_remove(&cb->link);
        free(cb);
    }

    /* Add untriggered callbacks back to the main list (which may now
     * have new callbacks added from a callback invocation above). */
    wl_list_for_each_safe(cb, tmp, &tmp_list, link)
    {
        wl_list_remove(&cb->link);
        wl_list_init(&cb->link);
        wayland_add_callback(wayland, cb);
    }
}

static int wayland_dispatch_thread_pending(struct wayland *wayland)
{
    char buf[64];

    TRACE("wayland=%p queue=%p\n", wayland, wayland->wl_event_queue);

    wl_display_flush(wayland->wl_display);

    /* Consume notifications */
    while (TRUE)
    {
        int ret = read(wayland->event_notification_pipe[0], buf, sizeof(buf));
        if (ret > 0) continue;
        if (ret == -1)
        {
            if (errno == EINTR) continue;
            if (errno == EAGAIN) break; /* no data to read */
            ERR("failed to read from notification pipe: %s\n", strerror(errno));
            break;
        }
        if (ret == 0)
        {
            ERR("failed to read from notification pipe: pipe is closed\n");
            break;
        }
    }

    return wl_display_dispatch_queue_pending(wayland->wl_display,
                                             wayland->wl_event_queue);
}

static BOOL wayland_process_thread_events(struct wayland *wayland, DWORD mask)
{
    int dispatched;

    wayland->last_dispatch_mask = 0;
    wayland->processing_events = TRUE;

    wayland_dispatch_thread_callbacks(wayland);

    dispatched = wayland_dispatch_thread_pending(wayland);

    wayland->processing_events = FALSE;

    TRACE("dispatched=%d mask=%s%s%s%s%s%s%s\n",
          dispatched,
          (wayland->last_dispatch_mask & QS_KEY) ? "QS_KEY|" : "",
          (wayland->last_dispatch_mask & QS_MOUSEMOVE) ? "QS_MOUSEMOVE|" : "",
          (wayland->last_dispatch_mask & QS_MOUSEBUTTON) ? "QS_MOUSEBUTTON|" : "",
          (wayland->last_dispatch_mask & QS_INPUT) ? "QS_INPUT|" : "",
          (wayland->last_dispatch_mask & QS_PAINT) ? "QS_PAINT|" : "",
          (wayland->last_dispatch_mask & QS_POSTMESSAGE) ? "QS_POSTMESSAGE|" : "",
          (wayland->last_dispatch_mask & QS_SENDMESSAGE) ? "QS_SENDMESSAGE|" : "");

    return wayland->last_dispatch_mask & mask;
}

/***********************************************************************
 *           WAYLAND_ProcessEvents
 */
BOOL WAYLAND_ProcessEvents(DWORD mask)
{
    struct wayland *wayland = thread_wayland();

    if (!wayland) return FALSE;

    if (wayland->processing_events)
    {
        wl_display_flush(wayland->wl_display);
        return FALSE;
    }

    return wayland_process_thread_events(wayland, mask);
}
