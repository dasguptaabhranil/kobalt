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
/* Portions derived from OpenBSD igc_api.c, BSD-3-Clause */

#include "igc_api.h"
#include "igc_hw.h"

int
igc_init_mac_params(struct igc_hw *hw)
{
	int ret_val = IGC_SUCCESS;

	if (hw->mac.ops.init_params) {
		ret_val = hw->mac.ops.init_params(hw);
		if (ret_val)
			goto out;
	} else {
		ret_val = -IGC_ERR_CONFIG;
	}
out:
	return ret_val;
}

int
igc_init_nvm_params(struct igc_hw *hw)
{
	int ret_val = IGC_SUCCESS;

	if (hw->nvm.ops.init_params) {
		ret_val = hw->nvm.ops.init_params(hw);
		if (ret_val)
			goto out;
	} else {
		ret_val = -IGC_ERR_CONFIG;
	}
out:
	return ret_val;
}

int
igc_init_phy_params(struct igc_hw *hw)
{
	int ret_val = IGC_SUCCESS;

	if (hw->phy.ops.init_params) {
		ret_val = hw->phy.ops.init_params(hw);
		if (ret_val)
			goto out;
	} else {
		ret_val = -IGC_ERR_CONFIG;
	}
out:
	return ret_val;
}

int
igc_set_mac_type(struct igc_hw *hw)
{
	struct igc_mac_info *mac = &hw->mac;
	int ret_val = IGC_SUCCESS;

	switch (hw->device_id) {
	case PCI_PRODUCT_INTEL_I220_V:
	case PCI_PRODUCT_INTEL_I221_V:
	case PCI_PRODUCT_INTEL_I225_BLANK_NVM:
	case PCI_PRODUCT_INTEL_I225_I:
	case PCI_PRODUCT_INTEL_I225_IT:
	case PCI_PRODUCT_INTEL_I225_K:
	case PCI_PRODUCT_INTEL_I225_K2:
	case PCI_PRODUCT_INTEL_I225_LM:
	case PCI_PRODUCT_INTEL_I225_LMVP:
	case PCI_PRODUCT_INTEL_I225_V:
	case PCI_PRODUCT_INTEL_I226_BLANK_NVM:
	case PCI_PRODUCT_INTEL_I226_IT:
	case PCI_PRODUCT_INTEL_I226_LM:
	case PCI_PRODUCT_INTEL_I226_LMVP:
	case PCI_PRODUCT_INTEL_I226_K:
	case PCI_PRODUCT_INTEL_I226_V:
		mac->type = igc_i225;
		break;
	default:
		ret_val = -IGC_ERR_MAC_INIT;
		break;
	}

	return ret_val;
}

int
igc_setup_init_funcs(struct igc_hw *hw, bool init_device)
{
	int ret_val;

	ret_val = igc_set_mac_type(hw);
	if (ret_val)
		goto out;

	if (!hw->hw_addr) {
		ret_val = -IGC_ERR_CONFIG;
		goto out;
	}

	igc_init_mac_ops_generic(hw);
	igc_init_phy_ops_generic(hw);
	igc_init_nvm_ops_generic(hw);

	switch (hw->mac.type) {
	case igc_i225:
		igc_init_function_pointers_i225(hw);
		break;
	default:
		ret_val = -IGC_ERR_CONFIG;
		break;
	}

	if (!(ret_val) && init_device) {
		ret_val = igc_init_mac_params(hw);
		if (ret_val)
			goto out;

		ret_val = igc_init_nvm_params(hw);
		if (ret_val)
			goto out;

		ret_val = igc_init_phy_params(hw);
		if (ret_val)
			goto out;
	}
out:
	return ret_val;
}

void
igc_update_mc_addr_list(struct igc_hw *hw, uint8_t *mc_addr_list,
    uint32_t mc_addr_count)
{
	if (hw->mac.ops.update_mc_addr_list)
		hw->mac.ops.update_mc_addr_list(hw, mc_addr_list, mc_addr_count);
}

int
igc_check_for_link(struct igc_hw *hw)
{
	if (hw->mac.ops.check_for_link)
		return hw->mac.ops.check_for_link(hw);
	return -IGC_ERR_CONFIG;
}

int
igc_reset_hw(struct igc_hw *hw)
{
	if (hw->mac.ops.reset_hw)
		return hw->mac.ops.reset_hw(hw);
	return -IGC_ERR_CONFIG;
}

int
igc_init_hw(struct igc_hw *hw)
{
	if (hw->mac.ops.init_hw)
		return hw->mac.ops.init_hw(hw);
	return -IGC_ERR_CONFIG;
}

int
igc_get_bus_info(struct igc_hw *hw)
{
	if (hw->mac.ops.get_bus_info)
		return hw->mac.ops.get_bus_info(hw);
	return IGC_SUCCESS;
}

int
igc_get_speed_and_duplex(struct igc_hw *hw, uint16_t *speed, uint16_t *duplex)
{
	if (hw->mac.ops.get_link_up_info)
		return hw->mac.ops.get_link_up_info(hw, speed, duplex);
	return -IGC_ERR_CONFIG;
}

int
igc_rar_set(struct igc_hw *hw, uint8_t *addr, uint32_t index)
{
	if (hw->mac.ops.rar_set)
		return hw->mac.ops.rar_set(hw, addr, index);
	return IGC_SUCCESS;
}

int
igc_check_reset_block(struct igc_hw *hw)
{
	if (hw->phy.ops.check_reset_block)
		return hw->phy.ops.check_reset_block(hw);
	return IGC_SUCCESS;
}

int
igc_get_phy_info(struct igc_hw *hw)
{
	if (hw->phy.ops.get_info)
		return hw->phy.ops.get_info(hw);
	return IGC_SUCCESS;
}

int
igc_phy_hw_reset(struct igc_hw *hw)
{
	if (hw->phy.ops.reset)
		return hw->phy.ops.reset(hw);
	return IGC_SUCCESS;
}

int
igc_read_mac_addr(struct igc_hw *hw)
{
	if (hw->mac.ops.read_mac_addr)
		return hw->mac.ops.read_mac_addr(hw);
	return igc_read_mac_addr_generic(hw);
}

int
igc_validate_nvm_checksum(struct igc_hw *hw)
{
	if (hw->nvm.ops.validate)
		return hw->nvm.ops.validate(hw);
	return -IGC_ERR_CONFIG;
}
