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

SMP_TRAMP_PHYS  equ  0x8000

TOFF_PML4       equ  0x02
TOFF_STACK      equ  0x0A
TOFF_ENTRY      equ  0x12
TOFF_CPUID      equ  0x1A
TOFF_GATE       equ  0x1E

TOFF_MINI_GDT   equ  0xA0
TOFF_MINI_GDTR  equ  0xB8
TOFF_CODE64     equ  0xC0

MINI_SEL_CODE64 equ  0x08
MINI_SEL_DATA64 equ  0x10

CR0_PE          equ  (1 << 0)
CR0_PG          equ  (1 << 31)
CR4_PAE         equ  (1 << 5)
CR4_PGE         equ  (1 << 7)
EFER_MSR        equ  0xC0000080
EFER_LME        equ  (1 << 8)
EFER_NXE        equ  (1 << 11)

bits 16
section .ap_tramp

global ap_tramp_start
global ap_tramp_end

ap_tramp_start:

    jmp  short ap_tramp_code16

    times (0x40 - 2) db 0

ap_tramp_code16:
    cli
    cld

    mov  ax, cs
    mov  ds, ax
    mov  es, ax
    mov  ss, ax
    xor  sp, sp

.gate_wait:
    mov  eax, [TOFF_GATE]
    test eax, eax
    jnz  .gate_done
    pause
    jmp  .gate_wait
.gate_done:

    lgdt [TOFF_MINI_GDTR]

    mov  eax, cr4
    or   eax, (CR4_PAE | CR4_PGE)
    mov  cr4, eax

    mov  eax, [TOFF_PML4]
    mov  cr3, eax

    mov  ecx, EFER_MSR
    rdmsr
    or   eax, (EFER_LME | EFER_NXE)
    wrmsr

    mov  eax, cr0
    or   eax, (CR0_PE | CR0_PG)
    mov  cr0, eax

    db   0x66, 0xEA
    dd   (SMP_TRAMP_PHYS + TOFF_CODE64)
    dw   MINI_SEL_CODE64

.hang16:
    hlt
    jmp  .hang16

times (TOFF_MINI_GDT - ($ - ap_tramp_start)) db 0

ap_mini_gdt:
    dq   0x0000000000000000
    dq   0x00AF9A000000FFFF
    dq   0x00CF92000000FFFF
ap_mini_gdt_end:

ap_mini_gdtr:
    dw   (ap_mini_gdt_end - ap_mini_gdt - 1)
    dd   (SMP_TRAMP_PHYS + TOFF_MINI_GDT)

times (TOFF_CODE64 - ($ - ap_tramp_start)) db 0

bits 64
ap_tramp_code64:

    mov  ax, MINI_SEL_DATA64
    mov  ds, ax
    mov  es, ax
    mov  ss, ax
    xor  ax, ax
    mov  fs, ax
    mov  gs, ax

    mov  rsp, [SMP_TRAMP_PHYS + TOFF_STACK]

    mov  edi, [SMP_TRAMP_PHYS + TOFF_CPUID]

    mov  rax, [SMP_TRAMP_PHYS + TOFF_ENTRY]

    and  rsp, -16
    sub  rsp, 8

    call rax

.hang64:
    cli
    hlt
    jmp  .hang64

ap_tramp_end:

    times (0x1000 - ($ - ap_tramp_start)) db 0
