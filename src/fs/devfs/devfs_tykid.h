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

#ifndef DEVFS_CDEV_H
#define DEVFS_CDEV_H

#include "devfs.h"

devfs_file_t *devfs_cdev_open(devfs_node_t *node, int flags);
void          devfs_cdev_close(devfs_file_t *f);
ssize_t       devfs_cdev_read(devfs_file_t *f, void *buf, size_t n);
ssize_t       devfs_cdev_write(devfs_file_t *f, const void *buf, size_t n);
int           devfs_cdev_ioctl(devfs_file_t *f, unsigned long cmd, void *arg);

int     devfs_tykid_register_node(devfs_node_t *n);
int     devfs_tykid_check(devfs_node_t *node, int flags);
ssize_t devfs_tykid_entropy_read(void *buf, size_t n);

#endif
