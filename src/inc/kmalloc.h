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

#pragma once

#include <stdint.h>
#include <stddef.h>

#define KMALLOC_MIN_SIZE          16U
#define KMALLOC_LARGE_THRESHOLD   4096U

void  kmalloc_init(void);
void *kmalloc(size_t size);
void *kmalloc_node(size_t size, uint32_t node);
void  kfree(void *ptr);
void  kmalloc_stats_node(uint32_t node, unsigned int cache_idx,
                         size_t *out_total, size_t *out_free);
void  kmalloc_stats(unsigned int cache_idx,
                    size_t *out_total, size_t *out_free);
