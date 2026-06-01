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

#include <kernel.h>

extern uint8_t _kernel_seal_start;
extern uint8_t _kernel_seal_end;

void mm_seal(void)
{
    const uintptr_t seal_start = (uintptr_t)&_kernel_seal_start;
    const uintptr_t seal_end   = (uintptr_t)&_kernel_seal_end;

    if (seal_end <= seal_start)
        return;

    const uint32_t first_page = (uint32_t)(seal_start / HUGE_PAGE_SIZE);
    const uint32_t last_page  = (uint32_t)(seal_end   / HUGE_PAGE_SIZE);

    for (uint32_t i = first_page; i < last_page && i < PD_ENTRIES_COUNT; i++) {

        uint64_t *pd  = (uint64_t *)(PD_BASE_ADDR
                                     + (uintptr_t)((i / 512U) * 4096U));
        uint32_t  idx = i % 512U;

        pd[idx] &= ~(uint64_t)PTE_W;
    }

    write_cr0(read_cr0() | (1ULL << 16));
    tlb_flush_all();
}
