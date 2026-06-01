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

#include "cpuid.h"
#include <string.h>

cpuid_cache_t g_cpuid;

void cpuid_init(void)
{
    g_cpuid.l0        = cpuid(0, 0);
    g_cpuid.max_basic = g_cpuid.l0.eax;

    uint32_t tmp[3] = { g_cpuid.l0.ebx, g_cpuid.l0.edx, g_cpuid.l0.ecx };
    memcpy(g_cpuid.vendor, tmp, 12);
    g_cpuid.vendor[12] = '\0';

    if (g_cpuid.max_basic >= 1)
        g_cpuid.l1 = cpuid(1, 0);

    if (g_cpuid.max_basic >= 7) {
        g_cpuid.l7      = cpuid(7, 0);
        g_cpuid.l7_sub1 = cpuid(7, 1);
    }

    if (g_cpuid.max_basic >= 0xDU && cpuid_has_xsave()) {
        g_cpuid.l13_sub0 = cpuid(0x0DU, 0);
        g_cpuid.lD0      = cpuid(0x0DU, 0);
        g_cpuid.lD1      = cpuid(0x0DU, 1);
    }

    g_cpuid.lx0     = cpuid(0x80000000U, 0);
    g_cpuid.max_ext = g_cpuid.lx0.eax;

    if (g_cpuid.max_ext >= 0x80000001U)
        g_cpuid.lx1 = cpuid(0x80000001U, 0);
}
