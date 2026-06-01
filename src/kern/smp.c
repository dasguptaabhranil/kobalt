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

#include "../inc/smp.h"
#include "../inc/percpu.h"
#include "../inc/ipi.h"
#include "../arch/x86_64/idt.h"
#include "../arch/x86_64/gdt.h"
#include "../inc/acpi.h"
#include "../inc/apic_timer.h"
#include "../inc/kmalloc.h"
#include "../inc/kernel.h"
#include <amx_init.h>
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include <string.h>

typedef struct __attribute__((packed)) {
    acpi_sdt_hdr_t  hdr;
    uint32_t        lapic_addr;
    uint32_t        flags;
} madt_hdr_t;

typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  length;
} madt_entry_hdr_t;

#define MADT_TYPE_LAPIC         0
#define MADT_TYPE_LAPIC_ADDR_OV 5
#define MADT_TYPE_X2APIC        9

typedef struct __attribute__((packed)) {
    madt_entry_hdr_t  hdr;
    uint8_t           acpi_proc_id;
    uint8_t           apic_id;
    uint32_t          flags;
} madt_lapic_t;

typedef struct __attribute__((packed)) {
    madt_entry_hdr_t  hdr;
    uint16_t          _reserved;
    uint64_t          lapic_pa;
} madt_lapic_addr_ov_t;

typedef struct __attribute__((packed)) {
    madt_entry_hdr_t  hdr;
    uint16_t          _reserved;
    uint32_t          x2apic_id;
    uint32_t          flags;
    uint32_t          acpi_proc_uid;
} madt_x2apic_t;

#define LAPIC_EOI           UINT32_C(0x0B0)
#define LAPIC_ICR_HIGH      UINT32_C(0x310)
#define LAPIC_ICR_LOW       UINT32_C(0x300)
#define LAPIC_SVR           UINT32_C(0x0F0)
#define LAPIC_ID            UINT32_C(0x020)

#define ICR_DELIVERY_FIXED   UINT32_C(0x000 << 8)
#define ICR_DELIVERY_INIT    UINT32_C(0x005 << 8)
#define ICR_DELIVERY_SIPI    UINT32_C(0x006 << 8)
#define ICR_DEST_PHYSICAL    UINT32_C(0U << 11)
#define ICR_DELIVERY_PENDING UINT32_C(1U << 12)
#define ICR_LEVEL_ASSERT     UINT32_C(1U << 14)
#define ICR_LEVEL_DEASSERT   UINT32_C(0U << 14)
#define ICR_TRIGGER_EDGE     UINT32_C(0U << 15)
#define ICR_TRIGGER_LEVEL    UINT32_C(1U << 15)
#define ICR_DEST_NO_SHORT    UINT32_C(0U << 18)
#define ICR_DEST_ALL_EXCSELF UINT32_C(3U << 18)

#define ICR_INIT_ASSERT    (ICR_DELIVERY_INIT | ICR_LEVEL_ASSERT   | \
                            ICR_TRIGGER_EDGE  | ICR_DEST_PHYSICAL  | ICR_DEST_NO_SHORT)
#define ICR_INIT_DEASSERT  (ICR_DELIVERY_INIT | ICR_LEVEL_DEASSERT | \
                            ICR_TRIGGER_LEVEL | ICR_DEST_PHYSICAL  | ICR_DEST_NO_SHORT)
#define ICR_SIPI(v)        (ICR_DELIVERY_SIPI | ICR_LEVEL_ASSERT   | \
                            ICR_TRIGGER_EDGE  | ICR_DEST_PHYSICAL  | \
                            ICR_DEST_NO_SHORT | ((v) & 0xFFU))

typedef struct __attribute__((packed)) {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit_high;
    uint8_t  base_high;
} smp_gdt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit_high;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t _reserved;
} smp_tss_desc_t;

typedef struct __attribute__((packed)) {
    uint32_t _reserved0;
    uint64_t rsp[3];
    uint64_t _reserved1;
    uint64_t ist[7];
    uint64_t _reserved2;
    uint16_t _reserved3;
    uint16_t iopb_offset;
} smp_tss_t;

_Static_assert(sizeof(smp_tss_t) == 104, "smp_tss_t must be 104 bytes");

#define SMP_GDT_ENTRIES  7

typedef struct {
    smp_gdt_entry_t gdt[SMP_GDT_ENTRIES];
    uint8_t         _gdt_pad[8];
    smp_tss_t       tss;
    uint8_t         _tss_pad[24];
    uint8_t         ist_nmi[SMP_AP_IST_SIZE];
    uint8_t         ist_df [SMP_AP_IST_SIZE];
    uint8_t         ist_mce[SMP_AP_IST_SIZE];
} __attribute__((aligned(16))) ap_hw_t;

#define ACC_P    (1U << 7)
#define ACC_DPL0  0U
#define ACC_DPL3 (3U << 5)
#define ACC_S    (1U << 4)
#define ACC_CODE (0x0AU)
#define ACC_DATA (0x02U)
#define ACC_TSS  (0x09U)

#define ACCESS_KCODE64 (ACC_P | ACC_DPL0 | ACC_S | ACC_CODE)
#define ACCESS_KDATA64 (ACC_P | ACC_DPL0 | ACC_S | ACC_DATA)
#define ACCESS_UDATA64 (ACC_P | ACC_DPL3 | ACC_S | ACC_DATA)
#define ACCESS_UCODE64 (ACC_P | ACC_DPL3 | ACC_S | ACC_CODE)
#define ACCESS_TSS64   (ACC_P | ACC_DPL0 | ACC_TSS)

#define FLAGS_CODE64  0xAU
#define FLAGS_DATA    0xCU
#define FLAGS_TSS     0x0U

#define LIMIT_FLAT  0xFFFFFU

typedef struct __attribute__((packed)) {
    uint64_t pml4;
    uint64_t stack;
    uint64_t entry;
    uint32_t cpuid;
    uint32_t gate;
} tramp_data_t;

#define TRAMP_DATA_PTR  ((volatile tramp_data_t *)((uintptr_t)SMP_TRAMP_PHYS + 2U))

cpu_info_t        g_cpu_table[SMP_MAX_CPUS];
volatile uint32_t g_cpu_online_count = 1;
uint32_t          g_cpu_total        = 1;
uint32_t          g_bsp_apic_id      = 0;
percpu_t          g_percpu_bsp;

static ap_hw_t   *g_ap_hw[SMP_MAX_CPUS];
static percpu_t  *g_percpu[SMP_MAX_CPUS];

static struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) g_idtr_save;

static volatile uint32_t *g_lapic = (volatile uint32_t *)0xFEE00000UL;

static inline uint32_t lapic_read(uint32_t off)
{
    return g_lapic[off >> 2];
}

static inline void lapic_write(uint32_t off, uint32_t val)
{
    g_lapic[off >> 2] = val;
}

static inline uint32_t lapic_id(void)
{
    return lapic_read(LAPIC_ID) >> 24;
}

static void lapic_icr_wait(void)
{
    while (lapic_read(LAPIC_ICR_LOW) & ICR_DELIVERY_PENDING)
        __asm__ volatile ("pause");
}

static void smp_mdelay(uint32_t ms)
{
    while (ms--) {
        uint8_t p61 = inb(0x61);
        outb(0x61, (uint8_t)(p61 & ~0x01u));
        outb(0x43, 0xB0u);
        outb(0x42, 0xA9u);
        outb(0x42, 0x04u);
        outb(0x61, (uint8_t)(p61 | 0x01u));
        while (!(inb(0x61) & 0x20u))
            __asm__ volatile ("pause");
    }
}

static void smp_pit_ch2_arm(uint32_t ms)
{
    uint32_t ticks = ms * 1193u;
    if (ticks > 65535u) ticks = 65535u;

    uint8_t p61 = inb(0x61);
    outb(0x61, (uint8_t)(p61 & ~0x01u));
    outb(0x43, 0xB0u);
    outb(0x42, (uint8_t)(ticks & 0xFFu));
    outb(0x42, (uint8_t)(ticks >> 8));
    outb(0x61, (uint8_t)(p61 | 0x01u));
}

static int smp_pit_ch2_expired(void)
{
    return (inb(0x61) & 0x20u) != 0;
}

static void smp_gdt_encode(smp_gdt_entry_t *e,
                            uint32_t base, uint32_t limit,
                            uint8_t access, uint8_t flags)
{
    e->limit_low        = (uint16_t)( limit        & 0xFFFFU);
    e->base_low         = (uint16_t)( base         & 0xFFFFU);
    e->base_mid         = (uint8_t) ((base  >> 16) & 0xFFU);
    e->access           =  access;
    e->flags_limit_high = (uint8_t) (((flags & 0x0FU) << 4) |
                                     ((limit >> 16)  & 0x0FU));
    e->base_high        = (uint8_t) ((base  >> 24) & 0xFFU);
}

static void smp_tss_encode(smp_gdt_entry_t *first_slot, const smp_tss_t *tss_ptr)
{
    const uint64_t base  = (uint64_t)(uintptr_t)tss_ptr;
    const uint32_t limit = (uint32_t)(sizeof(smp_tss_t) - 1);
    smp_tss_desc_t *d    = (smp_tss_desc_t *)first_slot;

    d->limit_low        = (uint16_t)( limit       & 0xFFFFU);
    d->base_low         = (uint16_t)( base        & 0xFFFFU);
    d->base_mid         = (uint8_t) ((base >> 16) & 0xFFU);
    d->access           =  ACCESS_TSS64;
    d->flags_limit_high = (uint8_t) (((FLAGS_TSS & 0x0FU) << 4) |
                                     ((limit >> 16) & 0x0FU));
    d->base_high        = (uint8_t) ((base >> 24) & 0xFFU);
    d->base_upper       = (uint32_t)((base >> 32) & 0xFFFFFFFFU);
    d->_reserved        = 0;
}

void smp_ap_gdt_init(uint32_t cpu_id)
{
    ap_hw_t *hw = g_ap_hw[cpu_id];

    smp_gdt_encode(&hw->gdt[0], 0, 0, 0, 0);
    smp_gdt_encode(&hw->gdt[1], 0, LIMIT_FLAT, ACCESS_KCODE64, FLAGS_CODE64);
    smp_gdt_encode(&hw->gdt[2], 0, LIMIT_FLAT, ACCESS_KDATA64, FLAGS_DATA);
    smp_gdt_encode(&hw->gdt[3], 0, LIMIT_FLAT, ACCESS_UDATA64, FLAGS_DATA);
    smp_gdt_encode(&hw->gdt[4], 0, LIMIT_FLAT, ACCESS_UCODE64, FLAGS_CODE64);
    smp_tss_encode(&hw->gdt[5], &hw->tss);

    hw->tss.ist[IDT_IST_NMI - 1] =
        (uint64_t)(uintptr_t)(hw->ist_nmi + SMP_AP_IST_SIZE);
    hw->tss.ist[IDT_IST_DF  - 1] =
        (uint64_t)(uintptr_t)(hw->ist_df  + SMP_AP_IST_SIZE);
    hw->tss.ist[IDT_IST_MCE - 1] =
        (uint64_t)(uintptr_t)(hw->ist_mce + SMP_AP_IST_SIZE);

    hw->tss.rsp[0]      = 0;
    hw->tss.iopb_offset = (uint16_t)sizeof(smp_tss_t);

    const struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) gdtr = {
        .limit = (uint16_t)(sizeof(hw->gdt) - 1),
        .base  = (uint64_t)(uintptr_t)hw->gdt,
    };
    __asm__ volatile ("lgdt %0" : : "m"(gdtr) : "memory");

    __asm__ volatile (
        "pushq  %[cs]            \n"
        "leaq   1f(%%rip), %%rax \n"
        "pushq  %%rax            \n"
        "lretq                   \n"
        "1:                      \n"
        : : [cs] "i"((uint64_t)GDT_KCODE64) : "rax", "memory"
    );

    __asm__ volatile (
        "movw %w[kd], %%ax  \n"
        "movw %%ax, %%ds    \n"
        "movw %%ax, %%es    \n"
        "movw %%ax, %%ss    \n"
        "xorw %%ax, %%ax    \n"
        "movw %%ax, %%fs    \n"
        "movw %%ax, %%gs    \n"
        : : [kd] "i"((uint16_t)GDT_KDATA64) : "rax", "memory"
    );

    __asm__ volatile (
        "movw %w[ts], %%ax  \n"
        "ltr  %%ax          \n"
        : : [ts] "i"((uint16_t)GDT_TSS_SEL) : "rax", "memory"
    );
}

static void smp_send_init_sipi(uint32_t apic_id)
{
    const uint8_t sipi_vector = (uint8_t)(SMP_TRAMP_VECTOR & 0xFFU);

    lapic_write(LAPIC_ICR_HIGH, apic_id << 24);
    lapic_write(LAPIC_ICR_LOW,  ICR_INIT_ASSERT);
    lapic_icr_wait();

    smp_mdelay(10);

    lapic_write(LAPIC_ICR_HIGH, apic_id << 24);
    lapic_write(LAPIC_ICR_LOW,  ICR_INIT_DEASSERT);
    lapic_icr_wait();

    smp_mdelay(1);

    lapic_write(LAPIC_ICR_HIGH, apic_id << 24);
    lapic_write(LAPIC_ICR_LOW,  ICR_SIPI(sipi_vector));
    lapic_icr_wait();

    smp_mdelay(1);

    lapic_write(LAPIC_ICR_HIGH, apic_id << 24);
    lapic_write(LAPIC_ICR_LOW,  ICR_SIPI(sipi_vector));
    lapic_icr_wait();
}

extern uint8_t ap_tramp_start[];
extern uint8_t ap_tramp_end[];

void smp_bsp_early_init(void)
{
    g_bsp_apic_id = lapic_id();

    g_percpu[0]          = &g_percpu_bsp;
    g_percpu_bsp.cpu_id  = 0;
    g_percpu_bsp.apic_id = g_bsp_apic_id;
    percpu_install(&g_percpu_bsp);

    g_cpu_table[0].cpu_id  = 0;
    g_cpu_table[0].apic_id = g_bsp_apic_id;
    atomic_store(&g_cpu_table[0].online, 1);
}

void smp_init(void)
{
    if (!g_percpu[0])
        smp_bsp_early_init();

    uint32_t next_cpu_id = 1;

    __asm__ volatile ("sidt %0" : "=m"(g_idtr_save) : : "memory");

    const madt_hdr_t *madt = (const madt_hdr_t *)acpi_find_table("APIC");
    if (!madt) {
        uart_puts("SMP: MADT not found; running uniprocessor.\n");
        g_cpu_total = 1;
        return;
    }

    g_lapic = (volatile uint32_t *)(uintptr_t)madt->lapic_addr;

    const size_t tramp_size = (size_t)(ap_tramp_end - ap_tramp_start);
    uint8_t *tramp_phys = (uint8_t *)((uintptr_t)SMP_TRAMP_PHYS);
    memcpy(tramp_phys, ap_tramp_start, tramp_size);

    uint64_t bsp_cr3;
    __asm__ volatile ("movq %%cr3, %0" : "=r"(bsp_cr3));

    const uint8_t *p   = (const uint8_t *)(madt + 1);
    const uint8_t *end = (const uint8_t *)madt + madt->hdr.length;

    while (p < end) {
        const madt_entry_hdr_t *eh = (const madt_entry_hdr_t *)p;
        if (eh->length < 2 || p + eh->length > end)
            break;

        if (eh->type == MADT_TYPE_LAPIC) {
            const madt_lapic_t *le = (const madt_lapic_t *)p;

            const int enabled = (le->flags & MADT_CPU_ENABLED) ||
                                 (le->flags & MADT_CPU_ONLINE_CAPABLE);
            if (!enabled) { p += eh->length; continue; }
            if (le->apic_id == g_bsp_apic_id) { p += eh->length; continue; }

            if (next_cpu_id >= SMP_MAX_CPUS) {
                uart_puts("SMP: exceeded SMP_MAX_CPUS; ignoring remaining APs.\n");
                break;
            }

            const uint32_t cpu_id  = next_cpu_id++;
            const uint32_t apic_id = le->apic_id;

            ap_hw_t *hw = (ap_hw_t *)kmalloc(sizeof(ap_hw_t));
            if (!hw) {
                uart_puts("SMP: kmalloc failed for AP hardware state; skipping AP.\n");
                p += eh->length;
                continue;
            }
            memset(hw, 0, sizeof(ap_hw_t));
            g_ap_hw[cpu_id] = hw;

            percpu_t *pc = (percpu_t *)kmalloc(sizeof(percpu_t));
            if (!pc) {
                uart_puts("SMP: kmalloc failed for percpu; skipping AP.\n");
                kfree(hw);
                p += eh->length;
                continue;
            }
            memset(pc, 0, sizeof(percpu_t));
            pc->cpu_id  = cpu_id;
            pc->apic_id = apic_id;
            g_percpu[cpu_id] = pc;

            uint8_t *kstack = (uint8_t *)kmalloc(SMP_AP_KSTACK_SIZE);
            if (!kstack) {
                uart_puts("SMP: kmalloc failed for AP kernel stack; skipping AP.\n");
                kfree(pc);
                kfree(hw);
                p += eh->length;
                continue;
            }
            memset(kstack, 0, SMP_AP_KSTACK_SIZE);
            const uint64_t stack_top = (uint64_t)(uintptr_t)(kstack + SMP_AP_KSTACK_SIZE);

            g_cpu_table[cpu_id].cpu_id   = cpu_id;
            g_cpu_table[cpu_id].apic_id  = apic_id;
            g_cpu_table[cpu_id].stack_top = stack_top;
            atomic_store(&g_cpu_table[cpu_id].online, 0);

            volatile tramp_data_t *td = TRAMP_DATA_PTR;
            td->pml4   = (uint64_t)bsp_cr3;
            td->stack  = stack_top;
            td->entry  = (uint64_t)(uintptr_t)smp_ap_startup;
            td->cpuid  = cpu_id;
            __asm__ volatile ("" ::: "memory");
            td->gate   = 1;

            smp_send_init_sipi(apic_id);

            smp_pit_ch2_arm(50);
            uint32_t slices = 4;
            while (!atomic_load(&g_cpu_table[cpu_id].online)) {
                if (smp_pit_ch2_expired()) {
                    if (--slices == 0) {
                        uart_puts("SMP: timeout waiting for AP; continuing.\n");
                        break;
                    }
                    smp_pit_ch2_arm(50);
                }
                __asm__ volatile ("pause");
            }

            td->gate = 0;
            __asm__ volatile ("" ::: "memory");

            g_cpu_total = cpu_id + 1;
        }

        p += eh->length;
    }

    kprintf("SMP: %u CPUs online.\n", (unsigned)g_cpu_total);
}

void smp_ap_startup(uint32_t cpu_id)
{
    smp_ap_gdt_init(cpu_id);

    __asm__ volatile ("lidt %0" : : "m"(g_idtr_save) : "memory");

    percpu_t *pc = g_percpu[cpu_id];
    pc->apic_id  = lapic_id();
    percpu_install(pc);

    ap_hw_t *hw = g_ap_hw[cpu_id];
    hw->tss.rsp[0]  = g_cpu_table[cpu_id].stack_top;
    pc->kernel_rsp0 = g_cpu_table[cpu_id].stack_top;

    apic_timer_ap_init();

    extern void sched_ap_init(void);
    sched_ap_init();
    amx_ap_init();

    __atomic_add_fetch(&g_cpu_online_count, 1U, __ATOMIC_SEQ_CST);
    atomic_store(&g_cpu_table[cpu_id].online, 1);

    __asm__ volatile ("sti");

    for (;;)
        __asm__ volatile ("pause; hlt");
}

uint32_t smp_cpu_count(void)
{
    return (uint32_t)__atomic_load_n(&g_cpu_online_count, __ATOMIC_RELAXED);
}

uint32_t smp_current_cpu_id(void)
{
    return PERCPU_ID();
}

int smp_current_cpu(void)
{
    return (int)smp_current_cpu_id();
}

uint32_t smp_current_apic_id(void)
{
    return PERCPU_APIC();
}

uint32_t smp_apic_id(unsigned cpu)
{
    if (cpu >= SMP_MAX_CPUS) return (uint32_t)-1;
    return g_cpu_table[cpu].apic_id;
}
