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

#include "fpu.h"
#include "xsave.h"
#include "idt.h"
#include "gdt.h"
#include "../../inc/percpu.h"
#include "../../inc/kernel.h"
#include <sched.h>

#define SCHED_MAX_CPUS 16

static sched_thread_t *g_fpu_owner[SCHED_MAX_CPUS];

static inline void cr0_ts_set(void)
{
    __asm__ volatile ("movq %%cr0, %%rax; orq $8, %%rax; movq %%rax, %%cr0"
                      ::: "rax", "memory");
}

static inline void cr0_ts_clear(void)
{
    __asm__ volatile ("clts" ::: "memory");
}

void fpu_init(void)
{
    xsave_arch_init();

    uint64_t cr4 = read_cr4();
    cr4 |= (1ULL << 9);
    cr4 |= (1ULL << 10);
    write_cr4(cr4);

    cr0_ts_set();

    idt_set_gate(7, (uintptr_t)fpu_nm_entry, GDT_KCODE64,
                 IDT_GATE_INTERRUPT, IDT_IST_NONE);
}

void fpu_on_switch(sched_thread_t *prev, sched_thread_t *next)
{
    uint32_t cpu = PERCPU_ID();

    (void)prev;
    (void)next;

    if (g_fpu_owner[cpu] != next)
        cr0_ts_set();
    else
        cr0_ts_clear();
}

void fpu_nm_handler(void)
{
    uint32_t cpu = PERCPU_ID();
    sched_thread_t *cur = (sched_thread_t *)percpu_get()->current_thread;

    sched_thread_t *owner = g_fpu_owner[cpu];

    if (owner && owner != cur && owner->fpu_state)
        xsave_save(owner->fpu_state);

    if (!cur->fpu_state) {
        cur->fpu_state = xsave_alloc();

    }

    if (cur->fpu_state)
        xsave_restore(cur->fpu_state);

    g_fpu_owner[cpu] = cur;
    cr0_ts_clear();
}

void fpu_save_current(void)
{
    uint32_t cpu = PERCPU_ID();
    sched_thread_t *cur = (sched_thread_t *)percpu_get()->current_thread;
    if (g_fpu_owner[cpu] == cur && cur && cur->fpu_state) {
        cr0_ts_clear();
        xsave_save(cur->fpu_state);
    }
}
