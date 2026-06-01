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

#include <ktrace.h>
#include <kernel.h>
#include <stddef.h>

#define KTRACE_MAX_FRAMES   32u

#define KTRACE_KBASE  ((uint64_t)0x200000u)

static void print_frame(unsigned int depth, uint64_t addr)
{

    kputs("  #");

    if (depth >= 10u) {
        char d[3] = {
            (char)('0' + depth / 10u),
            (char)('0' + depth % 10u),
            '\0'
        };
        kputs(d);
    } else {
        char d[2] = { (char)('0' + depth), '\0' };
        kputs(d);
    }

    kputs("  0x");

    static const char hex[] = "0123456789abcdef";
    char buf[17];
    buf[16] = '\0';
    for (int i = 15; i >= 0; i--) {
        buf[i] = hex[addr & 0xFu];
        addr >>= 4;
    }
    kputs(buf);
    kputs("\n");
}

void kstack_trace_from(uint64_t rbp)
{
    kputs("Stack trace:\n");

    for (unsigned int depth = 0; depth < KTRACE_MAX_FRAMES; depth++) {

        if (rbp == 0u || rbp < KTRACE_KBASE)
            break;

        if (rbp & 0x7u)
            break;

        uint64_t ret_addr = *((volatile uint64_t *)rbp + 1);

        if (ret_addr < KTRACE_KBASE)
            break;

        print_frame(depth, ret_addr);

        rbp = *(volatile uint64_t *)rbp;
    }
}

void kstack_trace(void)
{
    uint64_t rbp;

    __asm__ volatile ("mov %%rbp, %0" : "=r"(rbp) :: "memory");
    kstack_trace_from(rbp);
}
