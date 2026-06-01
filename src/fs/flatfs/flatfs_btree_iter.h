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

#ifndef FLATFS_BTREE_ITER_H
#define FLATFS_BTREE_ITER_H

#include "flatfs_btree.h"

typedef struct flatfs_btiter {
    uint64_t        leaf_blk;
    uint16_t        pos;
    flatfs_btkey_t  current_key;
    uint64_t        current_val;
    int             done;
} flatfs_btiter_t;

flatfs_err_t flatfs_btiter_init(flatfs_btiter_t *it,
                                 const flatfs_btkey_t *from);
flatfs_err_t flatfs_btiter_next(flatfs_btiter_t *it);

#endif
