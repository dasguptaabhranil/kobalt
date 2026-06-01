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

MB2_HDR_MAGIC       equ  0xE85250D6
MB2_HDR_ARCH_I386   equ  0x00000000
MB2_LOADER_MAGIC    equ  0x36D76289

XEN_ELFNOTE_PHYS32_ENTRY  equ  18

PML4_BASE   equ  0x1000
PDPT_BASE   equ  0x2000
PD0_BASE    equ  0x3000
PD1_BASE    equ  0x4000
PD2_BASE    equ  0x5000
PD3_BASE    equ  0x6000

PTE_P       equ  0x001
PTE_RW      equ  0x002
PTE_PS      equ  0x080
PTE_FLAGS   equ  (PTE_P | PTE_RW | PTE_PS)

DIR_FLAGS   equ  (PTE_P | PTE_RW)

CR0_PE      equ  (1 << 0)
CR0_PG      equ  (1 << 31)
CR4_PAE     equ  (1 << 5)
CR4_PGE     equ  (1 << 7)
EFER_MSR    equ  0xC0000080
EFER_LME    equ  (1 << 8)
EFER_NXE    equ  (1 << 11)

KCODE64     equ  0x08
KDATA64     equ  0x10

BOOT_STACK_SIZE  equ  0x4000

section .note.pvh
align 4

pvh_note_start:
    dd  4
    dd  4
    dd  XEN_ELFNOTE_PHYS32_ENTRY
    db  'X', 'e', 'n', 0
    dd  pvh_entry_point
pvh_note_end:

section .boot.text
bits 32

align 8
mb2_header:
    dd  MB2_HDR_MAGIC
    dd  MB2_HDR_ARCH_I386
    dd  (mb2_header_end - mb2_header)
    dd  -(MB2_HDR_MAGIC + MB2_HDR_ARCH_I386 + (mb2_header_end - mb2_header))

    align 8
    dw  5
    dw  0
    dd  20
    dd  1280
    dd  800
    dd  32

    align 8
    dw  0
    dw  0
    dd  8
mb2_header_end:

align 16
global kobalt_entry32
global pvh_entry_point
pvh_entry_point:
kobalt_entry32:
    cli
    cld

    cmp  eax, MB2_LOADER_MAGIC
    je   .path_multiboot2

.path_pvh:
    mov  dword [boot_magic],    0x58454E50
    mov  dword [boot_info_ptr], ebx
    jmp  .build_page_tables

.path_multiboot2:
    mov  dword [boot_magic],    MB2_LOADER_MAGIC
    mov  dword [boot_info_ptr], ebx

.build_page_tables:

    mov  edi, PML4_BASE
    xor  eax, eax
    mov  ecx, (6 * 4096) / 4
    rep  stosd

    mov  dword [PML4_BASE + 0],     (PDPT_BASE | DIR_FLAGS)
    mov  dword [PML4_BASE + 4],     0

    mov  dword [PDPT_BASE +  0],    (PD0_BASE | DIR_FLAGS)
    mov  dword [PDPT_BASE +  4],    0
    mov  dword [PDPT_BASE +  8],    (PD1_BASE | DIR_FLAGS)
    mov  dword [PDPT_BASE + 12],    0
    mov  dword [PDPT_BASE + 16],    (PD2_BASE | DIR_FLAGS)
    mov  dword [PDPT_BASE + 20],    0
    mov  dword [PDPT_BASE + 24],    (PD3_BASE | DIR_FLAGS)
    mov  dword [PDPT_BASE + 28],    0

%macro FILL_PD 2
    mov  edi, %1
    mov  eax, (%2 | PTE_FLAGS)
    mov  ecx, 512
%%loop:
    mov  dword [edi],     eax
    mov  dword [edi + 4], 0
    add  edi, 8
    add  eax, 0x00200000
    dec  ecx
    jnz  %%loop
%endmacro

    FILL_PD  PD0_BASE, 0x00000000
    FILL_PD  PD1_BASE, 0x40000000
    FILL_PD  PD2_BASE, 0x80000000
    FILL_PD  PD3_BASE, 0xC0000000

    mov  eax, PML4_BASE
    mov  cr3, eax

    mov  eax, cr4
    or   eax, (CR4_PAE | CR4_PGE)
    mov  cr4, eax

    mov  ecx, EFER_MSR
    rdmsr
    or   eax, (EFER_LME | EFER_NXE)
    wrmsr

    mov  eax, cr0
    or   eax, (CR0_PE | CR0_PG)
    mov  cr0, eax

    lgdt [gdt64_ptr]

    mov  eax, long_mode_entry
    push dword KCODE64
    push eax
    retf

bits 64

long_mode_entry:

    mov  ax, KDATA64
    mov  ds, ax
    mov  es, ax
    mov  ss, ax
    xor  ax, ax
    mov  fs, ax
    mov  gs, ax

    mov  rsp, boot_stack_top
    sub  rsp, 8

    extern _bss_start
    extern _bss_end
    mov  rdi, _bss_start
    xor  al,  al
    mov  rcx, _bss_end
    sub  rcx, rdi
    rep  stosb

    mov  edi, [rel boot_magic]
    mov  esi, [rel boot_info_ptr]

    extern kmain
    call  kmain

.halt:
    cli
    hlt
    jmp  .halt

section .boot.data
align 8

align 8
gdt64:
    dq  0x0000000000000000
    dq  0x00209A0000000000
    dq  0x0000920000000000
gdt64_end:

align 4
gdt64_ptr:
    dw  (gdt64_end - gdt64 - 1)
    dq  gdt64

align 4
boot_magic:
    dd  0

align 4
boot_info_ptr:
    dd  0

align 16
    times BOOT_STACK_SIZE  db  0
boot_stack_top:
