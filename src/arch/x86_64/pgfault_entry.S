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

global pgfault_entry
extern pgfault_handler

pgfault_entry:
    push    rax
    push    rbx
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    push    rbp
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15

    mov     r15, cr2

    mov     rdi, [rsp + 120]
    mov     rsi, r15
    mov     rdx, [rsp + 128]
    mov     rcx, [rsp + 64]

    and     rsp, -16

    call    pgfault_handler
