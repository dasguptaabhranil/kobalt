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
#include <stddef.h>

#define AMX_MAX_TILES       8
#define AMX_MAX_ROWS        16
#define AMX_MAX_COLSB       64
#define AMX_TILECFG_SIZE    64
#define AMX_TILEDATA_SIZE   8192
#define AMX_XSAVE_HDR_OFF   512
#define AMX_XSAVE_XCOMP_OFF (AMX_XSAVE_HDR_OFF + 8)

#define XCR0_XTILECFG   (1ULL << 17)
#define XCR0_XTILEDATA  (1ULL << 18)
#define XCR0_AMX_MASK   (XCR0_XTILECFG | XCR0_XTILEDATA)

#define MSR_IA32_XFD      0x1C4u
#define MSR_IA32_XFD_ERR  0x1C5u
#define XFD_AMX_BIT       (1ULL << 18)

#define CPUID_AMX_BF16    (1u << 22)
#define CPUID_AMX_TILE    (1u << 24)
#define CPUID_AMX_INT8    (1u << 25)

typedef struct {
    uint8_t  palette_id;
    uint8_t  start_row;
    uint8_t  rsvd0[14];
    uint16_t colsb[AMX_MAX_TILES];
    uint8_t  rows[AMX_MAX_TILES];
    uint8_t  rsvd1[24];
} __attribute__((packed)) amx_tilecfg_t;

_Static_assert(sizeof(amx_tilecfg_t) == AMX_TILECFG_SIZE, "");

extern int      g_amx_supported;
extern uint32_t g_amx_xsave_size;
