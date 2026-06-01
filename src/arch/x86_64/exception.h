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
    uint64_t r15, r14, r13, r12;
    uint64_t rbp, rbx;
    uint64_t r11, r10, r9, r8;
    uint64_t rdi, rsi, rdx, rcx, rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip, cs, rflags;
    uint64_t rsp, ss;
} __attribute__((packed)) pt_regs_t;

void exception_init(void);
void exception_dispatch(pt_regs_t *regs);

extern void exc_entry_0(void);
extern void exc_entry_1(void);
extern void exc_entry_2(void);
extern void exc_entry_3(void);
extern void exc_entry_4(void);
extern void exc_entry_5(void);
extern void exc_entry_6(void);
extern void exc_entry_7(void);
extern void exc_entry_8(void);
extern void exc_entry_9(void);
extern void exc_entry_10(void);
extern void exc_entry_11(void);
extern void exc_entry_12(void);
extern void exc_entry_13(void);
extern void exc_entry_14(void);
extern void exc_entry_15(void);
extern void exc_entry_16(void);
extern void exc_entry_17(void);
extern void exc_entry_18(void);
extern void exc_entry_19(void);
extern void exc_entry_20(void);
extern void exc_entry_21(void);
extern void exc_entry_22(void);
extern void exc_entry_23(void);
extern void exc_entry_24(void);
extern void exc_entry_25(void);
extern void exc_entry_26(void);
extern void exc_entry_27(void);
extern void exc_entry_28(void);
extern void exc_entry_29(void);
extern void exc_entry_30(void);
extern void exc_entry_31(void);
