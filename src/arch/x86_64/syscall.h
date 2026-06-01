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

typedef struct {
    int64_t  nr;
    uint64_t arg1, arg2, arg3, arg4, arg5, arg6;
    uint64_t rbx, rbp, r12, r13, r14, r15;
    uint64_t user_rip;
    uint64_t user_rflags;
} syscall_frame_t;

#define SYSCALL_STAR_KERN  0x0008U
#define SYSCALL_STAR_USER  0x0010U

#define SYSCALL_FMASK  ((1U << 9) | (1U << 10))

void syscall_init(void);

int64_t syscall_dispatch(syscall_frame_t *frame);

extern void syscall_entry(void);
