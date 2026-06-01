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

#ifndef FLATFS_INODE_H
#define FLATFS_INODE_H

#include "flatfs.h"

flatfs_err_t flatfs_inode_read(uint64_t ino, flatfs_inode_t *out);
flatfs_err_t flatfs_inode_write(const flatfs_inode_t *in);
flatfs_err_t flatfs_inode_alloc(uint64_t *out_ino);
flatfs_err_t flatfs_inode_free(uint64_t ino);
flatfs_err_t flatfs_inode_init(flatfs_inode_t *in, uint64_t ino,
                                uint16_t mode, uint16_t uid, uint16_t gid);
flatfs_err_t flatfs_inode_getattr(uint64_t ino, flatfs_stat_t *out);
flatfs_err_t flatfs_inode_setattr(uint64_t ino, const flatfs_stat_t *attr,
                                   uint32_t mask);

#define FLATFS_ATTR_MODE   (1u << 0)
#define FLATFS_ATTR_UID    (1u << 1)
#define FLATFS_ATTR_GID    (1u << 2)
#define FLATFS_ATTR_SIZE   (1u << 3)
#define FLATFS_ATTR_ATIME  (1u << 4)
#define FLATFS_ATTR_MTIME  (1u << 5)

#endif
