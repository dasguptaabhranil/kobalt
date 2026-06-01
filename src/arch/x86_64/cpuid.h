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
#include "../inc/kernel.h"

typedef struct {
    uint32_t eax, ebx, ecx, edx;
} cpuid_result_t;

static __inline__ cpuid_result_t cpuid(uint32_t leaf, uint32_t subleaf)
{
    cpuid_result_t r;
    __asm__ volatile ("cpuid"
        : "=a"(r.eax), "=b"(r.ebx), "=c"(r.ecx), "=d"(r.edx)
        : "a"(leaf), "c"(subleaf));
    return r;
}

typedef struct {
    cpuid_result_t l0;
    cpuid_result_t l1;
    cpuid_result_t l7;
    cpuid_result_t l7_sub1;
    cpuid_result_t l13_sub0;
    cpuid_result_t lD0;
    cpuid_result_t lD1;
    cpuid_result_t lx0;
    cpuid_result_t lx1;
    uint32_t max_basic;
    uint32_t max_ext;
    char     vendor[13];
} cpuid_cache_t;

extern cpuid_cache_t g_cpuid;

void cpuid_init(void);

static __inline__ int cpuid_has_xsave(void)    { return !!(g_cpuid.l1.ecx  & (1U << 26)); }
static __inline__ int cpuid_has_avx(void)       { return !!(g_cpuid.l1.ecx  & (1U << 28)); }
static __inline__ int cpuid_has_rdrand(void)    { return !!(g_cpuid.l1.ecx  & (1U << 30)); }
static __inline__ int cpuid_has_avx512f(void)   { return !!(g_cpuid.l7.ebx  & (1U << 16)); }
static __inline__ int cpuid_has_smep(void)      { return !!(g_cpuid.l7.ebx  & (1U <<  7)); }
static __inline__ int cpuid_has_smap(void)      { return !!(g_cpuid.l7.ebx  & (1U << 20)); }
static __inline__ int cpuid_has_fsgsbase(void)  { return !!(g_cpuid.l7.ebx  & (1U <<  0)); }
static __inline__ int cpuid_has_ibrs(void)      { return !!(g_cpuid.l7.edx  & (1U << 26)); }
static __inline__ int cpuid_has_stibp(void)     { return !!(g_cpuid.l7.edx  & (1U << 27)); }
static __inline__ int cpuid_has_ssbd(void)      { return !!(g_cpuid.l7.edx  & (1U << 31)); }
static __inline__ int cpuid_has_arch_caps(void) { return !!(g_cpuid.l7.edx  & (1U << 29)); }
static __inline__ int cpuid_has_rdtscp(void)    { return !!(g_cpuid.lx1.edx & (1U << 27)); }
static __inline__ int cpuid_has_1gb_pages(void) { return !!(g_cpuid.lx1.edx & (1U << 26)); }

static __inline__ int cpuid_has_amx_bf16(void)  { return !!(g_cpuid.l7_sub1.edx & (1U << 22)); }
static __inline__ int cpuid_has_amx_tile(void)  { return !!(g_cpuid.l7_sub1.edx & (1U << 24)); }
static __inline__ int cpuid_has_amx_int8(void)  { return !!(g_cpuid.l7_sub1.edx & (1U << 25)); }

static __inline__ uint32_t cpuid_xsave_size(void)      { return g_cpuid.lD0.ebx; }
static __inline__ uint64_t cpuid_xcr0_supported(void)  { return ((uint64_t)g_cpuid.lD0.edx << 32) | g_cpuid.lD0.eax; }
