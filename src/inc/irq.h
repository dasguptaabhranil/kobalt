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

typedef void (*irq_handler_fn_t)(uint8_t irq, void *arg);

#define IRQ_MAX             (64U)

#define IRQ_TIMER           (0U)
#define IRQ_KEYBOARD        (1U)
#define IRQ_CASCADE         (2U)
#define IRQ_UART_COM2       (3U)
#define IRQ_UART_COM1       (4U)
#define IRQ_LPT2            (5U)
#define IRQ_FLOPPY          (6U)
#define IRQ_LPT1            (7U)
#define IRQ_RTC             (8U)
#define IRQ_ACPI            (9U)
#define IRQ_NIC             (11U)
#define IRQ_MOUSE           (12U)
#define IRQ_FPU             (13U)
#define IRQ_ATA_PRIMARY     (14U)
#define IRQ_ATA_SECONDARY   (15U)

#define IRQ_PIC_VECTOR_BASE     (0x20U)
#define IRQ_IOAPIC_VECTOR_BASE  (0x30U)

#define IRQ_IOAPIC_PHYS_BASE    (0xFEC00000UL)

void irq_init(void);
void irq_register(uint8_t irq, uint8_t vector,
                  irq_handler_fn_t handler, void *arg,
                  const char *name);
void irq_dispatch(uint8_t irq);
void irq_mask  (uint8_t irq);
void irq_unmask(uint8_t irq);
void irq_pic_eoi(uint8_t irq);
void irq_set_ioapic_base(uintptr_t phys_addr);
