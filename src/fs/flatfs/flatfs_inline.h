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

#ifndef FLATFS_INLINE_H
#define FLATFS_INLINE_H

#include "flatfs.h"

int          flatfs_inline_fits(size_t sz);
flatfs_err_t flatfs_inline_read(const flatfs_inode_t *in, uint64_t off,
                                 void *buf, size_t len, size_t *nr);
flatfs_err_t flatfs_inline_write(flatfs_inode_t *in, uint64_t off,
                                  const void *buf, size_t len, size_t *nw);
flatfs_err_t flatfs_inline_to_block(flatfs_inode_t *in);

#endif
