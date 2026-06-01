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

#include <pci_msi.h>
#include <pci.h>
#include <madt.h>
#include <acpi.h>
#include "../../arch/x86_64/idt.h"
#include <stdint.h>
#include <stddef.h>

#define MSI_VEC_BASE        0x60u
#define MSI_VEC_COUNT       64u
#define MSI_DEV_MAX         16u
#define LAPIC_EOI_OFF       0xB0u
#define LAPIC_ID_OFF        0x20u
#define MAX_CPUS            64u

#define MADT_LAPIC_TYPE     0u
#define MADT_LAPIC_FL_EN    (1u << 0)
#define MADT_SDT_LEN_OFF    4u
#define MADT_ICS_OFF        44u

typedef struct {
    msi_handler_fn_t fn;
    void            *arg;
    uint8_t          cpu;
} msi_slot_t;

typedef struct {
    pci_device_t        *dev;
    volatile uint32_t   *msix_tbl;
    volatile uint32_t   *msix_pba;
    uint8_t              base_slot;
    uint8_t              nvecs;
    uint8_t              cap_off;
    uint8_t              msix;
} dev_msi_t;

static msi_slot_t g_slots[MSI_VEC_COUNT];
static dev_msi_t  g_devs[MSI_DEV_MAX];
static uint64_t   g_vec_bitmap;
static uintptr_t  g_lapic;
static uint8_t    g_lapic_ids[MAX_CPUS];
static uint32_t   g_ncpus;
static uint8_t    g_next_cpu;

void msi_c_entry(uint64_t slot);

#define DECL_MSI_STUB(n)                                                        \
static __attribute__((naked, used)) void msi_stub_##n(void) {                  \
    __asm__(                                                                    \
        "pushq %rax\n\t"                                                        \
        "pushq %rcx\n\t"                                                        \
        "pushq %rdx\n\t"                                                        \
        "pushq %rsi\n\t"                                                        \
        "pushq %rdi\n\t"                                                        \
        "pushq %r8\n\t"                                                         \
        "pushq %r9\n\t"                                                         \
        "pushq %r10\n\t"                                                        \
        "pushq %r11\n\t"                                                        \
        "movq $" #n ", %rdi\n\t"                                                \
        "movabsq $msi_c_entry, %r10\n\t"                                        \
        "callq *%r10\n\t"                                                       \
        "popq %r11\n\t"                                                         \
        "popq %r10\n\t"                                                         \
        "popq %r9\n\t"                                                          \
        "popq %r8\n\t"                                                          \
        "popq %rdi\n\t"                                                         \
        "popq %rsi\n\t"                                                         \
        "popq %rdx\n\t"                                                         \
        "popq %rcx\n\t"                                                         \
        "popq %rax\n\t"                                                         \
        "iretq\n\t"                                                             \
    );                                                                          \
}

DECL_MSI_STUB(0)  DECL_MSI_STUB(1)  DECL_MSI_STUB(2)  DECL_MSI_STUB(3)
DECL_MSI_STUB(4)  DECL_MSI_STUB(5)  DECL_MSI_STUB(6)  DECL_MSI_STUB(7)
DECL_MSI_STUB(8)  DECL_MSI_STUB(9)  DECL_MSI_STUB(10) DECL_MSI_STUB(11)
DECL_MSI_STUB(12) DECL_MSI_STUB(13) DECL_MSI_STUB(14) DECL_MSI_STUB(15)
DECL_MSI_STUB(16) DECL_MSI_STUB(17) DECL_MSI_STUB(18) DECL_MSI_STUB(19)
DECL_MSI_STUB(20) DECL_MSI_STUB(21) DECL_MSI_STUB(22) DECL_MSI_STUB(23)
DECL_MSI_STUB(24) DECL_MSI_STUB(25) DECL_MSI_STUB(26) DECL_MSI_STUB(27)
DECL_MSI_STUB(28) DECL_MSI_STUB(29) DECL_MSI_STUB(30) DECL_MSI_STUB(31)
DECL_MSI_STUB(32) DECL_MSI_STUB(33) DECL_MSI_STUB(34) DECL_MSI_STUB(35)
DECL_MSI_STUB(36) DECL_MSI_STUB(37) DECL_MSI_STUB(38) DECL_MSI_STUB(39)
DECL_MSI_STUB(40) DECL_MSI_STUB(41) DECL_MSI_STUB(42) DECL_MSI_STUB(43)
DECL_MSI_STUB(44) DECL_MSI_STUB(45) DECL_MSI_STUB(46) DECL_MSI_STUB(47)
DECL_MSI_STUB(48) DECL_MSI_STUB(49) DECL_MSI_STUB(50) DECL_MSI_STUB(51)
DECL_MSI_STUB(52) DECL_MSI_STUB(53) DECL_MSI_STUB(54) DECL_MSI_STUB(55)
DECL_MSI_STUB(56) DECL_MSI_STUB(57) DECL_MSI_STUB(58) DECL_MSI_STUB(59)
DECL_MSI_STUB(60) DECL_MSI_STUB(61) DECL_MSI_STUB(62) DECL_MSI_STUB(63)

typedef void (*stub_fn_t)(void);
static stub_fn_t const g_stubs[MSI_VEC_COUNT] = {
    msi_stub_0,  msi_stub_1,  msi_stub_2,  msi_stub_3,
    msi_stub_4,  msi_stub_5,  msi_stub_6,  msi_stub_7,
    msi_stub_8,  msi_stub_9,  msi_stub_10, msi_stub_11,
    msi_stub_12, msi_stub_13, msi_stub_14, msi_stub_15,
    msi_stub_16, msi_stub_17, msi_stub_18, msi_stub_19,
    msi_stub_20, msi_stub_21, msi_stub_22, msi_stub_23,
    msi_stub_24, msi_stub_25, msi_stub_26, msi_stub_27,
    msi_stub_28, msi_stub_29, msi_stub_30, msi_stub_31,
    msi_stub_32, msi_stub_33, msi_stub_34, msi_stub_35,
    msi_stub_36, msi_stub_37, msi_stub_38, msi_stub_39,
    msi_stub_40, msi_stub_41, msi_stub_42, msi_stub_43,
    msi_stub_44, msi_stub_45, msi_stub_46, msi_stub_47,
    msi_stub_48, msi_stub_49, msi_stub_50, msi_stub_51,
    msi_stub_52, msi_stub_53, msi_stub_54, msi_stub_55,
    msi_stub_56, msi_stub_57, msi_stub_58, msi_stub_59,
    msi_stub_60, msi_stub_61, msi_stub_62, msi_stub_63,
};

static void ensure_lapic(void)
{
    if (g_lapic)
        return;
    acpi_madt_t *m = (acpi_madt_t *)acpi_find_table("APIC");
    if (m)
        g_lapic = (uintptr_t)m->lapic_addr;
}

static void parse_lapic_ids(void)
{
    if (g_ncpus)
        return;
    acpi_madt_t *m = (acpi_madt_t *)acpi_find_table("APIC");
    if (!m) {
        g_lapic_ids[0] = 0;
        g_ncpus        = 1;
        return;
    }
    uint32_t tbl_len = *(uint32_t *)((uint8_t *)m + MADT_SDT_LEN_OFF);
    uint32_t off = MADT_ICS_OFF;
    while (off + 2u <= tbl_len) {
        uint8_t *e = (uint8_t *)m + off;
        if (e[1] < 2u)
            break;
        if (e[0] == MADT_LAPIC_TYPE && e[1] >= 8u && g_ncpus < MAX_CPUS) {
            uint32_t flags;
            __builtin_memcpy(&flags, e + 4u, sizeof(flags));
            if (flags & MADT_LAPIC_FL_EN)
                g_lapic_ids[g_ncpus++] = e[3];
        }
        off += e[1];
    }
    if (!g_ncpus) {
        g_lapic_ids[0] = 0;
        g_ncpus        = 1;
    }
}

static uint8_t next_lapic_id(void)
{
    parse_lapic_ids();
    uint8_t id = g_lapic_ids[g_next_cpu % g_ncpus];
    g_next_cpu  = (uint8_t)((g_next_cpu + 1u) % (uint8_t)g_ncpus);
    return id;
}

static uint32_t msi_addr_for_cpu(uint8_t lapic_id)
{
    return MSI_ADDR_BASE | ((uint32_t)lapic_id << 12);
}

static void lapic_eoi(void)
{
    if (g_lapic)
        *(volatile uint32_t *)(g_lapic + LAPIC_EOI_OFF) = 0;
}

void msi_c_entry(uint64_t slot)
{
    if (slot >= MSI_VEC_COUNT) {
        lapic_eoi();
        return;
    }
    if (g_slots[slot].fn)
        g_slots[slot].fn((int)(MSI_VEC_BASE + slot), g_slots[slot].arg);
    lapic_eoi();
}

static int alloc_slots(int n, int need_align)
{
    int step = need_align ? n : 1;
    uint64_t mask = (n < 64) ? (((uint64_t)1 << n) - 1u) : ~(uint64_t)0;
    for (int i = 0; i + n <= (int)MSI_VEC_COUNT; i += step) {
        if (!(g_vec_bitmap & (mask << i))) {
            g_vec_bitmap |= (mask << i);
            return i;
        }
    }
    return -1;
}

static void free_slots(int base, int n)
{
    uint64_t mask = (n < 64) ? (((uint64_t)1 << n) - 1u) : ~(uint64_t)0;
    g_vec_bitmap &= ~(mask << base);
}

static dev_msi_t *find_dev(pci_device_t *dev)
{
    for (unsigned i = 0; i < MSI_DEV_MAX; i++)
        if (g_devs[i].dev == dev)
            return &g_devs[i];
    return NULL;
}

static dev_msi_t *alloc_dev(pci_device_t *dev)
{
    for (unsigned i = 0; i < MSI_DEV_MAX; i++) {
        if (!g_devs[i].dev) {
            g_devs[i].dev = dev;
            return &g_devs[i];
        }
    }
    return NULL;
}

uint8_t pci_find_cap(pci_device_t *dev, uint8_t cap_id)
{
    uint16_t sts = pci_read_config16(dev, PCI_CFG_STATUS);
    if (!(sts & PCI_STS_CAP_LIST))
        return 0;
    uint8_t ptr = pci_read_config8(dev, PCI_CFG_CAP_PTR) & 0xFCu;
    for (int i = 0; ptr >= 0x40u && i < 48; i++) {
        if (pci_read_config8(dev, ptr) == cap_id)
            return ptr;
        ptr = pci_read_config8(dev, (uint16_t)(ptr + 1u)) & 0xFCu;
    }
    return 0;
}

int pci_enable_msi(pci_device_t *dev, int nvecs)
{
    if (!dev || nvecs <= 0)
        return -1;
    if (find_dev(dev))
        return -1;

    uint8_t cap = pci_find_cap(dev, PCI_CAP_ID_MSI);
    if (!cap)
        return -1;

    uint16_t mc  = pci_read_config16(dev, (uint16_t)(cap + MSI_MC_OFF));
    int      mmc = (mc >> 1) & 0x7;
    int      max = 1 << mmc;

    if (nvecs > max)
        nvecs = max;

    int mme = 0;
    while ((1 << mme) < nvecs)
        mme++;
    int actual = 1 << mme;

    int base = alloc_slots(actual, 1);
    if (base < 0)
        return -1;

    dev_msi_t *d = alloc_dev(dev);
    if (!d) {
        free_slots(base, actual);
        return -1;
    }

    ensure_lapic();
    uint8_t  lapic_id = next_lapic_id();
    uint32_t msi_addr = msi_addr_for_cpu(lapic_id);

    pci_write_config16(dev, (uint16_t)(cap + MSI_MC_OFF),
                       mc & ~(uint16_t)MSI_MC_ENABLE);

    uint8_t  vec0   = (uint8_t)(MSI_VEC_BASE + base);
    int      addr64 = (mc & MSI_MC_ADDR64) ? 1 : 0;
    uint16_t md_off = (uint16_t)(cap + (addr64 ? MSI_MD_OFF_64 : MSI_MD_OFF_32));

    pci_write_config32(dev, (uint16_t)(cap + MSI_MA_LO_OFF), msi_addr);
    if (addr64)
        pci_write_config32(dev, (uint16_t)(cap + MSI_MA_HI_OFF), 0u);

    pci_write_config16(dev, md_off, (uint16_t)vec0);

    if (mc & MSI_MC_PVMASK) {
        uint16_t mask_off = (uint16_t)(cap + (addr64 ? MSI_MASK_OFF_64
                                                      : MSI_MASK_OFF_32));
        pci_write_config32(dev, mask_off, 0u);
    }

    mc = (uint16_t)(mc & ~(uint16_t)0x0070u);
    mc = (uint16_t)(mc | (uint16_t)((unsigned)mme << 4));
    mc = (uint16_t)(mc | (uint16_t)MSI_MC_ENABLE);
    pci_write_config16(dev, (uint16_t)(cap + MSI_MC_OFF), mc);

    uint16_t cmd = pci_read_config16(dev, PCI_CFG_COMMAND);
    pci_write_config16(dev, PCI_CFG_COMMAND,
                       (uint16_t)(cmd | PCI_CMD_INTX_DISABLE));

    for (int i = 0; i < actual; i++) {
        int sl = base + i;
        g_slots[sl].cpu = lapic_id;
        idt_set_gate((uint8_t)(MSI_VEC_BASE + sl),
                     (uintptr_t)g_stubs[sl],
                     0x08u, IDT_GATE_INTERRUPT, IDT_IST_NONE);
    }

    d->base_slot = (uint8_t)base;
    d->nvecs     = (uint8_t)actual;
    d->cap_off   = cap;
    d->msix      = 0;
    d->msix_tbl  = NULL;
    d->msix_pba  = NULL;

    return (int)(MSI_VEC_BASE + base);
}

void pci_disable_msi(pci_device_t *dev)
{
    dev_msi_t *d = find_dev(dev);
    if (!d || d->msix)
        return;

    uint16_t mc = pci_read_config16(dev, (uint16_t)(d->cap_off + MSI_MC_OFF));
    pci_write_config16(dev, (uint16_t)(d->cap_off + MSI_MC_OFF),
                       mc & ~(uint16_t)MSI_MC_ENABLE);

    uint16_t cmd = pci_read_config16(dev, PCI_CFG_COMMAND);
    pci_write_config16(dev, PCI_CFG_COMMAND,
                       (uint16_t)(cmd & ~(uint16_t)PCI_CMD_INTX_DISABLE));

    for (int i = 0; i < d->nvecs; i++) {
        int s = d->base_slot + i;
        g_slots[s].fn  = NULL;
        g_slots[s].arg = NULL;
        g_slots[s].cpu = 0;
    }

    free_slots(d->base_slot, d->nvecs);
    d->dev = NULL;
}

int pci_enable_msix(pci_device_t *dev, msix_entry_t *entries, int nvecs)
{
    if (!dev || !entries || nvecs <= 0)
        return -1;
    if (find_dev(dev))
        return -1;

    uint8_t cap = pci_find_cap(dev, PCI_CAP_ID_MSIX);
    if (!cap)
        return -1;

    uint16_t mc  = pci_read_config16(dev, (uint16_t)(cap + MSIX_MC_OFF));
    int      tsz = (int)((mc & 0x07FFu) + 1u);

    if (nvecs > tsz)
        nvecs = tsz;

    uint32_t  tbl_raw = pci_read_config32(dev, (uint16_t)(cap + MSIX_TBL_OFF));
    uint8_t   tbl_bir = (uint8_t)(tbl_raw & 0x7u);
    uintptr_t tbl_base = dev->bar[tbl_bir] + (uintptr_t)(tbl_raw & ~(uint32_t)0x7u);

    uint32_t  pba_raw  = pci_read_config32(dev, (uint16_t)(cap + MSIX_PBA_OFF));
    uint8_t   pba_bir  = (uint8_t)(pba_raw & 0x7u);
    uintptr_t pba_base = dev->bar[pba_bir] + (uintptr_t)(pba_raw & ~(uint32_t)0x7u);

    int base = alloc_slots(nvecs, 0);
    if (base < 0)
        return -1;

    dev_msi_t *d = alloc_dev(dev);
    if (!d) {
        free_slots(base, nvecs);
        return -1;
    }

    ensure_lapic();

    pci_write_config16(dev, (uint16_t)(cap + MSIX_MC_OFF),
                       (uint16_t)(mc | MSIX_MC_FMASK));

    volatile uint32_t *tbl = (volatile uint32_t *)tbl_base;
    volatile uint32_t *pba = (volatile uint32_t *)pba_base;

    for (int i = 0; i < nvecs; i++) {
        int     ei      = entries[i].entry;
        int     sl      = base + i;
        uint8_t lapic   = next_lapic_id();
        uint8_t vec     = (uint8_t)(MSI_VEC_BASE + sl);

        entries[i].vector  = (int)vec;
        g_slots[sl].cpu    = lapic;

        if (ei < 0 || ei >= tsz)
            continue;

        volatile uint32_t *e = tbl + (unsigned)ei * 4u;
        e[0] = msi_addr_for_cpu(lapic);
        e[1] = 0u;
        e[2] = (uint32_t)vec;
        e[3] = 0u;

        idt_set_gate(vec, (uintptr_t)g_stubs[sl],
                     0x08u, IDT_GATE_INTERRUPT, IDT_IST_NONE);
    }

    uint16_t cmd = pci_read_config16(dev, PCI_CFG_COMMAND);
    pci_write_config16(dev, PCI_CFG_COMMAND,
                       (uint16_t)(cmd | PCI_CMD_INTX_DISABLE));

    mc = (uint16_t)(mc & ~(uint16_t)MSIX_MC_FMASK);
    mc = (uint16_t)(mc | (uint16_t)MSIX_MC_ENABLE);
    pci_write_config16(dev, (uint16_t)(cap + MSIX_MC_OFF), mc);

    d->base_slot = (uint8_t)base;
    d->nvecs     = (uint8_t)nvecs;
    d->cap_off   = cap;
    d->msix      = 1;
    d->msix_tbl  = tbl;
    d->msix_pba  = pba;

    int pba_words = (tsz + 63) / 64;
    for (int w = 0; w < pba_words; w++) {
        uint32_t lo = pba[(unsigned)w * 2u];
        uint32_t hi = pba[(unsigned)w * 2u + 1u];
        uint64_t bits = ((uint64_t)hi << 32) | lo;
        while (bits) {
            int bit = __builtin_ctzll(bits);
            int ei  = w * 64 + bit;
            for (int i = 0; i < nvecs; i++) {
                if (entries[i].entry == ei && g_slots[base + i].fn) {
                    int sl = base + i;
                    g_slots[sl].fn((int)(MSI_VEC_BASE + sl), g_slots[sl].arg);
                }
            }
            bits &= bits - 1u;
        }
    }

    return 0;
}

void pci_disable_msix(pci_device_t *dev)
{
    dev_msi_t *d = find_dev(dev);
    if (!d || !d->msix)
        return;

    uint16_t mc = pci_read_config16(dev, (uint16_t)(d->cap_off + MSIX_MC_OFF));
    pci_write_config16(dev, (uint16_t)(d->cap_off + MSIX_MC_OFF),
                       mc & ~(uint16_t)MSIX_MC_ENABLE);

    if (d->msix_tbl) {
        for (int i = 0; i < d->nvecs; i++)
            d->msix_tbl[(unsigned)i * 4u + 3u] = MSIX_VEC_MASKED;
    }

    uint16_t cmd = pci_read_config16(dev, PCI_CFG_COMMAND);
    pci_write_config16(dev, PCI_CFG_COMMAND,
                       (uint16_t)(cmd & ~(uint16_t)PCI_CMD_INTX_DISABLE));

    for (int i = 0; i < d->nvecs; i++) {
        int s = d->base_slot + i;
        g_slots[s].fn  = NULL;
        g_slots[s].arg = NULL;
        g_slots[s].cpu = 0;
    }

    free_slots(d->base_slot, d->nvecs);
    d->dev      = NULL;
    d->msix_tbl = NULL;
    d->msix_pba = NULL;
}

int pci_irq_vector(pci_device_t *dev, int nr)
{
    dev_msi_t *d = find_dev(dev);
    if (!d || nr < 0 || nr >= (int)d->nvecs)
        return -1;
    return (int)(MSI_VEC_BASE + d->base_slot + nr);
}

void msi_register_handler(int vector, msi_handler_fn_t fn, void *arg)
{
    int sl = vector - (int)MSI_VEC_BASE;
    if (sl < 0 || sl >= (int)MSI_VEC_COUNT)
        return;
    g_slots[sl].fn  = fn;
    g_slots[sl].arg = arg;
}
