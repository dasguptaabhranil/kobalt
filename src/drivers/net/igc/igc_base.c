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
/* Portions derived from OpenBSD igc_base.c, BSD-3-Clause */

#include "igc_hw.h"
#include "igc_mac.h"
#include "igc_phy.h"
#include "igc_base.h"

int
igc_acquire_phy_base(struct igc_hw *hw)
{
	uint16_t mask = IGC_SWFW_PHY0_SM;

	if (hw->bus.func == IGC_FUNC_1)
		mask = IGC_SWFW_PHY1_SM;

	return hw->mac.ops.acquire_swfw_sync(hw, mask);
}

void
igc_release_phy_base(struct igc_hw *hw)
{
	uint16_t mask = IGC_SWFW_PHY0_SM;

	if (hw->bus.func == IGC_FUNC_1)
		mask = IGC_SWFW_PHY1_SM;

	hw->mac.ops.release_swfw_sync(hw, mask);
}

int
igc_init_hw_base(struct igc_hw *hw)
{
	struct igc_mac_info *mac = &hw->mac;
	uint16_t i, rar_count = mac->rar_entry_count;
	int ret_val;

	igc_init_rx_addrs_generic(hw, rar_count);

	for (i = 0; i < mac->mta_reg_count; i++)
		IGC_WRITE_REG_ARRAY(hw, IGC_MTA, i, 0);

	for (i = 0; i < mac->uta_reg_count; i++)
		IGC_WRITE_REG_ARRAY(hw, IGC_UTA, i, 0);

	ret_val = mac->ops.setup_link(hw);
	igc_clear_hw_cntrs_base_generic(hw);

	return ret_val;
}

void
igc_power_down_phy_copper_base(struct igc_hw *hw)
{
	struct igc_phy_info *phy = &hw->phy;

	if (!(phy->ops.check_reset_block))
		return;

	if (phy->ops.check_reset_block(hw))
		igc_power_down_phy_copper(hw);
}