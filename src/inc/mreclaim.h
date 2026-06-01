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

#ifndef MRECLAIM_H
#define MRECLAIM_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    MEM_PRESSURE_NONE     = 0,
    MEM_PRESSURE_LOW,
    MEM_PRESSURE_MEDIUM,
    MEM_PRESSURE_CRITICAL,
} mem_pressure_t;

typedef size_t (*mreclaim_shrinker_t)(size_t target, mem_pressure_t lvl);

#define MRECLAIM_MAX_SHRINKERS  16

void            mreclaim_init(void);
int             mreclaim_register_shrinker(mreclaim_shrinker_t fn);
void            mreclaim_unregister_shrinker(mreclaim_shrinker_t fn);
size_t          mreclaim_run(size_t target_pages);
mem_pressure_t  mreclaim_pressure(void);
void            mreclaim_set_watermarks(size_t high_pg, size_t low_pg, size_t min_pg);
void            mreclaim_account_alloc(size_t pages);
void            mreclaim_account_free(size_t pages);
size_t          mreclaim_free_pages(void);
size_t          mreclaim_total_pages(void);
void            mreclaim_set_total(size_t total_pages);

#endif
