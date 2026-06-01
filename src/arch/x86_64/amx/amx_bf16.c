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

#include "amx_bf16.h"
#include "amx_tmul.h"
#include "amx_tile.h"
#include <cpuid.h>

int amx_bf16_supported(void)
{
    return !!(g_cpuid.l7_sub1.edx & CPUID_AMX_BF16);
}

void amx_bf16_matmul(const bf16_t *A, const bf16_t *B, float *C,
                     int M, int K, int N)
{
    static amx_tilecfg_t cfg __attribute__((aligned(64)));
    uint8_t  rows[3]  = { (uint8_t)M, (uint8_t)M, (uint8_t)K };
    uint16_t colsb[3] = { (uint16_t)(N * 4),
                          (uint16_t)(K * 2),
                          (uint16_t)(N * 2) };

    amx_tile_config(&cfg, 1, rows, colsb, 3);
    amx_tilezero_0();

    (void)A; (void)B; (void)C;
    amx_tmul_bf16();
    amx_tile_release_all();
}
