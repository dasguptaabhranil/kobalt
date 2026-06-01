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

#ifndef FLATFS_BTREE_BALANCE_H
#define FLATFS_BTREE_BALANCE_H

#include "flatfs_btree_node.h"

flatfs_err_t btree_split(flatfs_btnode_t *n, uint64_t parent_blk,
                          flatfs_btkey_t *out_sep, uint64_t *out_sib_blk);
flatfs_err_t btree_merge(flatfs_btnode_t *ln, flatfs_btnode_t *rn,
                          const flatfs_btkey_t *sep);

#endif
