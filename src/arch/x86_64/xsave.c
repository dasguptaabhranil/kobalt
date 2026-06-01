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

#include "xsave.h"
#include "cpuid.h"
#include "../inc/kernel.h"
#include <kmalloc.h>
#include <string.h>

size_t   g_xsave_size  = XSAVE_LEGACY_SZ;
uint64_t g_xcr0        = XCR0_X87 | XCR0_SSE;
int      g_xsave_avail = 0;

static void xcr0_write(uint64_t val)
{
    __asm__ volatile (
        "xsetbv"
        :: "c"(0U), "a"((uint32_t)val), "d"((uint32_t)(val >> 32))
        : "memory"
    );
}

void xsave_arch_init(void)
{
    if (!cpuid_has_xsave()) return;

    g_xsave_avail = 1;
    write_cr4(read_cr4() | (1ULL << 18));

    uint64_t sup = cpuid_xcr0_supported();
    uint64_t xcr0 = XCR0_X87 | XCR0_SSE;

    if (sup & XCR0_AVX)       xcr0 |= XCR0_AVX;
    if (sup & XCR0_OPMASK)    xcr0 |= XCR0_OPMASK;
    if (sup & XCR0_ZMM_HI256) xcr0 |= XCR0_ZMM_HI256;
    if (sup & XCR0_HI16_ZMM)  xcr0 |= XCR0_HI16_ZMM;
    if (sup & XCR0_PKRU)      xcr0 |= XCR0_PKRU;

    g_xcr0 = xcr0;
    xcr0_write(xcr0);

    cpuid_result_t rD = cpuid(0xDU, 0);
    g_xsave_size = (rD.ebx < XSAVE_LEGACY_SZ) ? XSAVE_LEGACY_SZ : rD.ebx;
}

void xsave_init(void)
{
    xsave_arch_init();
}

void *xsave_alloc(void)
{
    size_t total = g_xsave_size + 64 + sizeof(void *);
    void *raw = kmalloc(total);
    if (!raw) return NULL;

    uintptr_t ua = ((uintptr_t)raw + sizeof(void *) + 63U) & ~(uintptr_t)63U;
    ((void **)ua)[-1] = raw;

    xsave_zero((void *)ua);
    return (void *)ua;
}

void xsave_free(void *area)
{
    if (area) kfree(((void **)area)[-1]);
}

void xsave_zero(void *area)
{
    memset(area, 0, g_xsave_size);

    uint16_t *fcw = (uint16_t *)area;
    *fcw = 0x037FU;

    uint32_t *mxcsr = (uint32_t *)area + 6;
    *mxcsr = 0x1F80U;
}

void xsave_save(void *area)
{
    if (g_xsave_avail) {
        uint32_t lo = (uint32_t)g_xcr0;
        uint32_t hi = (uint32_t)(g_xcr0 >> 32);
        __asm__ volatile (
            "xsaveq (%0)"
            :: "r"(area), "a"(lo), "d"(hi)
            : "memory"
        );
    } else {
        __asm__ volatile (
            "fxsaveq (%0)"
            :: "r"(area)
            : "memory"
        );
    }
}

void xsave_restore(const void *area)
{
    if (g_xsave_avail) {
        uint32_t lo = (uint32_t)g_xcr0;
        uint32_t hi = (uint32_t)(g_xcr0 >> 32);
        __asm__ volatile (
            "xrstorq (%0)"
            :: "r"(area), "a"(lo), "d"(hi)
            : "memory"
        );
    } else {
        __asm__ volatile (
            "fxrstorq (%0)"
            :: "r"(area)
            : "memory"
        );
    }
}
