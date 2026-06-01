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

#ifndef FLATFS_CRC_H
#define FLATFS_CRC_H

#include <stdint.h>
#include <stddef.h>

void     flatfs_crc32_init_table(void);
uint32_t flatfs_crc32_internal(const void *data, size_t len);
uint32_t flatfs_crc32_update(uint32_t crc, const void *data, size_t len);
int      flatfs_block_crc_ok(const void *blk, size_t sz);
void     flatfs_block_crc_set(void *blk, size_t sz);

#endif
