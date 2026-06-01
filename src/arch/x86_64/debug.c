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

#include "debug.h"

void debug_init(void)
{
    __asm__ volatile (
        "xorq %%rax, %%rax  \n"
        "movq %%rax, %%dr0  \n"
        "movq %%rax, %%dr1  \n"
        "movq %%rax, %%dr2  \n"
        "movq %%rax, %%dr3  \n"
        "movq %%rax, %%dr7  \n"

        "movl %0, %%eax     \n"
        "movq %%rax, %%dr6  \n"
        :: "i"(DR6_INIT)
        : "rax", "memory"
    );
}

uint64_t debug_dr6(void)
{
    uint64_t v;
    __asm__ volatile ("movq %%dr6, %0" : "=r"(v));
    return v;
}

void debug_clear_dr6(void)
{
    uint64_t v = DR6_INIT;
    __asm__ volatile ("movq %0, %%dr6" :: "r"(v) : "memory");
}

void debug_set_dr7(uint64_t val)
{
    __asm__ volatile ("movq %0, %%dr7" :: "r"(val) : "memory");
}

void debug_set_dr(int n, uintptr_t addr)
{
    switch (n) {
    case 0: __asm__ volatile ("movq %0, %%dr0" :: "r"(addr) : "memory"); break;
    case 1: __asm__ volatile ("movq %0, %%dr1" :: "r"(addr) : "memory"); break;
    case 2: __asm__ volatile ("movq %0, %%dr2" :: "r"(addr) : "memory"); break;
    case 3: __asm__ volatile ("movq %0, %%dr3" :: "r"(addr) : "memory"); break;
    }
}
