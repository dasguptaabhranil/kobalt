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

#include <uacpi/uacpi.h>
#include <uacpi/tables.h>
#include "../../inc/kernel.h"
#include "../../inc/acpi.h"

bool acpi_init(void)
{
    uacpi_status st = uacpi_initialize(0);
    if (uacpi_unlikely_error(st)) {
        uart_puts("acpi: uacpi_initialize failed\n");
        return false;
    }

    st = uacpi_namespace_load();
    if (uacpi_unlikely_error(st)) {
        uart_puts("acpi: namespace load failed\n");
        return false;
    }

    st = uacpi_namespace_initialize();
    if (uacpi_unlikely_error(st)) {
        uart_puts("acpi: namespace init failed\n");
        return false;
    }

    uart_puts("acpi: uACPI ready\n");
    return true;
}

acpi_sdt_hdr_t *acpi_find_table(const char *sig)
{
    uacpi_table tbl;
    uacpi_status st = uacpi_table_find_by_signature(sig, &tbl);
    if (uacpi_unlikely_error(st)) return NULL;
    return (acpi_sdt_hdr_t *)tbl.ptr;
}
