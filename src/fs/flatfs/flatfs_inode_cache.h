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

#ifndef FLATFS_INODE_CACHE_H
#define FLATFS_INODE_CACHE_H

#include "flatfs.h"

#define FLATFS_ICACHE_SIZE  128u

void          flatfs_icache_init(void);
flatfs_inode_t *flatfs_icache_get(uint64_t ino);
flatfs_inode_t *flatfs_icache_insert(uint64_t ino, const flatfs_inode_t *in);
void          flatfs_icache_invalidate(uint64_t ino);
void          flatfs_icache_flush(void);
void          flatfs_icache_stats(uint64_t *hits, uint64_t *misses);

#endif
