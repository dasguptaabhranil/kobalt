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

#include <amx_sched.h>
#include <amx.h>
#include <amx_state.h>
#include <amx_mem.h>
#include <msr.h>
#include <kobalt_ident.h>
#include <kernel.h>

static inline uint64_t amx_sig(sched_thread_t *t)
{
    return (uint64_t)(uintptr_t)t ^ (uint64_t)t->vdeadline ^ KOBALT_KERNEL_IDENT;
}

static __attribute__((noreturn)) void sig_panic(sched_thread_t *t)
{
    (void)t;
    kputs("\n*** AMX context signature mismatch - halting\n");
    for (;;) __asm__ volatile ("cli; hlt" ::: "memory");
}

void amx_context_switch_out(sched_thread_t *prev)
{
    if (!prev->amx_context)
        return;
    if (prev->amx_signature != amx_sig(prev))
        sig_panic(prev);

    amx_state_save(prev->amx_context);
    wrmsr(MSR_IA32_XFD, rdmsr(MSR_IA32_XFD) | XFD_AMX_BIT);
}

void amx_context_switch_in(sched_thread_t *next)
{
    if (!next->amx_context) {
        wrmsr(MSR_IA32_XFD, rdmsr(MSR_IA32_XFD) | XFD_AMX_BIT);
        return;
    }

    if (next->amx_signature != amx_sig(next))
        sig_panic(next);

    wrmsr(MSR_IA32_XFD, rdmsr(MSR_IA32_XFD) & ~XFD_AMX_BIT);
    amx_state_restore(next->amx_context);
}

void amx_thread_free(sched_thread_t *t)
{
    if (t->amx_context_raw) {
        amx_ctx_free(t->amx_context_raw);
        t->amx_context_raw = NULL;
        t->amx_context     = NULL;
    }
}
