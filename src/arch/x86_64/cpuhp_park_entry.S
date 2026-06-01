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

%define LAPIC_EOI_REG   0xFEE000B0

section .text

extern cpuhp_park_action

global cpuhp_park_entry
cpuhp_park_entry:
    push    rax
    push    rcx
    mov     rax, LAPIC_EOI_REG
    mov     dword [rax], 0
    pop     rcx
    pop     rax
    call    cpuhp_park_action
    iretq
