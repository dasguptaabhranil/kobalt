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
#include "acpi.h"

#define MADT_TYPE_LOCAL_APIC    0
#define MADT_TYPE_IO_APIC       1
#define MADT_TYPE_ISO           2

typedef struct __attribute__((packed)) {
    acpi_sdt_hdr_t hdr;
    uint32_t lapic_addr;
    uint32_t flags;
} acpi_madt_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t length;
} madt_entry_hdr_t;

typedef struct __attribute__((packed)) {
    madt_entry_hdr_t hdr;
    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t flags;
} madt_lapic_t;

typedef struct __attribute__((packed)) {
    madt_entry_hdr_t hdr;
    uint8_t io_apic_id;
    uint8_t reserved;
    uint32_t io_apic_addr;
    uint32_t gsi_base;
} madt_ioapic_t;

void madt_parse(void);
