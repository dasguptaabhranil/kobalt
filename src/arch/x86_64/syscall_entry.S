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

extern syscall_dispatch

global syscall_entry
syscall_entry:
    swapgs
    mov     qword [gs:56], rsp
    mov     rsp, qword [gs:16]

    push    r11
    push    rcx
    push    r15
    push    r14
    push    r13
    push    r12
    push    rbp
    push    rbx
    push    r9
    push    r8
    push    r10
    push    rdx
    push    rsi
    push    rdi
    push    rax

    sti

    mov     rdi, rsp
    call    syscall_dispatch

    cli

    add     rsp, 56
    pop     rbx
    pop     rbp
    pop     r12
    pop     r13
    pop     r14
    pop     r15
    pop     rcx
    pop     r11

    mov     rsp, qword [gs:56]
    swapgs
    sysretq
