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

#ifndef FLATFS_ALLOC_H
#define FLATFS_ALLOC_H

#include "flatfs.h"

typedef struct __attribute__((packed)) flatfs_flgroup {
    uint32_t  magic;
    uint32_t  group_idx;
    uint64_t  base_blk;
    uint32_t  free_count;
    uint32_t  order_free[FLATFS_ALLOC_MAXORDER + 1];
    uint8_t   bitmap[FLATFS_GROUP_BLOCKS / 8];
    uint8_t   _pad[3728];
    uint32_t  crc32;
} flatfs_flgroup_t;

typedef char _flgroup_chk[(sizeof(flatfs_flgroup_t) == FLATFS_BLOCK_SIZE) ? 1 : -1];

flatfs_err_t flatfs_alloc_init(void);
flatfs_err_t flatfs_alloc_block(uint64_t *out);
flatfs_err_t flatfs_alloc_blocks(uint32_t count, uint64_t *out);
flatfs_err_t flatfs_free_block(uint64_t blk);
flatfs_err_t flatfs_free_blocks(uint64_t blk, uint32_t count);
void         flatfs_alloc_stats(uint64_t *total, uint64_t *free_blks,
                                 uint64_t *alloc_ops, uint64_t *free_ops);

#endif
