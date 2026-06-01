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

static inline uint64_t xgetbv(uint32_t xcr)
{
    uint32_t lo, hi;
    __asm__ volatile ("xgetbv" : "=a"(lo), "=d"(hi) : "c"(xcr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void xsetbv(uint32_t xcr, uint64_t val)
{
    __asm__ volatile ("xsetbv" ::
        "c"(xcr), "a"((uint32_t)val), "d"((uint32_t)(val >> 32)));
}

void amx_xcr0_enable(void);
void amx_xcr0_disable(void);
