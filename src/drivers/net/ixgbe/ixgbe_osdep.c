/* Copyright (C) 2026 Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "ixgbe_type.h"
#include "../../../inc/kmalloc.h"

static pci_device_t *osdep_pdev(struct ixgbe_hw *hw)
{
    return ((struct ixgbe_osdep *)hw->back)->pdev;
}

void ixgbe_read_pci_cfg(struct ixgbe_hw *hw, u32 reg, u16 *value)
{
    *value = pci_read_config16(osdep_pdev(hw), (uint16_t)reg);
}

void ixgbe_write_pci_cfg(struct ixgbe_hw *hw, u32 reg, u16 *value)
{
    pci_write_config16(osdep_pdev(hw), (uint16_t)reg, *value);
}

static uint8_t find_pcie_cap(pci_device_t *dev)
{
    uint16_t st = pci_read_config16(dev, PCI_CFG_STATUS);
    if (!(st & PCI_STS_CAP_LIST))
        return 0;
    uint8_t ptr = pci_read_config8(dev, PCI_CFG_CAP_PTR) & 0xFCu;
    for (int i = 0; ptr && i < 48; i++) {
        if (pci_read_config8(dev, ptr) == PCI_CAP_ID_PCIE)
            return ptr;
        ptr = pci_read_config8(dev, (uint16_t)(ptr + 1u)) & 0xFCu;
    }
    return 0;
}

s32 ixgbe_read_pcie_cap_reg(struct ixgbe_hw *hw, u32 reg, u16 *value)
{
    pci_device_t *dev = osdep_pdev(hw);
    uint8_t off = find_pcie_cap(dev);
    if (!off) { *value = 0; return -1; }
    *value = pci_read_config16(dev, (uint16_t)(off + reg));
    return 0;
}

s32 ixgbe_write_pcie_cap_reg(struct ixgbe_hw *hw, u32 reg, u16 *value)
{
    pci_device_t *dev = osdep_pdev(hw);
    uint8_t off = find_pcie_cap(dev);
    if (!off) return -1;
    pci_write_config16(dev, (uint16_t)(off + reg), *value);
    return 0;
}

void ixgbe_init_lock(struct ixgbe_lock *l)    { l->locked = 0; }
void ixgbe_acquire_lock(struct ixgbe_lock *l) { while (__sync_lock_test_and_set(&l->locked, 1)); }
void ixgbe_release_lock(struct ixgbe_lock *l) { __sync_lock_release(&l->locked); }
void ixgbe_destroy_lock(struct ixgbe_lock *l) { l->locked = 0; }

void ixgbe_info_fwlog(struct ixgbe_hw *hw, uint32_t rowsize,
                      uint32_t groupsize, uint8_t *buf, size_t len)
{
    (void)hw; (void)rowsize; (void)groupsize; (void)buf; (void)len;
}