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

#include "speculation.h"
#include "cpuid.h"

spec_state_t g_spec;

void speculation_init(void)
{
    g_spec.has_ibrs      = (uint8_t)cpuid_has_ibrs();
    g_spec.has_ibpb      = g_spec.has_ibrs;
    g_spec.has_stibp     = (uint8_t)cpuid_has_stibp();
    g_spec.has_ssbd      = (uint8_t)cpuid_has_ssbd();
    g_spec.has_arch_caps = (uint8_t)cpuid_has_arch_caps();

    g_spec.has_l1d_flush = (uint8_t)!!(g_cpuid.l7.edx & (1U << 28));

    if (g_spec.has_arch_caps) {
        g_spec.arch_caps      = rdmsr(MSR_IA32_ARCH_CAPS);
        g_spec.rdcl_no        = !!(g_spec.arch_caps & ARCH_CAPS_RDCL_NO);
        g_spec.mds_no         = !!(g_spec.arch_caps & ARCH_CAPS_MDS_NO);
        g_spec.taa_no         = !!(g_spec.arch_caps & ARCH_CAPS_TAA_NO);
        g_spec.ibrs_always_on = !!(g_spec.arch_caps & ARCH_CAPS_IBRS_ALL);
    }

    uint64_t sc = 0;

    if (g_spec.has_ibrs && !g_spec.ibrs_always_on)
        sc |= SPEC_CTRL_IBRS;

    if (g_spec.has_stibp)
        sc |= SPEC_CTRL_STIBP;

    if (g_spec.has_ssbd)
        sc |= SPEC_CTRL_SSBD;

    if (sc)
        wrmsr(MSR_IA32_SPEC_CTRL, sc);
}
