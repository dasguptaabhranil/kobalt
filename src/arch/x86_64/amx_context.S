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

AMX_XCR0_MASK   equ 0x60000

global amx_xsaves
amx_xsaves:
    mov     eax, AMX_XCR0_MASK
    xor     edx, edx
    xsaves  [rdi]
    ret

global amx_xrstors
amx_xrstors:
    mov     eax, AMX_XCR0_MASK
    xor     edx, edx
    xrstors [rdi]
    ret

global amx_tilerelease
amx_tilerelease:

    db 0xC4, 0xE2, 0x78, 0x49, 0xC0
    ret

global amx_ldtilecfg
amx_ldtilecfg:

    db 0xC4, 0xE2, 0x78, 0x49, 0x07
    ret

global amx_tilezero_0
amx_tilezero_0:

    db 0xC4, 0xE3, 0x7B, 0x49, 0xC0
    ret

global amx_tdpbf16ps_012
amx_tdpbf16ps_012:

    tdpbf16ps tmm0, tmm1, tmm2
    ret

global amx_tdpbssd_012
amx_tdpbssd_012:
    tdpbssd tmm0, tmm1, tmm2
    ret

global amx_tdpbsud_012
amx_tdpbsud_012:
    tdpbsud tmm0, tmm1, tmm2
    ret

global amx_tdpbusd_012
amx_tdpbusd_012:
    tdpbusd tmm0, tmm1, tmm2
    ret

global amx_tdpbuud_012
amx_tdpbuud_012:
    tdpbuud tmm0, tmm1, tmm2
    ret

global amx_tdpfp16ps_012
amx_tdpfp16ps_012:

    db 0xC4, 0xE2, 0x6B, 0x5C, 0xC1
    ret
