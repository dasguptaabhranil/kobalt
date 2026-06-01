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

#include "e1000_api.h"

static pci_device_t *osdep_pdev(struct e1000_hw *hw)
{
    return ((struct e1000_osdep *)hw->back)->pdev;
}

void e1000_read_pci_cfg(struct e1000_hw *hw, u32 reg, u16 *value)
{
    *value = pci_read_config16(osdep_pdev(hw), (uint16_t)reg);
}

void e1000_write_pci_cfg(struct e1000_hw *hw, u32 reg, u16 *value)
{
    pci_write_config16(osdep_pdev(hw), (uint16_t)reg, *value);
}

void e1000_pci_set_mwi(struct e1000_hw *hw)
{
    pci_device_t *dev = osdep_pdev(hw);
    uint16_t cmd = pci_read_config16(dev, PCI_CFG_COMMAND);
    pci_write_config16(dev, PCI_CFG_COMMAND,
                       (uint16_t)(cmd | CMD_MEM_WRT_INVALIDATE));
}

void e1000_pci_clear_mwi(struct e1000_hw *hw)
{
    pci_device_t *dev = osdep_pdev(hw);
    uint16_t cmd = pci_read_config16(dev, PCI_CFG_COMMAND);
    pci_write_config16(dev, PCI_CFG_COMMAND,
                       (uint16_t)(cmd & ~(uint16_t)CMD_MEM_WRT_INVALIDATE));
}

static uint8_t find_cap(pci_device_t *dev, uint8_t cap_id)
{
    uint16_t st = pci_read_config16(dev, PCI_CFG_STATUS);
    if (!(st & PCI_STS_CAP_LIST))
        return 0;
    uint8_t ptr = pci_read_config8(dev, PCI_CFG_CAP_PTR) & 0xFCu;
    for (int i = 0; ptr && i < 48; i++) {
        if (pci_read_config8(dev, ptr) == cap_id)
            return ptr;
        ptr = pci_read_config8(dev, (uint16_t)(ptr + 1u)) & 0xFCu;
    }
    return 0;
}

s32 e1000_read_pcie_cap_reg(struct e1000_hw *hw, u32 reg, u16 *value)
{
    pci_device_t *dev = osdep_pdev(hw);
    uint8_t off = find_cap(dev, PCI_CAP_ID_PCIE);
    if (!off) { *value = 0; return -1; }
    *value = pci_read_config16(dev, (uint16_t)(off + reg));
    return 0;
}

s32 e1000_write_pcie_cap_reg(struct e1000_hw *hw, u32 reg, u16 *value)
{
    pci_device_t *dev = osdep_pdev(hw);
    uint8_t off = find_cap(dev, PCI_CAP_ID_PCIE);
    if (!off) return -1;
    pci_write_config16(dev, (uint16_t)(off + reg), *value);
    return 0;
}
