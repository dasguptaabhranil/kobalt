/* Copyright (C) 2026 Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)

ENTRY(kobalt_entry32)

PHDRS
{
    pvh_note  PT_NOTE  FLAGS(4);
    ro_load   PT_LOAD  FLAGS(5);
    rw_load   PT_LOAD  FLAGS(6);
}

SECTIONS
{
    . = 0x200000;
    _kernel_start      = .;
    _kernel_seal_start = .;

    .note.pvh : ALIGN(4) {
        KEEP(*(.note.pvh))
    } :pvh_note :ro_load

    .boot.text : ALIGN(16) {
        KEEP(*(.boot.text))
    } :ro_load

    .text : ALIGN(16) {
        *(.text .text.*)
    } :ro_load

    .rodata : ALIGN(16) {
        *(.rodata .rodata.*)
        *(.rodata1)
    } :ro_load

    .uaccess_ex_table : ALIGN(8) {
        __uaccess_ex_start = .;
        KEEP(*(.uaccess_ex_table))
        __uaccess_ex_end = .;
    } :ro_load

    . = ALIGN(0x200000);
    _kernel_seal_end = .;

    .boot.data : ALIGN(16) {
        KEEP(*(.boot.data))
    } :rw_load

    .data : ALIGN(16) {
        *(.data .data.*)
    } :rw_load

    .eh_frame : ALIGN(8) {
        KEEP(*(.eh_frame .eh_frame.*))
    } :rw_load

    .bss : ALIGN(4096) {
        _bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        _bss_end   = .;
    } :rw_load

    _kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note.GNU-stack)
        *(.note.gnu.*)
        *(.gnu.*)
        *(.debug .debug_*)
        *(.gcc_except_table .gcc_except_table.*)
    }
}
