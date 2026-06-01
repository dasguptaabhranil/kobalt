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
/* Portions derived from OpenBSD igc_api.h, BSD-3-Clause */

#ifndef _IGC_API_H_
#define _IGC_API_H_

#include "igc_hw.h"
#include "igc_mac.h"
#include "igc_phy.h"
#include "igc_nvm.h"

#ifndef IGC_UNUSEDARG
#define IGC_UNUSEDARG  __attribute__((unused))
#endif

extern void igc_init_function_pointers_i225(struct igc_hw *);

int  igc_set_mac_type(struct igc_hw *);
int  igc_setup_init_funcs(struct igc_hw *, bool);
int  igc_init_mac_params(struct igc_hw *);
int  igc_init_nvm_params(struct igc_hw *);
int  igc_init_phy_params(struct igc_hw *);
int  igc_check_for_link(struct igc_hw *);
int  igc_reset_hw(struct igc_hw *);
int  igc_init_hw(struct igc_hw *);
int  igc_get_bus_info(struct igc_hw *);
int  igc_get_speed_and_duplex(struct igc_hw *, uint16_t *, uint16_t *);
int  igc_rar_set(struct igc_hw *, uint8_t *, uint32_t);
void igc_update_mc_addr_list(struct igc_hw *, uint8_t *, uint32_t);
int  igc_check_reset_block(struct igc_hw *);
int  igc_get_phy_info(struct igc_hw *);
int  igc_phy_hw_reset(struct igc_hw *);
int  igc_read_mac_addr(struct igc_hw *);
int  igc_validate_nvm_checksum(struct igc_hw *);

#endif /* _IGC_API_H_ */