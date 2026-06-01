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

typedef struct {
    volatile uint64_t  wall_clock_ns;
    volatile uint64_t  monotonic_ns;
    volatile uint64_t  tsc_freq_hz;
    volatile uint32_t  seq;
    volatile uint32_t  _pad;
} vdso_data_t;

extern vdso_data_t g_vdso_data;

void vdso_init(void);

void vdso_update_clocks(uint64_t wall_ns, uint64_t mono_ns);

extern uint8_t __vdso_start[];
extern uint8_t __vdso_end[];

static inline size_t vdso_size(void)
{
    return (size_t)(__vdso_end - __vdso_start);
}
