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

#define GDT_NULL        (0x00U)
#define GDT_KCODE64     (0x08U)
#define GDT_KDATA64     (0x10U)
#define GDT_UDATA64     (0x18U)
#define GDT_UCODE64     (0x20U)
#define GDT_TSS_SEL     (0x28U)

#define GDT_RPL3        (0x03U)

#define GDT_UDATA64_RPL3    ((uint16_t)(GDT_UDATA64 | GDT_RPL3))
#define GDT_UCODE64_RPL3    ((uint16_t)(GDT_UCODE64 | GDT_RPL3))

#define GDT_IST_STACK_SIZE  (0x2000U)

void     gdt_init(void);
void     gdt_tss_set_rsp0(uint64_t rsp0);
