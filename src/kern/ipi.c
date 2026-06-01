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

#include "../inc/ipi.h"
#include "../inc/smp.h"
#include "../inc/spinlock.h"
#include "../arch/x86_64/idt.h"
#include "../arch/x86_64/gdt.h"

#include <stdint.h>
#include <stddef.h>

#define LAPIC_EOI_REG       UINT32_C(0x0B0)
#define LAPIC_ICR_HIGH      UINT32_C(0x310)
#define LAPIC_ICR_LOW       UINT32_C(0x300)
#define LAPIC_ICR_PENDING   UINT32_C(1U << 12)

static volatile uint32_t *ipi_lapic = (volatile uint32_t *)0xFEE00000UL;

static inline uint32_t ipi_lapic_read(uint32_t off)
{
    return ipi_lapic[off >> 2];
}

static inline void ipi_lapic_write(uint32_t off, uint32_t val)
{
    ipi_lapic[off >> 2] = val;
}

static inline void lapic_eoi(void)
{
    ipi_lapic_write(LAPIC_EOI_REG, 0U);
}

static void lapic_icr_wait(void)
{
    while (ipi_lapic_read(LAPIC_ICR_LOW) & LAPIC_ICR_PENDING)
        __asm__ volatile ("pause");
}

#define ICR_FIXED       UINT32_C(0x000 << 8)
#define ICR_ASSERT      UINT32_C(1U << 14)
#define ICR_EDGE        UINT32_C(0U << 15)
#define ICR_PHYS_DEST   UINT32_C(0U << 11)
#define ICR_NO_SHORT    UINT32_C(0U << 18)
#define ICR_ALL_EXCSELF UINT32_C(3U << 18)
#define ICR_ALL_INCSELF UINT32_C(2U << 18)

#define ICR_FIXED_SINGLE(v) \
    (ICR_FIXED | ICR_ASSERT | ICR_EDGE | ICR_PHYS_DEST | ICR_NO_SHORT | ((v) & 0xFFU))

#define ICR_FIXED_OTHERS(v) \
    (ICR_FIXED | ICR_ASSERT | ICR_EDGE | ICR_PHYS_DEST | ICR_ALL_EXCSELF | ((v) & 0xFFU))

#define ICR_FIXED_ALL(v) \
    (ICR_FIXED | ICR_ASSERT | ICR_EDGE | ICR_PHYS_DEST | ICR_ALL_INCSELF | ((v) & 0xFFU))

static spinlock_t           g_ipi_lock   = SPINLOCK_INIT;
static volatile tlb_shootdown_t *g_tlb_work  = NULL;
static volatile ipi_call_t      *g_call_work = NULL;

void ipi_init(void)
{
    idt_set_gate(IPI_VECTOR_TLB_FLUSH,
                 (uintptr_t)ipi_entry_tlb_flush,
                 GDT_KCODE64, IDT_GATE_INTERRUPT, IDT_IST_NONE);

    idt_set_gate(IPI_VECTOR_FUNC_CALL,
                 (uintptr_t)ipi_entry_func_call,
                 GDT_KCODE64, IDT_GATE_INTERRUPT, IDT_IST_NONE);

    idt_set_gate(IPI_VECTOR_HALT,
                 (uintptr_t)ipi_entry_halt,
                 GDT_KCODE64, IDT_GATE_INTERRUPT, IDT_IST_NONE);

    idt_set_gate(IPI_VECTOR_RESCHED,
                 (uintptr_t)ipi_entry_resched,
                 GDT_KCODE64, IDT_GATE_INTERRUPT, IDT_IST_NONE);
}

void ipi_send_single(uint32_t apic_id, uint8_t vector)
{
    lapic_icr_wait();
    ipi_lapic_write(LAPIC_ICR_HIGH, apic_id << 24);
    ipi_lapic_write(LAPIC_ICR_LOW,  ICR_FIXED_SINGLE(vector));
    lapic_icr_wait();
}

void ipi_send_to_cpu(uint32_t cpu_id, uint8_t vector)
{
    ipi_send_single(smp_apic_id(cpu_id), vector);
}

void ipi_send_others(uint8_t vector)
{
    lapic_icr_wait();
    ipi_lapic_write(LAPIC_ICR_HIGH, 0U);
    ipi_lapic_write(LAPIC_ICR_LOW,  ICR_FIXED_OTHERS(vector));
    lapic_icr_wait();
}

void ipi_send_all(uint8_t vector)
{
    lapic_icr_wait();
    ipi_lapic_write(LAPIC_ICR_HIGH, 0U);
    ipi_lapic_write(LAPIC_ICR_LOW,  ICR_FIXED_ALL(vector));
    lapic_icr_wait();
}

static void do_local_flush(const tlb_shootdown_t *sd)
{
    if (sd->vaddr == 0 && sd->npages == 0) {
        uint64_t cr3;
        __asm__ volatile ("movq %%cr3, %0" : "=r"(cr3));
        __asm__ volatile ("movq %0, %%cr3" : : "r"(cr3) : "memory");
    } else {
        for (uint64_t i = 0; i < sd->npages || (i == 0 && sd->npages == 0); i++) {
            const uintptr_t va = sd->vaddr + (i << 12);
            __asm__ volatile ("invlpg (%0)" : : "r"(va) : "memory");
            if (sd->npages == 0) break;
        }
    }
}

static void shootdown_and_wait(tlb_shootdown_t *sd)
{
    const int target_count = (int)smp_cpu_count() - 1;

    sd->ack_count = 0;
    __asm__ volatile ("" ::: "memory");

    g_tlb_work = sd;
    __asm__ volatile ("mfence" ::: "memory");

    do_local_flush(sd);

    if (target_count > 0) {
        ipi_send_others(IPI_VECTOR_TLB_FLUSH);

        while (__atomic_load_n(&sd->ack_count, __ATOMIC_ACQUIRE) < target_count)
            __asm__ volatile ("pause");
    }

    g_tlb_work = NULL;
    __asm__ volatile ("" ::: "memory");
}

void ipi_tlb_flush_page(uintptr_t vaddr)
{
    tlb_shootdown_t sd = { .vaddr = vaddr, .npages = 0, .ack_count = 0 };

    const uint64_t flags = spin_lock_irqsave(&g_ipi_lock);
    shootdown_and_wait(&sd);
    spin_unlock_irqrestore(&g_ipi_lock, flags);
}

void ipi_tlb_flush_range(uintptr_t vaddr, uint64_t npages)
{
    if (npages == 0) return;
    tlb_shootdown_t sd = { .vaddr = vaddr, .npages = npages, .ack_count = 0 };

    const uint64_t flags = spin_lock_irqsave(&g_ipi_lock);
    shootdown_and_wait(&sd);
    spin_unlock_irqrestore(&g_ipi_lock, flags);
}

void ipi_tlb_flush_all(void)
{
    tlb_shootdown_t sd = { .vaddr = 0, .npages = 0, .ack_count = 0 };

    const uint64_t flags = spin_lock_irqsave(&g_ipi_lock);
    shootdown_and_wait(&sd);
    spin_unlock_irqrestore(&g_ipi_lock, flags);
}

void ipi_call_function(ipi_fn_t fn, void *arg)
{
    const int target_count = (int)smp_cpu_count() - 1;
    if (target_count <= 0) return;

    ipi_call_t call = { .fn = fn, .arg = arg, .ack_count = 0 };

    const uint64_t flags = spin_lock_irqsave(&g_ipi_lock);

    g_call_work = &call;
    __asm__ volatile ("mfence" ::: "memory");

    ipi_send_others(IPI_VECTOR_FUNC_CALL);

    while (__atomic_load_n(&call.ack_count, __ATOMIC_ACQUIRE) < target_count)
        __asm__ volatile ("pause");

    g_call_work = NULL;
    __asm__ volatile ("" ::: "memory");

    spin_unlock_irqrestore(&g_ipi_lock, flags);
}

void ipi_panic_halt(void)
{
    if (smp_cpu_count() > 1)
        ipi_send_others(IPI_VECTOR_HALT);
}

void ipi_handle_tlb_flush(void)
{
    lapic_eoi();

    const volatile tlb_shootdown_t *sd = g_tlb_work;
    if (!sd) return;

    if (sd->vaddr == 0 && sd->npages == 0) {
        uint64_t cr3;
        __asm__ volatile ("movq %%cr3, %0" : "=r"(cr3));
        __asm__ volatile ("movq %0, %%cr3" : : "r"(cr3) : "memory");
    } else {
        const uint64_t n = sd->npages ? sd->npages : 1ULL;
        for (uint64_t i = 0; i < n; i++) {
            const uintptr_t va = sd->vaddr + (i << 12);
            __asm__ volatile ("invlpg (%0)" : : "r"(va) : "memory");
        }
    }

    __atomic_fetch_add((volatile int *)&sd->ack_count, 1, __ATOMIC_SEQ_CST);
}

void ipi_handle_func_call(void)
{
    lapic_eoi();

    const volatile ipi_call_t *c = g_call_work;
    if (!c || !c->fn) return;

    c->fn(c->arg);

    __atomic_fetch_add((volatile int *)&c->ack_count, 1, __ATOMIC_SEQ_CST);
}

void ipi_handle_halt(void)
{
    lapic_eoi();

    __asm__ volatile (
        "cli        \n"
        "1:         \n"
        "   hlt     \n"
        "   jmp 1b  \n"
    );
    __builtin_unreachable();
}

void ipi_handle_resched(void)
{
    lapic_eoi();
}
