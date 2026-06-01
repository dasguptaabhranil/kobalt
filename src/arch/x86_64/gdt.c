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

#include "gdt.h"
#include "idt.h"
#include <stdint.h>

typedef struct
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit_high;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

_Static_assert(sizeof(gdt_entry_t) == 8,
               "gdt_entry_t must be exactly 8 bytes");

typedef struct
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit_high;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t _reserved;
} __attribute__((packed)) tss_desc_t;

_Static_assert(sizeof(tss_desc_t) == 16,
               "tss_desc_t must be exactly 16 bytes");

typedef struct
{
    uint32_t _reserved0;
    uint64_t rsp[3];
    uint64_t _reserved1;
    uint64_t ist[7];
    uint64_t _reserved2;
    uint16_t _reserved3;
    uint16_t iopb_offset;
} __attribute__((packed)) tss_t;

_Static_assert(sizeof(tss_t) == 104,
               "tss_t must be exactly 104 bytes (SDM §7.7)");

typedef struct
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdtr_t;

#define ACC_PRESENT         (1U << 7)
#define ACC_DPL0            (0U << 5)
#define ACC_DPL3            (3U << 5)
#define ACC_SEGMENT         (1U << 4)
#define ACC_SYSTEM          (0U << 4)
#define ACC_TYPE_CODE       (0x0AU)
#define ACC_TYPE_DATA       (0x02U)
#define ACC_TYPE_TSS_AVAIL  (0x09U)

#define ACCESS_KCODE64  (ACC_PRESENT | ACC_DPL0 | ACC_SEGMENT | ACC_TYPE_CODE)
#define ACCESS_KDATA64  (ACC_PRESENT | ACC_DPL0 | ACC_SEGMENT | ACC_TYPE_DATA)
#define ACCESS_UDATA64  (ACC_PRESENT | ACC_DPL3 | ACC_SEGMENT | ACC_TYPE_DATA)
#define ACCESS_UCODE64  (ACC_PRESENT | ACC_DPL3 | ACC_SEGMENT | ACC_TYPE_CODE)
#define ACCESS_TSS      (ACC_PRESENT | ACC_DPL0 | ACC_SYSTEM  | ACC_TYPE_TSS_AVAIL)

#define FLAGS_CODE64    (0xAU)
#define FLAGS_DATA      (0xCU)
#define FLAGS_TSS       (0x0U)

#define GDT_ENTRIES     7
#define GDT_TSS_IDX     5

static gdt_entry_t gdt_table[GDT_ENTRIES] __attribute__((aligned(16)));

_Static_assert(sizeof(gdt_table) == GDT_ENTRIES * 8,
               "gdt_table size sanity check");

static tss_t tss __attribute__((aligned(16)));

static uint8_t ist_stack_nmi[GDT_IST_STACK_SIZE] __attribute__((aligned(16)));
static uint8_t ist_stack_df [GDT_IST_STACK_SIZE] __attribute__((aligned(16)));
static uint8_t ist_stack_mce[GDT_IST_STACK_SIZE] __attribute__((aligned(16)));

static void gdt_encode_descriptor(gdt_entry_t *out,
                                   uint32_t base, uint32_t limit,
                                   uint8_t access, uint8_t flags)
{
    out->limit_low        = (uint16_t)( limit        & 0xFFFFU);
    out->base_low         = (uint16_t)( base         & 0xFFFFU);
    out->base_mid         = (uint8_t) ((base  >> 16) & 0xFFU);
    out->access           =  access;
    out->flags_limit_high = (uint8_t) (((flags & 0x0FU) << 4)
                                      | ((limit >> 16) & 0x0FU));
    out->base_high        = (uint8_t) ((base  >> 24) & 0xFFU);
}

static void gdt_encode_tss(gdt_entry_t *first_slot, const tss_t *tss_ptr)
{
    const uint64_t base  = (uint64_t)(uintptr_t)tss_ptr;
    const uint32_t limit = (uint32_t)(sizeof(tss_t) - 1);

    tss_desc_t *desc = (tss_desc_t *)first_slot;

    desc->limit_low        = (uint16_t)( limit        & 0xFFFFU);
    desc->base_low         = (uint16_t)( base         & 0xFFFFU);
    desc->base_mid         = (uint8_t) ((base  >> 16) & 0xFFU);
    desc->access           =  ACCESS_TSS;
    desc->flags_limit_high = (uint8_t) (((FLAGS_TSS & 0x0FU) << 4)
                                        | ((limit >> 16) & 0x0FU));
    desc->base_high        = (uint8_t) ((base  >> 24) & 0xFFU);
    desc->base_upper       = (uint32_t)((base  >> 32) & 0xFFFFFFFFU);
    desc->_reserved        =  0;
}

void gdt_tss_set_rsp0(uint64_t rsp0)
{
    tss.rsp[0] = rsp0;
}

void gdt_init(void)
{

    gdt_encode_descriptor(&gdt_table[0], 0, 0, 0, 0);

    gdt_encode_descriptor(&gdt_table[1],
                          0x00000000U, 0xFFFFFU,
                          ACCESS_KCODE64, FLAGS_CODE64);

    gdt_encode_descriptor(&gdt_table[2],
                          0x00000000U, 0xFFFFFU,
                          ACCESS_KDATA64, FLAGS_DATA);

    gdt_encode_descriptor(&gdt_table[3],
                          0x00000000U, 0xFFFFFU,
                          ACCESS_UDATA64, FLAGS_DATA);

    gdt_encode_descriptor(&gdt_table[4],
                          0x00000000U, 0xFFFFFU,
                          ACCESS_UCODE64, FLAGS_CODE64);

    tss.ist[IDT_IST_NMI - 1] = (uint64_t)(uintptr_t)(ist_stack_nmi
                                           + GDT_IST_STACK_SIZE);
    tss.ist[IDT_IST_DF  - 1] = (uint64_t)(uintptr_t)(ist_stack_df
                                           + GDT_IST_STACK_SIZE);
    tss.ist[IDT_IST_MCE - 1] = (uint64_t)(uintptr_t)(ist_stack_mce
                                           + GDT_IST_STACK_SIZE);

    tss.rsp[0] = 0;

    tss.iopb_offset = (uint16_t)sizeof(tss_t);

    gdt_encode_tss(&gdt_table[GDT_TSS_IDX], &tss);

    const gdtr_t gdtr = {
        .limit = (uint16_t)(sizeof(gdt_table) - 1),
        .base  = (uint64_t)(uintptr_t)gdt_table,
    };

    __asm__ volatile ("lgdt %0" : : "m"(gdtr) : "memory");

    __asm__ volatile (
        "pushq  %[kcode64]          \n"
        "leaq   1f(%%rip), %%rax    \n"
        "pushq  %%rax               \n"
        "lretq                      \n"
        "1:                         \n"
        :
        : [kcode64] "i"((uint64_t)GDT_KCODE64)
        : "rax", "memory"
    );

    __asm__ volatile (
        "movw   %[kdata64], %%ax    \n"
        "movw   %%ax, %%ds          \n"
        "movw   %%ax, %%es          \n"
        "movw   %%ax, %%ss          \n"
        "xorw   %%ax, %%ax          \n"
        "movw   %%ax, %%fs          \n"
        "movw   %%ax, %%gs          \n"
        :
        : [kdata64] "i"((uint16_t)GDT_KDATA64)
        : "rax", "memory"
    );

    __asm__ volatile (
        "movw   %[tss_sel], %%ax    \n"
        "ltr    %%ax                \n"
        :
        : [tss_sel] "i"((uint16_t)GDT_TSS_SEL)
        : "rax", "memory"
    );
}
