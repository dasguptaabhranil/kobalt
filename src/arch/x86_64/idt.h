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

#define IDT_GATE_INTERRUPT      (0x8E)
#define IDT_GATE_TRAP           (0x8F)
#define IDT_GATE_INTERRUPT_U3   (0xEE)

#define IDT_IST_NONE    (0)
#define IDT_IST_NMI     (1)
#define IDT_IST_DF      (2)
#define IDT_IST_MCE     (3)

struct interrupt_frame
{
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed));

void idt_init(void);
void idt_set_gate(uint8_t vector, uintptr_t handler,
                  uint16_t selector, uint8_t type_attr, uint8_t ist);
