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

#ifndef FLATFS_FILE_H
#define FLATFS_FILE_H

#include "flatfs.h"

flatfs_err_t flatfs_file_read_blk(const flatfs_inode_t *in, uint64_t lblk,
                                   void *buf);
flatfs_err_t flatfs_file_write_blk(flatfs_inode_t *in, uint64_t lblk,
                                    const void *data);
flatfs_err_t flatfs_file_truncate(flatfs_inode_t *in, uint64_t new_sz);

flatfs_err_t flatfs_file_pread(const flatfs_inode_t *in, uint64_t off,
                                void *buf, size_t len, size_t *nr);
flatfs_err_t flatfs_file_pwrite(flatfs_inode_t *in, uint64_t off,
                                 const void *buf, size_t len, size_t *nw);

#endif
