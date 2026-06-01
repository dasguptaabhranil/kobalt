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

#include "amx_state.h"
#include <amx.h>
#include <sched.h>
#include <msr.h>
#include <amx_mem.h>
#include <kobalt_ident.h>
#include <tykid.h>
#include <kernel.h>

extern tykid_gate_ctx_t *tykid_kobalt_get_ctx(void);

void amx_state_save(void *ctx)
{
    amx_xsaves(ctx);
}

void amx_state_restore(void *ctx)
{
    amx_xrstors(ctx);
}

void amx_nm_handler(void)
{
    uint64_t xfd_err = rdmsr(MSR_IA32_XFD_ERR);
    if (!(xfd_err & XFD_AMX_BIT))
        return;

    sched_thread_t *t = sched_current();

    if (!t->amx_permitted)
        goto kill;

    if (tykid_recheck_all(tykid_kobalt_get_ctx()) != TYKID_OK)
        goto kill;

    if (t->amx_context)
        goto arm_off;

    void *raw = NULL;
    void *ctx = amx_ctx_alloc(&raw);
    if (!ctx)
        goto kill;

    amx_ctx_init(ctx);
    t->amx_context_raw = raw;
    t->amx_context     = ctx;
    t->amx_signature   = (uint64_t)(uintptr_t)t
                         ^ (uint64_t)t->vdeadline
                         ^ KOBALT_KERNEL_IDENT;

arm_off:
    wrmsr(MSR_IA32_XFD, rdmsr(MSR_IA32_XFD) & ~XFD_AMX_BIT);
    wrmsr(MSR_IA32_XFD_ERR, 0);
    return;

kill:
    sched_kill_current();
    __builtin_unreachable();
}
