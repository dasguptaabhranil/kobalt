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

#include "amx_init.h"
#include "amx_xcr.h"
#include <amx.h>
#include <cpuid.h>
#include <msr.h>

int      g_amx_supported = 0;
uint32_t g_amx_xsave_size = 0;

int amx_detect(void)
{
    return !!(g_cpuid.l7_sub1.edx & CPUID_AMX_TILE);
}

void amx_init_cpu(void)
{
    if (!amx_detect())
        return;

    amx_xcr0_enable();

    cpuid_result_t r = cpuid(0x0D, 0);
    g_amx_xsave_size = r.ebx;
    if (!g_amx_xsave_size)
        g_amx_xsave_size = 10368;

    wrmsr(MSR_IA32_XFD, rdmsr(MSR_IA32_XFD) | XFD_AMX_BIT);

    g_amx_supported = 1;
}

void amx_ap_init(void)
{
    if (!g_amx_supported)
        return;
    amx_xcr0_enable();
    wrmsr(MSR_IA32_XFD, rdmsr(MSR_IA32_XFD) | XFD_AMX_BIT);
}
