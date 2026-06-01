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
#include <amx.h>
#include <stdint.h>

typedef enum {
    AMX_INT8_SS,
    AMX_INT8_SU,
    AMX_INT8_US,
    AMX_INT8_UU,
} amx_int8_mode_t;

int  amx_int8_supported(void);
void amx_int8_matmul(const int8_t *A, const int8_t *B, int32_t *C,
                     int M, int K, int N, amx_int8_mode_t mode);
