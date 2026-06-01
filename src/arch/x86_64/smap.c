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

#include "smap.h"
#include "cpuid.h"
#include "../inc/kernel.h"

void smap_init(void)
{
    cpuid_result_t r = cpuid(7, 0);
    uint64_t cr4 = read_cr4();

    if (r.ebx & (1U << 7))  cr4 |= (1ULL << 20);
    if (r.ebx & (1U << 20)) cr4 |= (1ULL << 21);

    write_cr4(cr4);
}
