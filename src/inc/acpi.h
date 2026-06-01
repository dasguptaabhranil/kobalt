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

#ifndef ACPI_H
#define ACPI_H

#include <kernel.h>
#include <stdbool.h>

typedef struct __attribute__((packed)) {
    char        signature[4];
    uint32_t    length;
    uint8_t     revision;
    uint8_t     checksum;
    char        oem_id[6];
    char        oem_table_id[8];
    uint32_t    oem_revision;
    uint32_t    creator_id;
    uint32_t    creator_revision;
} acpi_sdt_hdr_t;

_Static_assert(sizeof(acpi_sdt_hdr_t) == 36,
               "acpi_sdt_hdr_t ABI break: must be exactly 36 bytes");

typedef struct __attribute__((packed)) {
    char        signature[8];
    uint8_t     checksum;
    char        oem_id[6];
    uint8_t     revision;
    uint32_t    rsdt_addr;
    uint32_t    length;
    uint64_t    xsdt_addr;
    uint8_t     ext_checksum;
    uint8_t     _reserved[3];
} acpi_rsdp_t;

_Static_assert(sizeof(acpi_rsdp_t) == 36,
               "acpi_rsdp_t ABI break: must be exactly 36 bytes");

typedef struct __attribute__((packed)) {
    acpi_sdt_hdr_t  hdr;
    uint32_t        entries[];
} acpi_rsdt_t;

typedef struct __attribute__((packed)) {
    acpi_sdt_hdr_t  hdr;
    uint64_t        entries[];
} acpi_xsdt_t;

typedef struct __attribute__((packed)) {
    uint64_t base_addr;
    uint16_t pci_segment;
    uint8_t  start_bus;
    uint8_t  end_bus;
    uint32_t reserved;
} acpi_mcfg_entry_t;

typedef struct __attribute__((packed)) {
    acpi_sdt_hdr_t    hdr;
    uint64_t          reserved;
    acpi_mcfg_entry_t entries[];
} acpi_mcfg_t;

bool            acpi_init(void);
acpi_sdt_hdr_t *acpi_find_table(const char *sig);

#endif
