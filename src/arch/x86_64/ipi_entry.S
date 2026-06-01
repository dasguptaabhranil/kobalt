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

extern ipi_handle_tlb_flush
extern ipi_handle_func_call
extern ipi_handle_halt
extern ipi_handle_resched

%macro IPI_SAVE 0
    push    rax
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    push    r8
    push    r9
    push    r10
    push    r11
%endmacro

%macro IPI_RESTORE 0
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rdi
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rax
%endmacro

%macro IPI_ENTRY 2
global %1
%1:
    IPI_SAVE
    call    %2
    IPI_RESTORE
    iretq
%endmacro

IPI_ENTRY ipi_entry_tlb_flush,  ipi_handle_tlb_flush

IPI_ENTRY ipi_entry_func_call,  ipi_handle_func_call

IPI_ENTRY ipi_entry_halt,       ipi_handle_halt

IPI_ENTRY ipi_entry_resched,    ipi_handle_resched
