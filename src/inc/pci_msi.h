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

#include <pci.h>

#define MSI_MC_OFF          2u
#define MSI_MA_LO_OFF       4u
#define MSI_MA_HI_OFF       8u
#define MSI_MD_OFF_32       8u
#define MSI_MD_OFF_64       12u
#define MSI_MASK_OFF_32     12u
#define MSI_MASK_OFF_64     16u

#define MSI_MC_ENABLE       (1u << 0)
#define MSI_MC_ADDR64       (1u << 7)
#define MSI_MC_PVMASK       (1u << 8)

#define MSIX_MC_OFF         2u
#define MSIX_TBL_OFF        4u
#define MSIX_PBA_OFF        8u

#define MSIX_MC_FMASK       (1u << 14)
#define MSIX_MC_ENABLE      (1u << 15)

#define MSIX_VEC_MASKED     (1u << 0)

#define MSI_ADDR_BASE       0xFEE00000u

typedef void (*msi_handler_fn_t)(int vector, void *arg);

typedef struct {
    int entry;
    int vector;
} msix_entry_t;

uint8_t pci_find_cap(pci_device_t *dev, uint8_t cap_id);

int  pci_enable_msi(pci_device_t *dev, int nvecs);
void pci_disable_msi(pci_device_t *dev);

int  pci_enable_msix(pci_device_t *dev, msix_entry_t *entries, int nvecs);
void pci_disable_msix(pci_device_t *dev);

int  pci_irq_vector(pci_device_t *dev, int nr);

void msi_register_handler(int vector, msi_handler_fn_t fn, void *arg);
