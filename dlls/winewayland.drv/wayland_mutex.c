/*
 * Wayland mutex
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

#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

/**********************************************************************
 *          wayland_mutex_init
 *
 * Initialize a wayland_mutex.
 */
void wayland_mutex_init(struct wayland_mutex *wayland_mutex, int kind,
                        const char *name)
{
    pthread_mutexattr_t mutexattr;

    pthread_mutexattr_init(&mutexattr);
    pthread_mutexattr_settype(&mutexattr, kind);
    pthread_mutex_init(&wayland_mutex->mutex, &mutexattr);
    pthread_mutexattr_destroy(&mutexattr);

    wayland_mutex->owner_tid = 0;
    wayland_mutex->lock_count = 0;
    wayland_mutex->name = name;
}

/**********************************************************************
 *          wayland_mutex_destroy
 *
 * Destroys a wayland_mutex.
 */
void wayland_mutex_destroy(struct wayland_mutex *wayland_mutex)
{
    pthread_mutex_destroy(&wayland_mutex->mutex);
    wayland_mutex->owner_tid = 0;
    wayland_mutex->lock_count = 0;
    wayland_mutex->name = NULL;
}

/**********************************************************************
 *          wayland_mutex_lock
 *
 *  Lock a mutex, emitting error messages in cases of suspected deadlock.
 *  In case of an unrecoverable error abort to ensure the program doesn't
 *  continue with an inconsistent state.
 */
void wayland_mutex_lock(struct wayland_mutex *wayland_mutex)
{
    UINT tid = GetCurrentThreadId();
    struct timespec timeout;
    int err;

    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 5;

    while (TRUE)
    {
        err = pthread_mutex_timedlock(&wayland_mutex->mutex, &timeout);
        if (!err) break;

        if (err == ETIMEDOUT)
        {
            ERR("mutex %p %s lock timed out in thread %04x, blocked by %04x, retrying (60 sec)\n",
                wayland_mutex, wayland_mutex->name, tid, wayland_mutex->owner_tid);
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_sec += 60;
        }
        else
        {
            ERR("error locking mutex %p %s errno=%d, aborting\n",
                wayland_mutex, wayland_mutex->name, errno);
            abort();
        }
    }

    wayland_mutex->owner_tid = tid;
    wayland_mutex->lock_count++;
}

/**********************************************************************
 *          wayland_mutex_unlock
 *
 *  Unlock a mutex.
 */
void wayland_mutex_unlock(struct wayland_mutex *wayland_mutex)
{
    int err;

    wayland_mutex->lock_count--;

    if (wayland_mutex->lock_count == 0)
    {
        wayland_mutex->owner_tid = 0;
    }
    else if (wayland_mutex->lock_count < 0)
    {
        ERR("mutex %p %s lock_count is %d < 0\n",
             wayland_mutex, wayland_mutex->name, wayland_mutex->lock_count);
    }

    if ((err = pthread_mutex_unlock(&wayland_mutex->mutex)))
    {
        ERR("failed to unlock mutex %p %s errno=%d\n",
            wayland_mutex, wayland_mutex->name, err);
    }
}
