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

extern exception_dispatch

%macro EXC_NO_EC 1
global exc_entry_%1
exc_entry_%1:
    push    qword 0
    push    qword %1
    jmp     exc_common
%endmacro

%macro EXC_EC 1
global exc_entry_%1
exc_entry_%1:
    push    qword %1
    jmp     exc_common
%endmacro

EXC_NO_EC  0
EXC_NO_EC  1
EXC_NO_EC  2
EXC_NO_EC  3
EXC_NO_EC  4
EXC_NO_EC  5
EXC_NO_EC  6
EXC_NO_EC  7
EXC_EC     8
EXC_NO_EC  9
EXC_EC    10
EXC_EC    11
EXC_EC    12
EXC_EC    13
EXC_EC    14
EXC_NO_EC 15
EXC_NO_EC 16
EXC_EC    17
EXC_NO_EC 18
EXC_NO_EC 19
EXC_NO_EC 20
EXC_EC    21
EXC_NO_EC 22
EXC_NO_EC 23
EXC_NO_EC 24
EXC_NO_EC 25
EXC_NO_EC 26
EXC_NO_EC 27
EXC_NO_EC 28
EXC_EC    29
EXC_EC    30
EXC_NO_EC 31

exc_common:
    push    rax
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    push    r8
    push    r9
    push    r10
    push    r11
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15

    mov     rdi, rsp
    call    exception_dispatch

    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rdi
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rax

    add     rsp, 16
    iretq
