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

#include "amx_int8.h"
#include "amx_tmul.h"
#include "amx_tile.h"
#include <cpuid.h>

int amx_int8_supported(void)
{
    return !!(g_cpuid.l7_sub1.edx & CPUID_AMX_INT8);
}

void amx_int8_matmul(const int8_t *A, const int8_t *B, int32_t *C,
                     int M, int K, int N, amx_int8_mode_t mode)
{
    static amx_tilecfg_t cfg __attribute__((aligned(64)));
    uint8_t  rows[3]  = { (uint8_t)M, (uint8_t)M, (uint8_t)K };
    uint16_t colsb[3] = { (uint16_t)(N * 4),
                          (uint16_t)K,
                          (uint16_t)N };

    amx_tile_config(&cfg, 1, rows, colsb, 3);
    amx_tilezero_0();

    (void)A; (void)B; (void)C;

    switch (mode) {
    case AMX_INT8_SS: amx_tmul_int8_ss(); break;
    case AMX_INT8_SU: amx_tmul_int8_su(); break;
    case AMX_INT8_US: amx_tmul_int8_us(); break;
    case AMX_INT8_UU: amx_tmul_int8_uu(); break;
    }

    amx_tile_release_all();
}
