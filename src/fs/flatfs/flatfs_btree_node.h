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

#ifndef FLATFS_BTREE_NODE_H
#define FLATFS_BTREE_NODE_H

#include "flatfs_btree.h"

flatfs_btnode_t *btnode_load(uint64_t blk);
flatfs_err_t     btnode_flush(flatfs_btnode_t *n);
flatfs_err_t     btnode_alloc(flatfs_btnode_t **out, uint64_t *out_blk);
flatfs_err_t     btnode_free(uint64_t blk);
void             btnode_init(flatfs_btnode_t *n, uint64_t blk,
                              uint64_t parent, int is_leaf, uint32_t level);

int btkey_cmp(const flatfs_btkey_t *a, const flatfs_btkey_t *b);

uint16_t btnode_lower_bound(const flatfs_btnode_t *n, const flatfs_btkey_t *k);

void btnode_leaf_insert_at(flatfs_btnode_t *n, uint16_t pos,
                            const flatfs_btkey_t *k, uint64_t val);

void btnode_internal_insert_at(flatfs_btnode_t *n, uint16_t pos,
                                const flatfs_btkey_t *k, uint64_t child);

void btnode_remove_at(flatfs_btnode_t *n, uint16_t pos);

#endif
