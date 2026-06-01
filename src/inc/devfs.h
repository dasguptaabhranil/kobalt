/* Copyright (C) 2026 Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef KOBALT_DEVFS_H
#define KOBALT_DEVFS_H

#include <stdint.h>
#include <stddef.h>

typedef uint32_t dev_t;
typedef int64_t  ssize_t;
typedef int64_t  off_t;

#define MKDEV(maj, min)   (((dev_t)(maj) << 20) | ((dev_t)(min) & 0xFFFFFu))
#define MAJOR(d)          ((unsigned)((d) >> 20))
#define MINOR(d)          ((unsigned)((d) & 0xFFFFFu))
#define NODEV             ((dev_t)0)

#define DEVFS_NAME_MAX  64u

#define DEVFS_MAJOR_MEM      1u
#define DEVFS_MAJOR_TTY      4u
#define DEVFS_MAJOR_HD       8u
#define DEVFS_MAJOR_INPUT   13u
#define DEVFS_MAJOR_SND     14u
#define DEVFS_MAJOR_TTYUSB 188u
#define DEVFS_MAJOR_VBLK   252u
#define DEVFS_MAJOR_NVME   259u

#define DEVFS_MEM_NULL     1u
#define DEVFS_MEM_ZERO     5u
#define DEVFS_MEM_RANDOM   8u
#define DEVFS_MEM_URANDOM  9u

#define DEVFS_CLASS_MEM    0x01u
#define DEVFS_CLASS_TTY    0x02u
#define DEVFS_CLASS_BLOCK  0x04u
#define DEVFS_CLASS_INPUT  0x08u
#define DEVFS_CLASS_SOUND  0x10u
#define DEVFS_CLASS_NET    0x20u
#define DEVFS_CLASS_USB    0x40u

#define DEVFS_CLASS_PRIV   0x80u

#define DEVFS_O_RDONLY  0
#define DEVFS_O_WRONLY  1
#define DEVFS_O_RDWR    2
#define DEVFS_O_NONBLOCK (1 << 11)

typedef struct devfs_ops {
    int     (*open)(void *priv, int flags);
    void    (*close)(void *priv);
    ssize_t (*read)(void *priv, void *buf, size_t n, uint64_t *pos);
    ssize_t (*write)(void *priv, const void *buf, size_t n, uint64_t *pos);
    int     (*ioctl)(void *priv, unsigned long cmd, void *arg);

    int     (*poll)(void *priv, unsigned events);
} devfs_ops_t;

int devfs_register_cdev(uint32_t major, uint32_t minor, const char *name,
                        uint32_t tykid_class, devfs_ops_t *ops, void *priv);

int devfs_register_bdev(uint32_t major, uint32_t minor, const char *name,
                        uint32_t tykid_class, devfs_ops_t *ops, void *priv);

int devfs_unregister(uint32_t major, uint32_t minor);

int devfs_mkdir(const char *name);

typedef void (*devfs_hotplug_fn)(int blkdev_idx, int present);

#endif
