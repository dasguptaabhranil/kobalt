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

#include <stdint.h>
#include <stddef.h>
#include <kernel.h>
#include <smp.h>
#include <percpu.h>
#include <spinlock.h>
#include <ipi.h>
#include <cpuhp.h>
#include "../arch/x86_64/idt.h"
#include "../arch/x86_64/gdt.h"

#define LAPIC_BASE          0xFEE00000UL
#define LAPIC_ICR_LO        0x300
#define LAPIC_ICR_HI        0x310

#define ICR_INIT            0x00004500UL
#define ICR_INIT_DEASSERT   0x00008500UL
#define ICR_STARTUP         0x00004600UL
#define ICR_PENDING         (1UL << 12)

#define SIPI_PAGE           0x08
#define AP_ONLINE_TIMEOUT   1000

#define MAX_CPUS            64

static inline uint32_t lapic_read(uint32_t off)
{
    return *(volatile uint32_t *)(LAPIC_BASE + off);
}

static inline void lapic_write(uint32_t off, uint32_t val)
{
    *(volatile uint32_t *)(LAPIC_BASE + off) = val;
}

static inline uint32_t lapic_id_of(unsigned cpu)
{
    return smp_apic_id(cpu);
}

static void mdelay(uint32_t ms)
{
    extern uint32_t sys_now(void);
    uint32_t t0 = sys_now();
    while (sys_now() - t0 < ms)
        __asm__ volatile("pause" ::: "memory");
}

static void udelay(uint32_t us)
{
    extern uint32_t sys_now(void);
    uint32_t t0 = sys_now();
    (void)us;
    while (sys_now() == t0)
        __asm__ volatile("pause" ::: "memory");
}

typedef struct {
    volatile cpuhp_state_t  state;
    volatile int            parked;
    uint32_t                apic_id;
} cpuhp_entry_t;

static cpuhp_entry_t    g_hp[MAX_CPUS];
static spinlock_t       g_lock      = SPINLOCK_INIT;
static int              g_ncpus;

static cpuhp_notifier_t *g_notifiers[CPUHP_MAX_NOTIFIERS];
static int               g_nnotifiers;

static void send_ipi(uint32_t apic_id, uint32_t icr_lo)
{
    while (lapic_read(LAPIC_ICR_LO) & ICR_PENDING)
        __asm__ volatile("pause" ::: "memory");
    lapic_write(LAPIC_ICR_HI, apic_id << 24);
    lapic_write(LAPIC_ICR_LO, icr_lo);
}

static void do_init_sipi(uint32_t apic_id)
{
    send_ipi(apic_id, ICR_INIT);
    mdelay(10);
    send_ipi(apic_id, ICR_INIT_DEASSERT);
    mdelay(2);
    send_ipi(apic_id, ICR_STARTUP | SIPI_PAGE);
    udelay(300);
    send_ipi(apic_id, ICR_STARTUP | SIPI_PAGE);
    udelay(300);
}

static void notify_prepare(unsigned cpu)
{
    for (int i = 0; i < g_nnotifiers; i++)
        if (g_notifiers[i] && g_notifiers[i]->prepare)
            g_notifiers[i]->prepare(cpu);
}

static void notify_teardown(unsigned cpu)
{
    for (int i = 0; i < g_nnotifiers; i++)
        if (g_notifiers[i] && g_notifiers[i]->teardown)
            g_notifiers[i]->teardown(cpu);
}

void cpuhp_park_action(void)
{
    unsigned cpu = PERCPU_ID();
    if (cpu < (unsigned)g_ncpus) {
        g_hp[cpu].parked = 1;
        g_hp[cpu].state  = CPUHP_OFFLINE;
    }
    for (;;)
        __asm__ volatile("cli; hlt" ::: "memory");
}

int cpuhp_online(unsigned cpu)
{
    if (cpu == 0 || cpu >= (unsigned)g_ncpus)
        return -1;

    uint64_t fl = spin_lock_irqsave(&g_lock);

    if (g_hp[cpu].state == CPUHP_ONLINE) {
        spin_unlock_irqrestore(&g_lock, fl);
        return 0;
    }

    g_hp[cpu].state  = CPUHP_BRINGUP_PREPARE;
    g_hp[cpu].parked = 0;
    spin_unlock_irqrestore(&g_lock, fl);

    notify_prepare(cpu);

    fl = spin_lock_irqsave(&g_lock);
    g_hp[cpu].state = CPUHP_BRINGUP_KICK;
    spin_unlock_irqrestore(&g_lock, fl);

    do_init_sipi(lapic_id_of(cpu));

    extern uint32_t sys_now(void);
    uint32_t t0 = sys_now();

    while (g_hp[cpu].state != CPUHP_ONLINE) {
        if (sys_now() - t0 > (uint32_t)AP_ONLINE_TIMEOUT) {
            char msg[40];
            ksnprintf(msg, sizeof(msg), "CPU %u bringup timeout", cpu);
            klog_warn("cpuhp", msg);
            return -1;
        }
        __asm__ volatile("pause" ::: "memory");
    }

    char msg[32];
    ksnprintf(msg, sizeof(msg), "CPU %u online", cpu);
    klog_ok("cpuhp", msg);
    return 0;
}

int cpuhp_offline(unsigned cpu)
{
    if (cpu == 0 || cpu >= (unsigned)g_ncpus)
        return -1;

    uint64_t fl = spin_lock_irqsave(&g_lock);

    if (g_hp[cpu].state != CPUHP_ONLINE) {
        spin_unlock_irqrestore(&g_lock, fl);
        return 0;
    }

    g_hp[cpu].state  = CPUHP_TEARDOWN;
    g_hp[cpu].parked = 0;
    spin_unlock_irqrestore(&g_lock, fl);

    notify_teardown(cpu);

    ipi_send_single(lapic_id_of(cpu), CPUHP_PARK_VECTOR);

    extern uint32_t sys_now(void);
    uint32_t t0 = sys_now();

    while (!g_hp[cpu].parked) {
        if (sys_now() - t0 > 500u) {
            char msg[48];
            ksnprintf(msg, sizeof(msg), "CPU %u park timeout -- forcing halt", cpu);
            klog_warn("cpuhp", msg);
            break;
        }
        __asm__ volatile("pause" ::: "memory");
    }

    char msg[32];
    ksnprintf(msg, sizeof(msg), "CPU %u offline", cpu);
    klog_ok("cpuhp", msg);
    return 0;
}

cpuhp_state_t cpuhp_get_state(unsigned cpu)
{
    if (cpu >= (unsigned)g_ncpus)
        return CPUHP_OFFLINE;
    return g_hp[cpu].state;
}

int cpuhp_register_notifier(cpuhp_notifier_t *n)
{
    uint64_t fl = spin_lock_irqsave(&g_lock);
    if (g_nnotifiers >= CPUHP_MAX_NOTIFIERS) {
        spin_unlock_irqrestore(&g_lock, fl);
        return -1;
    }
    g_notifiers[g_nnotifiers++] = n;
    spin_unlock_irqrestore(&g_lock, fl);
    return 0;
}

void cpuhp_unregister_notifier(cpuhp_notifier_t *n)
{
    uint64_t fl = spin_lock_irqsave(&g_lock);
    for (int i = 0; i < g_nnotifiers; i++) {
        if (g_notifiers[i] == n) {
            g_notifiers[i] = g_notifiers[--g_nnotifiers];
            g_notifiers[g_nnotifiers] = NULL;
            break;
        }
    }
    spin_unlock_irqrestore(&g_lock, fl);
}

unsigned cpuhp_online_count(void)
{
    unsigned cnt = 0;
    for (int i = 0; i < g_ncpus; i++)
        if (g_hp[i].state == CPUHP_ONLINE)
            cnt++;
    return cnt;
}

void cpuhp_init(void)
{
    extern void cpuhp_park_entry(void);

    g_ncpus = (int)smp_cpu_count();

    for (int i = 0; i < g_ncpus; i++) {
        g_hp[i].state   = CPUHP_ONLINE;
        g_hp[i].parked  = 0;
        g_hp[i].apic_id = smp_apic_id((unsigned)i);
    }

    idt_set_gate(CPUHP_PARK_VECTOR, (uintptr_t)cpuhp_park_entry,
                 GDT_KCODE64, IDT_GATE_INTERRUPT, 0);

    char msg[48];
    ksnprintf(msg, sizeof(msg), "%u CPUs managed  park_vec=0x%02x",
              (unsigned)g_ncpus, CPUHP_PARK_VECTOR);
    klog_ok("cpuhp", msg);
}
