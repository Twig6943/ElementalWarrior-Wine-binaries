/*
 * Wayland SHM fd
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

/* For memfd_create */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "waylanddrv.h"

static int fd_resize(int fd, off_t size)
{
    /*
     * Filesystems that do support fallocate will return EINVAL or
     * EOPNOTSUPP. In this case we need to fall back to ftruncate
     */
    errno = posix_fallocate(fd, 0, size);
    if (errno == 0)
        return 0;
    else if (errno != EINVAL && errno != EOPNOTSUPP)
        return -1;
    if (ftruncate(fd, size) < 0)
        return -1;

    return 0;
}

/**********************************************************************
 *          wayland_shmfd_create
 *
 * Creates a file descriptor representing an anonymous SHM region.
 */
int wayland_shmfd_create(const char *name, int size)
{
    int fd = memfd_create(name, MFD_CLOEXEC | MFD_ALLOW_SEALING);

    if (fd >= 0)
    {
        /* We can add this seal before calling posix_fallocate(), as
         * the file is currently zero-sized anyway.
         *
         * There is also no need to check for the return value, we
         * couldn't do anything with it anyway.
         */
        fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_SEAL);
    }

    while (TRUE)
    {
        int ret = fd_resize(fd, size);
        if (ret == 0) break;
        if (ret < 0 && errno == EINTR) continue;
        close(fd);
        return -1;
    }

    return fd;
}
