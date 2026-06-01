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

void amx_tdpbf16ps_012(void);
void amx_tdpbssd_012(void);
void amx_tdpbsud_012(void);
void amx_tdpbusd_012(void);
void amx_tdpbuud_012(void);
void amx_tdpfp16ps_012(void);
void amx_tilezero_0(void);

void amx_tmul_bf16(void);
void amx_tmul_int8_ss(void);
void amx_tmul_int8_su(void);
void amx_tmul_int8_us(void);
void amx_tmul_int8_uu(void);
void amx_tmul_fp16(void);
