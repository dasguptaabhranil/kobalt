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
#include <stdint.h>

#define PF_PRESENT      (1u << 0)
#define PF_WRITE        (1u << 1)
#define PF_USER         (1u << 2)
#define PF_RSVD         (1u << 3)
#define PF_INSTR_FETCH  (1u << 4)
#define PF_PROT_KEY     (1u << 5)

__attribute__((noreturn))
void pgfault_handler(uint64_t error_code, uint64_t cr2,
                     uint64_t rip,        uint64_t rbp)
{
    (void)rbp;

    kputs("\n\n*** [!!] PAGE FAULT\n");
    kprintf("    CR2  (fault addr) : 0x%016llx\n", cr2);
    kprintf("    RIP  (fault instr): 0x%016llx\n", rip);

    kputs("    Type: ");
    kputs((error_code & PF_PRESENT) ? "protection-violation" : "not-present");
    kputs((error_code & PF_WRITE)   ? " | write"   : " | read");
    kputs((error_code & PF_USER)    ? " | user"    : " | kernel");
    if (error_code & PF_INSTR_FETCH) kputs(" | instr-fetch(NX)");
    if (error_code & PF_RSVD)        kputs(" | reserved-bit");
    if (error_code & PF_PROT_KEY)    kputs(" | prot-key");
    kputs("\n");

    kprintf("    Error code: 0x%08x\n", (uint32_t)error_code);

    kstack_trace();

    kputs("*** System halted.\n");
    for (;;)
        __asm__ volatile ("cli; hlt" ::: "memory");
}
