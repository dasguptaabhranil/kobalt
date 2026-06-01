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

#define DR6_B0      (1U << 0)
#define DR6_B1      (1U << 1)
#define DR6_B2      (1U << 2)
#define DR6_B3      (1U << 3)
#define DR6_BD      (1U << 13)
#define DR6_BS      (1U << 14)
#define DR6_BT      (1U << 15)
#define DR6_INIT    (0xFFFF0FF0U)

#define DR7_L0      (1U << 0)
#define DR7_G0      (1U << 1)
#define DR7_L1      (1U << 2)
#define DR7_G1      (1U << 3)
#define DR7_L2      (1U << 4)
#define DR7_G2      (1U << 5)
#define DR7_L3      (1U << 6)
#define DR7_G3      (1U << 7)
#define DR7_GD      (1U << 13)

#define DR7_COND(n, c)  ((uint32_t)((c) & 3U) << (16 + (n) * 4))

#define DR7_SIZE(n, s)  ((uint32_t)((s) & 3U) << (18 + (n) * 4))

void     debug_init(void);
uint64_t debug_dr6(void);
void     debug_clear_dr6(void);
void     debug_set_dr7(uint64_t val);
void     debug_set_dr(int n, uintptr_t addr);
