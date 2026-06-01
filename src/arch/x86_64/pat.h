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
#include <stdint.h>

#define PAT_UC    0x00U
#define PAT_WC    0x01U
#define PAT_WT    0x04U
#define PAT_WP    0x05U
#define PAT_WB    0x06U
#define PAT_UCM   0x07U

#define PAT_SLOT_WB   0
#define PAT_SLOT_WT   1
#define PAT_SLOT_UCM  2
#define PAT_SLOT_UC   3
#define PAT_SLOT_WC   4
#define PAT_SLOT_WP   5

void pat_init(void);
