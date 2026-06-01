/* Copyright (C) 2026 Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <kfmt.h>

static __inline__ __attribute__((always_inline))
uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static __inline__ __attribute__((always_inline))
void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static __inline__ __attribute__((always_inline))
void hlt(void) {
    __asm__ volatile ("hlt" ::: "memory");
}

static __inline__ __attribute__((always_inline))
void cpu_relax(void) {
    __asm__ volatile ("pause" ::: "memory");
}

static __inline__ __attribute__((always_inline))
uint64_t read_cr0(void) {
    uint64_t val;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(val));
    return val;
}

static __inline__ __attribute__((always_inline))
void sti(void) {
    __asm__ volatile ("sti" ::: "memory");
}

static __inline__ __attribute__((always_inline))
void write_cr0(uint64_t val) {
    __asm__ volatile ("mov %0, %%cr0" : : "r"(val) : "memory");
}

static __inline__ __attribute__((always_inline))
void tlb_flush_all(void) {
    uintptr_t cr3;
    __asm__ volatile ("mov %%cr3, %0; mov %0, %%cr3" : "=r"(cr3) : : "memory");
}

#define PAGE_SIZE           4096
#define HUGE_PAGE_SIZE      0x200000UL
#define PTE_W               (1ULL << 1)
#define PD_BASE_ADDR        0x3000
#define PD_ENTRIES_COUNT    2048

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char* s);

void vga_init(void);
void vga_putc(char c);
void vga_puts(const char* s);

void mm_seal(void);
static __inline__ __attribute__((always_inline))
uint64_t read_cr4(void) {
    uint64_t val;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(val));
    return val;
}

static __inline__ __attribute__((always_inline))
void write_cr4(uint64_t val) {
    __asm__ volatile ("mov %0, %%cr4" : : "r"(val) : "memory");
}

static __inline__ __attribute__((always_inline))
void store_barrier(void) {
    __asm__ volatile ("sfence" ::: "memory");
}
