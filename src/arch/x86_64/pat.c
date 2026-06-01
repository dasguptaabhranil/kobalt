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

#include "pat.h"
#include "msr.h"
#include "cpuid.h"
#include "../inc/kernel.h"

void pat_init(void)
{
    cpuid_result_t r = cpuid(1, 0);
    if (!(r.edx & (1U << 16))) return;

    uint64_t pat =
        ((uint64_t)PAT_WB  <<  0) |
        ((uint64_t)PAT_WT  <<  8) |
        ((uint64_t)PAT_UCM << 16) |
        ((uint64_t)PAT_UC  << 24) |
        ((uint64_t)PAT_WC  << 32) |
        ((uint64_t)PAT_WP  << 40) |
        ((uint64_t)PAT_UCM << 48) |
        ((uint64_t)PAT_UC  << 56);

    wrmsr(MSR_IA32_PAT, pat);
}
