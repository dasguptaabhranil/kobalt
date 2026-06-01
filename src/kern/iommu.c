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

#include <iommu.h>
#include <acpi.h>
#include <kfmt.h>

static vtd_root_entry_t g_vtd_root_table[256] __attribute__((aligned(4096)));
static vtd_context_entry_t g_vtd_bus0_ctx[256] __attribute__((aligned(4096)));
static uintptr_t g_vtd_reg_base = 0;

void iommu_init(void) {
    acpi_sdt_hdr_t* dmar = (acpi_sdt_hdr_t*)acpi_find_table("DMAR");
    if (!dmar) return;

    uint8_t* drhd = (uint8_t*)dmar + 48;
    g_vtd_reg_base = *(uintptr_t*)(drhd + 8);

    for (int i = 0; i < 256; i++) {

        g_vtd_bus0_ctx[i].lo = (2ULL << 2) | 0x1;

        g_vtd_bus0_ctx[i].hi = (1ULL << 0) | (1ULL << 8);
    }

    g_vtd_root_table[0].lo = (uintptr_t)g_vtd_bus0_ctx | 0x1;
    *(volatile uint64_t*)(g_vtd_reg_base + 0x20) = (uintptr_t)g_vtd_root_table;
    *(volatile uint32_t*)(g_vtd_reg_base + 0x18) = (1U << 30);
    while(!(*(volatile uint32_t*)(g_vtd_reg_base + 0x1C) & (1U << 30))) cpu_relax();

    *(volatile uint64_t*)(g_vtd_reg_base + 0x28) = (1ULL << 63) | (1ULL << 61);
    while(*(volatile uint64_t*)(g_vtd_reg_base + 0x28) & (1ULL << 63)) cpu_relax();

    *(volatile uint64_t*)(g_vtd_reg_base + 0x108) = (1ULL << 63) | (1ULL << 60);
    while(*(volatile uint64_t*)(g_vtd_reg_base + 0x108) & (1ULL << 63)) cpu_relax();

    *(volatile uint32_t*)(g_vtd_reg_base + 0x18) = (1U << 31);
    while(!(*(volatile uint32_t*)(g_vtd_reg_base + 0x1C) & (1U << 31))) cpu_relax();

    klog_ok("shield", "IOMMU Pass-Through Bridge: READY");
}
