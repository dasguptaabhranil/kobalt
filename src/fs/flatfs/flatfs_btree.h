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

#ifndef FLATFS_BTREE_H
#define FLATFS_BTREE_H

#include "flatfs.h"

typedef struct __attribute__((packed)) flatfs_btkey {
    uint64_t  hash;
    uint64_t  ino;
} flatfs_btkey_t;

typedef struct __attribute__((packed)) flatfs_btnode {
    uint32_t        magic;
    uint16_t        is_leaf;
    uint16_t        count;
    uint32_t        level;
    uint32_t        _pad0;
    uint64_t        self_blk;
    uint64_t        parent_blk;
    uint64_t        next_leaf;
    uint64_t        prev_leaf;
    uint8_t         _hpad[8];

    flatfs_btkey_t  keys[FLATFS_BTREE_ORDER];
    union {
        uint64_t    values[FLATFS_BTREE_ORDER];
        uint64_t    children[FLATFS_BTREE_ORDER + 1];
    };

    uint8_t _tail[1148];
    uint32_t crc32;
} flatfs_btnode_t;

typedef char _btnode_chk[(sizeof(flatfs_btnode_t) == FLATFS_BLOCK_SIZE) ? 1 : -1];

flatfs_err_t flatfs_btree_init(uint64_t root_blk);
flatfs_err_t flatfs_btree_lookup(uint64_t hash, uint64_t ino, uint64_t *val);
flatfs_err_t flatfs_btree_insert(uint64_t hash, uint64_t ino, uint64_t val);
flatfs_err_t flatfs_btree_delete(uint64_t hash, uint64_t ino);
flatfs_err_t flatfs_btree_verify(void);
void         flatfs_btree_stats(uint64_t *lookups, uint64_t *inserts,
                                 uint64_t *deletes, uint64_t *splits);

#endif
