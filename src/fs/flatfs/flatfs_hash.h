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

#ifndef FLATFS_HASH_H
#define FLATFS_HASH_H

#include "flatfs.h"

typedef struct __attribute__((packed)) flatfs_htree_root {
    uint32_t  magic;
    uint32_t  count;
    uint32_t  depth;
    uint32_t  _pad;
    uint64_t  children[FLATFS_HTREE_FANOUT];
    uint8_t   _tail[4096 - 16 - 2048 - 4];
    uint32_t  crc32;
} flatfs_htree_root_t;

typedef char _htroot_chk[(sizeof(flatfs_htree_root_t) == FLATFS_BLOCK_SIZE) ? 1 : -1];

uint32_t     flatfs_hash_name(const char *name, size_t len);

flatfs_err_t flatfs_htree_init(uint64_t dir_ino, uint64_t root_blk);
flatfs_err_t flatfs_htree_lookup(uint64_t dir_ino, const char *name,
                                  uint64_t *out_ino);
flatfs_err_t flatfs_htree_insert(uint64_t dir_ino, const char *name,
                                  uint64_t ino, uint8_t ftype);
flatfs_err_t flatfs_htree_remove(uint64_t dir_ino, const char *name);
flatfs_err_t flatfs_htree_iterate(uint64_t dir_ino, uint32_t *pos,
                                   flatfs_dirent_info_t *out);
flatfs_err_t flatfs_htree_isempty(uint64_t dir_ino, int *empty);

#endif
