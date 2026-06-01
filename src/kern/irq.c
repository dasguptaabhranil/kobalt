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

#include "../inc/irq.h"
#include "../arch/x86_64/idt.h"
#include "../arch/x86_64/gdt.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define PIC_MASTER_CMD      (0x20U)
#define PIC_MASTER_DATA     (0x21U)
#define PIC_SLAVE_CMD       (0xA0U)
#define PIC_SLAVE_DATA      (0xA1U)

#define PIC_ICW1_INIT       (0x10U)
#define PIC_ICW1_ICW4       (0x01U)
#define PIC_ICW1            (PIC_ICW1_INIT | PIC_ICW1_ICW4)

#define PIC_MASTER_ICW3     (0x04U)

#define PIC_SLAVE_ICW3      (0x02U)

#define PIC_ICW4_8086       (0x01U)

#define PIC_CMD_EOI         (0x20U)

#define PIC_MASK_ALL        (0xFFU)

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

static inline void io_wait(void)
{
    outb(0x80U, 0x00U);
}

#define IOAPIC_REG_ID           (0x00U)
#define IOAPIC_REG_VER          (0x01U)
#define IOAPIC_REG_ARB          (0x02U)
#define IOAPIC_REG_RTE_LO(n)    (0x10U + 2U * (n))
#define IOAPIC_REG_RTE_HI(n)    (0x11U + 2U * (n))

#define IOAPIC_RTE_MASKED       (1U << 16)
#define IOAPIC_RTE_LEVEL        (1U << 15)
#define IOAPIC_RTE_LOWPOL       (1U << 13)
#define IOAPIC_RTE_FIXED        (0U <<  8)
#define IOAPIC_RTE_LOWEST       (1U <<  8)

#define IOAPIC_MAX_INPUTS       (24U)

static volatile uint32_t *ioapic_base = (volatile uint32_t *)IRQ_IOAPIC_PHYS_BASE;
static int                ioapic_present = 0;
static uint32_t           ioapic_max_rte = 0;

void irq_set_ioapic_base(uintptr_t phys_addr)
{
    ioapic_base = (volatile uint32_t *)phys_addr;
}

static uint32_t ioapic_read(uint8_t reg)
{
    ioapic_base[0] = reg;
    return ioapic_base[4];
}

static void ioapic_write(uint8_t reg, uint32_t val)
{
    ioapic_base[0] = reg;
    ioapic_base[4] = val;
}

typedef struct
{
    irq_handler_fn_t  handler;
    void             *arg;
    uint8_t           vector;
    uint8_t           claimed;
    const char       *name;
} irq_entry_t;

static irq_entry_t irq_table[IRQ_MAX];

static uint8_t irq_for_vector[256];

static volatile uint32_t irq_spurious_count = 0;

static void spurious_handler(uint8_t irq, void *arg)
{
    (void)irq;
    (void)arg;
    irq_spurious_count++;

}

static void pic_remap(uint8_t master_offset, uint8_t slave_offset)
{

    uint8_t master_mask = inb(PIC_MASTER_DATA);
    uint8_t slave_mask  = inb(PIC_SLAVE_DATA);

    outb(PIC_MASTER_CMD,  PIC_ICW1);   io_wait();
    outb(PIC_SLAVE_CMD,   PIC_ICW1);   io_wait();

    outb(PIC_MASTER_DATA, master_offset);  io_wait();
    outb(PIC_SLAVE_DATA,  slave_offset);   io_wait();

    outb(PIC_MASTER_DATA, PIC_MASTER_ICW3);  io_wait();
    outb(PIC_SLAVE_DATA,  PIC_SLAVE_ICW3);   io_wait();

    outb(PIC_MASTER_DATA, PIC_ICW4_8086);  io_wait();
    outb(PIC_SLAVE_DATA,  PIC_ICW4_8086);  io_wait();

    outb(PIC_MASTER_DATA, master_mask);
    outb(PIC_SLAVE_DATA,  slave_mask);
}

static int ioapic_probe(void)
{
    uint32_t id_reg = ioapic_read(IOAPIC_REG_ID);
    if (id_reg == 0xFFFFFFFFU)
        return 0;

    uint32_t ver = ioapic_read(IOAPIC_REG_VER);
    ioapic_max_rte = (ver >> 16) & 0xFFU;

    for (uint32_t n = 0; n <= ioapic_max_rte; n++) {
        uint32_t lo = ioapic_read(IOAPIC_REG_RTE_LO(n));
        lo |= IOAPIC_RTE_MASKED;
        ioapic_write(IOAPIC_REG_RTE_LO(n), lo);
    }

    return 1;
}

static void ioapic_program_rte(uint32_t irq_input, uint8_t vector,
                                uint8_t lapic_id)
{
    uint32_t lo = (uint32_t)vector
                | IOAPIC_RTE_FIXED
                | IOAPIC_RTE_MASKED;

    uint32_t hi = (uint32_t)lapic_id << 24;

    ioapic_write(IOAPIC_REG_RTE_HI(irq_input), hi);
    ioapic_write(IOAPIC_REG_RTE_LO(irq_input), lo);
}

void irq_init(void)
{

    memset(irq_table,     0,    sizeof(irq_table));
    memset(irq_for_vector, 0xFF, sizeof(irq_for_vector));

    pic_remap(IRQ_PIC_VECTOR_BASE,
              IRQ_PIC_VECTOR_BASE + 8U);

    outb(PIC_MASTER_DATA, PIC_MASK_ALL);
    outb(PIC_SLAVE_DATA,  PIC_MASK_ALL);

    irq_table[7].handler  = spurious_handler;
    irq_table[7].arg      = NULL;
    irq_table[7].vector   = IRQ_PIC_VECTOR_BASE + 7U;
    irq_table[7].claimed  = 1;
    irq_table[7].name     = "pic-spurious-master";
    irq_for_vector[IRQ_PIC_VECTOR_BASE + 7U] = 7U;

    irq_table[15].handler = spurious_handler;
    irq_table[15].arg     = NULL;
    irq_table[15].vector  = IRQ_PIC_VECTOR_BASE + 15U;
    irq_table[15].claimed = 1;
    irq_table[15].name    = "pic-spurious-slave";
    irq_for_vector[IRQ_PIC_VECTOR_BASE + 15U] = 15U;

    ioapic_present = ioapic_probe();

    if (ioapic_present) {
        for (uint32_t n = 0; n <= ioapic_max_rte; n++) {
            uint8_t vec = (uint8_t)(IRQ_IOAPIC_VECTOR_BASE + n);
            ioapic_program_rte(n, vec, 0 );
        }
    }
}

void irq_register(uint8_t irq, uint8_t vector,
                  irq_handler_fn_t handler, void *arg,
                  const char *name)
{

    if (irq >= IRQ_MAX || irq_table[irq].claimed) {

        __asm__ volatile ("cli; 1: hlt; jmp 1b");
        __builtin_unreachable();
    }

    irq_table[irq].handler = handler;
    irq_table[irq].arg     = arg;
    irq_table[irq].vector  = vector;
    irq_table[irq].claimed = 1;
    irq_table[irq].name    = name;
    irq_for_vector[vector] = irq;

    if (ioapic_present && irq <= ioapic_max_rte) {
        ioapic_program_rte(irq, vector, 0 );

    }
}

void irq_dispatch(uint8_t irq)
{
    if (irq >= IRQ_MAX)
        return;

    irq_handler_fn_t h = irq_table[irq].handler;
    if (h)
        h(irq, irq_table[irq].arg);
}

void irq_mask(uint8_t irq)
{
    if (irq >= IRQ_MAX) return;

    if (ioapic_present && irq <= ioapic_max_rte) {

        uint32_t lo = ioapic_read(IOAPIC_REG_RTE_LO(irq));
        lo |= IOAPIC_RTE_MASKED;
        ioapic_write(IOAPIC_REG_RTE_LO(irq), lo);
    } else {

        if (irq < 8U) {
            uint8_t mask = inb(PIC_MASTER_DATA);
            mask |= (uint8_t)(1U << irq);
            outb(PIC_MASTER_DATA, mask);
        } else if (irq < 16U) {
            uint8_t mask = inb(PIC_SLAVE_DATA);
            mask |= (uint8_t)(1U << (irq - 8U));
            outb(PIC_SLAVE_DATA, mask);
        }
    }
}

void irq_unmask(uint8_t irq)
{
    if (irq >= IRQ_MAX) return;

    if (ioapic_present && irq <= ioapic_max_rte) {
        uint32_t lo = ioapic_read(IOAPIC_REG_RTE_LO(irq));
        lo &= ~IOAPIC_RTE_MASKED;
        ioapic_write(IOAPIC_REG_RTE_LO(irq), lo);
    } else {
        if (irq < 8U) {
            uint8_t mask = inb(PIC_MASTER_DATA);
            mask &= (uint8_t)~(1U << irq);
            outb(PIC_MASTER_DATA, mask);
        } else if (irq < 16U) {

            uint8_t slave_mask  = inb(PIC_SLAVE_DATA);
            uint8_t master_mask = inb(PIC_MASTER_DATA);
            slave_mask  &= (uint8_t)~(1U << (irq - 8U));
            master_mask &= (uint8_t)~(1U << 2U);
            outb(PIC_SLAVE_DATA,  slave_mask);
            outb(PIC_MASTER_DATA, master_mask);
        }
    }
}

void irq_pic_eoi(uint8_t irq)
{
    if (irq >= 8U)
        outb(PIC_SLAVE_CMD,  PIC_CMD_EOI);
    outb(PIC_MASTER_CMD, PIC_CMD_EOI);
}
