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

#ifndef _IXGBE_DCB_H_
#define _IXGBE_DCB_H_

#define MAX_TRAFFIC_CLASS           8
#define MAX_USER_PRIORITY           8
#define IXGBE_DCB_MAX_TC            MAX_TRAFFIC_CLASS
#define IXGBE_DCB_MAX_USER_PRIORITY MAX_USER_PRIORITY

#define IXGBE_RTRUP2TC_UP_MASK      0x7
#define IXGBE_RTRUP2TC_UP_SHIFT     3

struct ixgbe_dcb_tc_config {
    u8 path[2];
    u8 pfc;
};

struct ixgbe_dcb_config {
    struct ixgbe_dcb_tc_config tc_config[MAX_TRAFFIC_CLASS];
    u8  bw_percentage[2][MAX_TRAFFIC_CLASS];
    bool pfc_mode_enable;
    u8  num_tcs;
    u8  num_tcs_pfc;
};

#endif /* _IXGBE_DCB_H_ */