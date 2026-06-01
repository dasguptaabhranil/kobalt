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

#include "../../inc/madt.h"
#include "../../inc/kfmt.h"

#define MAX_CPUS    256
#define MAX_IOAPICS 8

uint32_t  g_lapic_base;
uint32_t  g_ioapic_base[MAX_IOAPICS];
uint32_t  g_ioapic_gsi_base[MAX_IOAPICS];
uint16_t  g_ioapic_count;
uint8_t   g_apic_ids[MAX_CPUS];
uint16_t  g_cpu_count;

void madt_parse(void)
{
    acpi_madt_t *madt = (acpi_madt_t *)acpi_find_table("APIC");
    if (!madt) {
        klog_fail("madt", "MADT not found");
        return;
    }

    g_lapic_base = madt->lapic_addr;

    uint8_t *p   = (uint8_t *)(madt + 1);
    uint8_t *end = (uint8_t *)madt + madt->hdr.length;

    while (p < end) {
        madt_entry_hdr_t *hdr = (madt_entry_hdr_t *)p;
        if (hdr->length < 2) break;

        switch (hdr->type) {
        case MADT_TYPE_LOCAL_APIC: {
            madt_lapic_t *l = (madt_lapic_t *)p;
            if ((l->flags & 1) && g_cpu_count < MAX_CPUS)
                g_apic_ids[g_cpu_count++] = l->apic_id;
            break;
        }
        case MADT_TYPE_IO_APIC: {
            madt_ioapic_t *io = (madt_ioapic_t *)p;
            if (g_ioapic_count < MAX_IOAPICS) {
                uint8_t i = g_ioapic_count++;
                g_ioapic_base[i]     = io->io_apic_addr;
                g_ioapic_gsi_base[i] = io->gsi_base;
            }
            break;
        }
        default:
            break;
        }

        p += hdr->length;
    }

    char msg[64];
    ksnprintf(msg, sizeof(msg), "%u CPU(s), %u I/O APIC(s), LAPIC @ 0x%x",
              g_cpu_count, g_ioapic_count, g_lapic_base);
    klog_ok("madt", msg);
}
