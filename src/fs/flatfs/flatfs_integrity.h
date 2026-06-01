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

#ifndef FLATFS_INTEGRITY_H
#define FLATFS_INTEGRITY_H

#include "flatfs.h"

typedef struct flatfs_integrity_result {
    uint64_t  blocks_checked;
    uint64_t  crc_errors;
    uint64_t  inode_errors;
    uint64_t  btree_errors;
    uint64_t  journal_errors;
    int       passed;
} flatfs_integrity_result_t;

flatfs_err_t flatfs_integrity_check(flatfs_integrity_result_t *out);
flatfs_err_t flatfs_integrity_check_inode(uint64_t ino);
flatfs_err_t flatfs_integrity_check_block(uint64_t blk);

#endif
