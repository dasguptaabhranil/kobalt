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

#include "../inc/kposixz_internal.h"

#include <apic_timer.h>
#include <sched.h>
#include <kmalloc.h>
#include <kfmt.h>

extern void uart_putc(char c);

__attribute__((weak))
uptr vmm_alloc_pages(uptr hint, usz n_pages, u32 prot)
{
    (void)hint; (void)prot;
    if (!n_pages) return 0;

    void *p = kmalloc(n_pages << 12);
    return (uptr)p;
}

__attribute__((weak))
void vmm_free_pages(uptr addr, usz n_pages)
{
    (void)n_pages;
    if (addr) kfree((void *)addr);
}

__attribute__((weak))
s32 vmm_set_prot(uptr addr, usz n_pages, u32 prot)
{

    (void)addr; (void)n_pages; (void)prot;
    return 0;
}

__attribute__((weak))
s32 uart_getc(void)
{
    return -1;
}

u64 kobalt_acpi_timer_ns(void)
{
    return apic_timer_uptime_ms() * 1000000ULL;
}

void kobalt_sched_yield(void)
{
    sched_yield();
}

uptr kobalt_vmm_alloc(uptr hint, usz size, u32 prot)
{
    if (!size) return 0;
    usz n_pages = (size + 0xFFFULL) >> 12;
    return vmm_alloc_pages(hint, n_pages, prot);
}

void kobalt_vmm_free(uptr addr, usz size)
{
    if (!addr || !size) return;
    usz n_pages = (size + 0xFFFULL) >> 12;
    vmm_free_pages(addr, n_pages);
}

s32 kobalt_vmm_protect(uptr addr, usz size, u32 prot)
{
    if (!addr || !size) return -1;
    usz n_pages = (size + 0xFFFULL) >> 12;
    return vmm_set_prot(addr, n_pages, prot);
}

s32 uart_getc_blocking(void)
{
    s32 c;
    do {
        c = uart_getc();
        if (c < 0) {

            __asm__ volatile("pause" ::: "memory");
        }
    } while (c < 0);
    return c;
}
