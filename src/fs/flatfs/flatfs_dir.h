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

#ifndef FLATFS_DIR_H
#define FLATFS_DIR_H

#include "flatfs.h"

flatfs_err_t flatfs_dir_create(uint64_t parent_ino, const char *name,
                                uint16_t mode, uint16_t uid, uint16_t gid,
                                uint64_t *out_ino);
flatfs_err_t flatfs_dir_lookup(uint64_t dir_ino, const char *name,
                                uint64_t *out_ino);
flatfs_err_t flatfs_dir_insert(uint64_t dir_ino, const char *name,
                                uint64_t ino, uint8_t type);
flatfs_err_t flatfs_dir_remove(uint64_t dir_ino, const char *name);
flatfs_err_t flatfs_dir_isempty(uint64_t dir_ino, int *empty);
flatfs_err_t flatfs_dir_readdir(uint64_t dir_ino, uint32_t *pos,
                                 flatfs_dirent_info_t *out);

#endif
