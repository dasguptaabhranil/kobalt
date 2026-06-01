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

#include "signal_frame.h"
#include "xsave.h"
#include "fpu.h"
#include "../../inc/uaccess.h"
#include "../../inc/kernel.h"
#include <string.h>

#ifndef ALIGN_UP
#define ALIGN_UP(x, a)  (((x) + (a) - 1) & ~((a) - 1))
#endif

void signal_frame_init(void) {}

static void fill_mcontext(mcontext_t *mc, pt_regs_t *r)
{
    mc->r8     = r->r8;
    mc->r9     = r->r9;
    mc->r10    = r->r10;
    mc->r11    = r->r11;
    mc->r12    = r->r12;
    mc->r13    = r->r13;
    mc->r14    = r->r14;
    mc->r15    = r->r15;
    mc->rdi    = r->rdi;
    mc->rsi    = r->rsi;
    mc->rbp    = r->rbp;
    mc->rbx    = r->rbx;
    mc->rdx    = r->rdx;
    mc->rax    = r->rax;
    mc->rcx    = r->rcx;
    mc->rsp    = r->rsp;
    mc->rip    = r->rip;
    mc->rflags = r->rflags;
    mc->cs     = (uint16_t)r->cs;
    mc->ss     = (uint16_t)r->ss;
    mc->gs     = 0;
    mc->fs     = 0;
    mc->err    = r->error_code;
    mc->trapno = r->vector;
    mc->cr2    = 0;
}

static void restore_mcontext(pt_regs_t *r, const mcontext_t *mc)
{
    r->r8      = mc->r8;
    r->r9      = mc->r9;
    r->r10     = mc->r10;
    r->r11     = mc->r11;
    r->r12     = mc->r12;
    r->r13     = mc->r13;
    r->r14     = mc->r14;
    r->r15     = mc->r15;
    r->rdi     = mc->rdi;
    r->rsi     = mc->rsi;
    r->rbp     = mc->rbp;
    r->rbx     = mc->rbx;
    r->rdx     = mc->rdx;
    r->rax     = mc->rax;
    r->rcx     = mc->rcx;
    r->rsp     = mc->rsp;
    r->rip     = mc->rip;
    r->rflags  = (mc->rflags & 0x3FFFFULL) | 0x200ULL;
    r->cs      = mc->cs | 3;
    r->ss      = mc->ss | 3;
}

uintptr_t sigframe_push(pt_regs_t *regs, uintptr_t user_rsp,
                        int sig, uintptr_t handler)
{
    (void)sig;

    size_t fpu_sz  = ALIGN_UP(g_xsave_size, 64U);
    size_t total   = sizeof(sigframe_t) + fpu_sz;

    uintptr_t frame_top = user_rsp - SIGNAL_RED_ZONE;
    uintptr_t frame_va  = (frame_top - total) & ~(uintptr_t)(SIGNAL_FRAME_ALIGN - 1);

    sigframe_t kframe;
    memset(&kframe, 0, sizeof(kframe));
    fill_mcontext(&kframe.uc.uc_mcontext, regs);

    if (copy_to_user((void *)frame_va, &kframe, sizeof(kframe)) != 0)
        return 0;

    fpu_save_current();
    sched_thread_t *cur = sched_current();
    if (cur->fpu_state) {
        uintptr_t fpu_uva = frame_va + sizeof(sigframe_t);
        if (copy_to_user((void *)fpu_uva, cur->fpu_state, g_xsave_size) != 0)
            return 0;
    }

    uintptr_t ret_va = frame_va - 8;
    uint8_t tramp[8] = {0x48,0xc7,0xc0,0x0f,0x00,0x00,0x00,0x0f};
    uint8_t tramp2   = 0x05;
    copy_to_user((void *)ret_va,       tramp, 8);
    copy_to_user((void *)(ret_va + 8), &tramp2, 1);

    regs->rip  = handler;
    regs->rdi  = (uint64_t)sig;
    regs->rsi  = 0;
    regs->rdx  = frame_va + offsetof(sigframe_t, uc);
    regs->rsp  = ret_va;
    regs->rflags &= ~(1ULL << 8);

    return frame_va;
}

int sigframe_restore(uintptr_t frame_uva, pt_regs_t *regs)
{
    sigframe_t kframe;
    if (copy_from_user(&kframe, (const void *)frame_uva, sizeof(kframe)) != 0)
        return -1;

    restore_mcontext(regs, &kframe.uc.uc_mcontext);

    sched_thread_t *cur = sched_current();
    if (cur->fpu_state) {
        uintptr_t fpu_uva = frame_uva + sizeof(sigframe_t);
        if (copy_from_user(cur->fpu_state, (const void *)fpu_uva, g_xsave_size) != 0)
            return -1;
    }

    return 0;
}
