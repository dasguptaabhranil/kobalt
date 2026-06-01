/* Copyright (C) 2026 Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

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

%define CLOCK_REALTIME   0
%define CLOCK_MONOTONIC  1
%define SYS_clock_gettime 228
%define SYS_gettimeofday  96

%define VDSO_WALL_NS    0
%define VDSO_MONO_NS    8
%define VDSO_TSC_HZ     16
%define VDSO_SEQ        24

section .text

global __vdso_data_ptr
__vdso_data_ptr:
    dq  0

global __vdso_clock_gettime
__vdso_clock_gettime:
    cmp     edi, CLOCK_REALTIME
    je      .realtime
    cmp     edi, CLOCK_MONOTONIC
    je      .monotonic

    mov     eax, SYS_clock_gettime
    syscall
    ret

.realtime:
    mov     rax, [rel __vdso_data_ptr]
    jmp     .read_ns

.monotonic:
    mov     rax, [rel __vdso_data_ptr]
    add     rax, VDSO_MONO_NS - VDSO_WALL_NS

.read_ns:

.retry:
    mov     ecx, dword [rax + VDSO_SEQ]
    test    ecx, 1
    jnz     .retry
    lfence

    cmp     edi, CLOCK_REALTIME
    je      .read_wall
    mov     rdx, qword [rax + VDSO_MONO_NS]
    jmp     .check_seq
.read_wall:
    mov     rdx, qword [rax + VDSO_WALL_NS]

.check_seq:
    lfence
    cmp     dword [rax + VDSO_SEQ], ecx
    jne     .retry

    mov     rax, rdx
    mov     rcx, 1000000000
    xor     edx, edx
    div     rcx
    mov     qword [rsi], rax
    mov     qword [rsi + 8], rdx
    xor     eax, eax
    ret

global __vdso_gettimeofday
__vdso_gettimeofday:
    test    rdi, rdi
    jz      .done

    mov     rax, [rel __vdso_data_ptr]
.gtod_retry:
    mov     ecx, dword [rax + VDSO_SEQ]
    test    ecx, 1
    jnz     .gtod_retry
    lfence
    mov     r8, qword [rax + VDSO_WALL_NS]
    lfence
    cmp     dword [rax + VDSO_SEQ], ecx
    jne     .gtod_retry

    mov     rax, r8
    mov     rcx, 1000000000
    xor     edx, edx
    div     rcx
    mov     qword [rdi], rax
    mov     rax, rdx
    mov     rcx, 1000
    xor     edx, edx
    div     rcx
    mov     qword [rdi + 8], rax
.done:
    xor     eax, eax
    ret
