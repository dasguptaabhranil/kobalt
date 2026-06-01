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
#include <stddef.h>
#include "exception.h"

typedef struct {
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rdi, rsi, rbp, rbx, rdx, rax, rcx, rsp;
    uint64_t rip, rflags;
    uint16_t cs, gs, fs, ss;
    uint64_t err, trapno;
    uint64_t oldmask, cr2;
} mcontext_t;

typedef struct {
    uint64_t  uc_flags;
    uint64_t  uc_link;
    uint64_t  uc_stack_ss_sp;
    uint64_t  uc_stack_ss_flags;
    uint64_t  uc_stack_ss_size;
    mcontext_t uc_mcontext;
    uint64_t  uc_sigmask;
} ucontext_t;

#define SIGNAL_RED_ZONE   128U
#define SIGNAL_FRAME_ALIGN 16U

typedef struct {
    ucontext_t  uc;
    uint8_t     fpu_state[];
} sigframe_t;

uintptr_t sigframe_push(pt_regs_t *regs, uintptr_t user_rsp,
                        int sig, uintptr_t handler);

int sigframe_restore(uintptr_t frame_uva, pt_regs_t *regs);
