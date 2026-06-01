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

#define MSR_STAR            0xC0000081
#define MSR_LSTAR           0xC0000082
#define MSR_FMASK           0xC0000084
#define MSR_KERNEL_GSBASE   0xC0000102
#define STAR_KERN_CS        0x0008
#define STAR_USER_BASE      0x0018
#define STAR_VALUE          ( ((u64)STAR_USER_BASE << 48) | \
                              ((u64)STAR_KERN_CS   << 32) )

#define FMASK_VALUE         ((1 << 9) | (1 << 10))

.section .text

.global kposixz_enable_syscall
.type   kposixz_enable_syscall, @function
kposixz_enable_syscall:
    leaq    kposixz_syscall_entry(%rip), %rax
    movq    %rax,   %rdx
    shrq    $32,    %rdx
    movl    $MSR_LSTAR, %ecx
    wrmsr

    movl    $MSR_STAR, %ecx
    xorl    %eax, %eax
    movl    $(STAR_USER_BASE << 16 | STAR_KERN_CS), %edx
    wrmsr

    movl    $MSR_FMASK, %ecx
    movl    $FMASK_VALUE, %eax
    xorl    %edx, %edx
    wrmsr

    retq
.size kposixz_enable_syscall, . - kposixz_enable_syscall

.global kposixz_syscall_entry
.type   kposixz_syscall_entry, @function
kposixz_syscall_entry:

    swapgs
    movq    %rsp, %gs:16
    movq    %gs:8, %rsp

    pushq   %r11
    pushq   %rcx
    pushq   %r15
    pushq   %r14
    pushq   %r13
    pushq   %r12
    pushq   %rbp
    pushq   %rbx
    pushq   %r9
    pushq   %r8
    pushq   %r10
    pushq   %rdx
    pushq   %rsi
    pushq   %rdi
    pushq   %rax

    sti

    movq    %rsp, %rdi
    call    kposixz_dispatch
    cli

    addq    $(7 * 8), %rsp
    popq    %rbx
    popq    %rbp
    popq    %r12
    popq    %r13
    popq    %r14
    popq    %r15
    popq    %rcx
    popq    %r11

    movq    %gs:16, %rsp
    swapgs
    sysretq

.size kposixz_syscall_entry, . - kposixz_syscall_entry
