; Copyright (C) 2026 Abhranil Dasgupta
; SPDX-License-Identifier: GPL-3.0-only
;
; This program is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, version 3 of the License.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program. If not, see <https://www.gnu.org/licenses/>.

bits 64
section .text

extern gdt_tss_set_rsp0
extern sched_thread_exit

THREAD_KERNEL_RSP   equ  0
THREAD_KSTACK_TOP   equ  8

global sched_context_switch
sched_context_switch:
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15

    mov     [rdi + THREAD_KERNEL_RSP], rsp
    mov     rsp, [rsi + THREAD_KERNEL_RSP]

    push    rsi
    mov     rdi, [rsi + THREAD_KSTACK_TOP]
    call    gdt_tss_set_rsp0
    pop     rsi

    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

global sched_thread_enter
sched_thread_enter:
    sti
    mov     rdi, r12
    call    rbx
    call    sched_thread_exit
.halt:
    cli
    hlt
    jmp     .halt
