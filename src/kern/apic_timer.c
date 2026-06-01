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

#include "apic_timer.h"
#include "../arch/x86_64/idt.h"
#include "../arch/x86_64/gdt.h"
#include <stdint.h>
#include <stddef.h>

#define LAPIC_SVR           (0x0F0U)
#define LAPIC_EOI           (0x0B0U)
#define LAPIC_LVT_TIMER     (0x320U)
#define LAPIC_ICR           (0x380U)
#define LAPIC_CCR           (0x390U)
#define LAPIC_DCR           (0x3E0U)

#define LAPIC_SVR_ENABLE    (1U << 8)
#define LAPIC_SPURIOUS_VEC  (0xFFU)

#define LVT_TIMER_ONESHOT   (0U)
#define LVT_TIMER_MASKED    (0x1U << 16)

#define DCR_DIVIDE_BY_1     (0x0BU)

#define PIT_HZ              (1193182U)
#define PIT_CAL_MS          (5U)
#define PIT_CAL_TICKS       ((PIT_HZ * PIT_CAL_MS) / 1000U)

#define PIT_CH2_DATA        (0x42U)
#define PIT_CMD             (0x43U)
#define PIT_NMI_SC          (0x61U)
#define PIT_CMD_CH2_ONESHOT (0xB0U)
#define NMI_SC_CH2_GATE     (0x01U)
#define NMI_SC_SPKR_EN      (0x02U)
#define NMI_SC_CH2_OUT      (0x20U)

#define LAPIC_PHYS_BASE     (0xFEE00000UL)

volatile uint32_t *lapic_base     = (volatile uint32_t *)LAPIC_PHYS_BASE;
uint32_t           apic_ticks_per_ms;
uint64_t           tsc_khz;

static volatile uint64_t apic_tick_count = 0;
static uint32_t          apic_timer_hz   = APIC_TIMER_DEFAULT_HZ;

static inline uint32_t lapic_read(uint32_t off)  { return lapic_base[off >> 2]; }
static inline void lapic_write(uint32_t off, uint32_t val) { lapic_base[off >> 2] = val; }

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port) : "memory");
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port) : "memory");
    return val;
}

static inline void io_wait(void) { outb(0x80U, 0x00U); }

static inline uint64_t rdtsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static void lapic_enable(void)
{
    uint32_t svr = lapic_read(LAPIC_SVR);
    svr &= ~0xFFU;
    svr |=  LAPIC_SVR_ENABLE | LAPIC_SPURIOUS_VEC;
    lapic_write(LAPIC_SVR, svr);
}

static uint32_t pit_calibrate_lapic(void)
{
    uint8_t nmi_sc_saved = inb(PIT_NMI_SC);
    outb(PIT_NMI_SC, (nmi_sc_saved & ~(NMI_SC_CH2_GATE | NMI_SC_SPKR_EN)));

    outb(PIT_CMD,      PIT_CMD_CH2_ONESHOT);
    io_wait();
    outb(PIT_CH2_DATA, (uint8_t)( PIT_CAL_TICKS       & 0xFFU));
    io_wait();
    outb(PIT_CH2_DATA, (uint8_t)((PIT_CAL_TICKS >> 8) & 0xFFU));

    lapic_write(LAPIC_DCR,       DCR_DIVIDE_BY_1);
    lapic_write(LAPIC_LVT_TIMER, LVT_TIMER_MASKED);
    lapic_write(LAPIC_ICR,       0xFFFFFFFFU);

    uint64_t tsc_start = rdtsc();
    outb(PIT_NMI_SC, (nmi_sc_saved & ~NMI_SC_SPKR_EN) | NMI_SC_CH2_GATE);

    while (!(inb(PIT_NMI_SC) & NMI_SC_CH2_OUT))
        __asm__ volatile ("pause");

    uint64_t tsc_end     = rdtsc();
    uint32_t lapic_remain = lapic_read(LAPIC_CCR);
    outb(PIT_NMI_SC, nmi_sc_saved);

    tsc_khz = (tsc_end - tsc_start) / PIT_CAL_MS;

    uint32_t elapsed = 0xFFFFFFFFU - lapic_remain;
    return elapsed / PIT_CAL_MS;
}

static void lapic_arm_oneshot_default(void)
{
    uint32_t initial = apic_ticks_per_ms * APIC_TIMER_DEFAULT_QUANTUM_MS;
    lapic_write(LAPIC_DCR,       DCR_DIVIDE_BY_1);
    lapic_write(LAPIC_LVT_TIMER, (uint32_t)APIC_TIMER_VECTOR | LVT_TIMER_ONESHOT);
    lapic_write(LAPIC_ICR,       initial);
}

void apic_send_eoi(void) { lapic_write(LAPIC_EOI, 0); }

void apic_timer_init(void)
{
    lapic_enable();
    apic_ticks_per_ms = pit_calibrate_lapic();

    extern void apic_timer_entry(void);
    idt_set_gate(APIC_TIMER_VECTOR,
                 (uintptr_t)apic_timer_entry,
                 GDT_KCODE64, IDT_GATE_INTERRUPT, IDT_IST_NONE);

    lapic_arm_oneshot_default();
}

void apic_timer_ap_init(void)
{
    lapic_enable();

    extern void apic_timer_entry(void);
    idt_set_gate(APIC_TIMER_VECTOR,
                 (uintptr_t)apic_timer_entry,
                 GDT_KCODE64, IDT_GATE_INTERRUPT, IDT_IST_NONE);

    lapic_arm_oneshot_default();
}

void apic_timer_tick(void)
{
    lapic_write(LAPIC_EOI, 0);
    apic_tick_count++;

    extern void sched_tick(void);
    sched_tick();
}

void apic_timer_set_freq(uint32_t hz)
{
    if (hz < 1U)     hz = 1U;
    if (hz > 10000U) hz = 10000U;
    apic_timer_hz = hz;
}

uint64_t apic_timer_ticks(void) { return apic_tick_count; }

uint64_t apic_timer_uptime_ms(void)
{
    if (!tsc_khz) return (apic_tick_count * 1000ULL) / (uint64_t)apic_timer_hz;
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    uint64_t tsc = ((uint64_t)hi << 32) | lo;
    return tsc / tsc_khz;
}

void apic_timer_set_base(uintptr_t vaddr)
{
    lapic_base = (volatile uint32_t *)vaddr;
}
