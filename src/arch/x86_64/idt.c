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

#include "idt.h"
#include <stddef.h>
#include <stdint.h>

typedef struct
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t _reserved;
} __attribute__((packed)) idt_gate_t;

_Static_assert(sizeof(idt_gate_t) == 16,
               "idt_gate_t must be exactly 16 bytes");

typedef struct
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idtr_t;

static idt_gate_t idt_table[256]
    __attribute__((aligned(4096)));

__attribute__((naked))
static void idt_unhandled_stub(void)
{
    __asm__ volatile (
        "cli          \n"
        "1:           \n"
        "   hlt       \n"
        "   jmp 1b    \n"
    );
}

static void idt_encode_gate(idt_gate_t *gate,
                             uintptr_t   handler,
                             uint16_t    selector,
                             uint8_t     type_attr,
                             uint8_t     ist)
{
    gate->offset_low  = (uint16_t)( handler        & 0xFFFFU);
    gate->selector    =  selector;
    gate->ist         = (uint8_t)  ( ist            & 0x07U);
    gate->type_attr   =  type_attr;
    gate->offset_mid  = (uint16_t)((handler >> 16)  & 0xFFFFU);
    gate->offset_high = (uint32_t)((handler >> 32)  & 0xFFFFFFFFU);
    gate->_reserved   =  0;
}

void idt_set_gate(uint8_t vector, uintptr_t handler,
                  uint16_t selector, uint8_t type_attr, uint8_t ist)
{
    idt_encode_gate(&idt_table[vector], handler, selector, type_attr, ist);
}

void idt_init(void)
{
    const uintptr_t stub_addr = (uintptr_t)idt_unhandled_stub;

    for (unsigned int v = 0; v < 256; v++) {
        idt_encode_gate(&idt_table[v],
                        stub_addr,
                        0x08,
                        IDT_GATE_INTERRUPT,
                        IDT_IST_NONE);
    }

    idt_table[2]._reserved  = 0;
    idt_table[2].ist        = IDT_IST_NMI;

    idt_table[8]._reserved  = 0;
    idt_table[8].ist        = IDT_IST_DF;

    idt_table[18]._reserved = 0;
    idt_table[18].ist       = IDT_IST_MCE;

    const idtr_t idtr = {
        .limit = (uint16_t)(sizeof(idt_table) - 1),
        .base  = (uint64_t)(uintptr_t)idt_table,
    };

    __asm__ volatile (
        "lidt %0"
        :
        : "m"(idtr)
        : "memory"
    );
}
