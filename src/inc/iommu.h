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
#include <kernel.h>

#define VTD_VER_REG         0x00
#define VTD_CAP_REG         0x08
#define VTD_ECAP_REG        0x10
#define VTD_GCMD_REG        0x18
#define VTD_GSTS_REG        0x1C
#define VTD_RTADDR_REG      0x20
#define VTD_CCMD_REG        0x28

#define VTD_GCMD_TE         (1U << 31)
#define VTD_GCMD_SRTP       (1U << 30)

#define VTD_GSTS_TES        (1U << 31)
#define VTD_GSTS_RTPS       (1U << 30)

#define SLPT_READ           (1ULL << 0)
#define SLPT_WRITE          (1ULL << 1)

#define VTD_CTX_PRESENT     (1ULL << 0)
#define VTD_CTX_TT_SLPT     (0ULL << 2)
#define VTD_CTX_AW_39BIT    (1ULL << 0)
#define VTD_CTX_AW_48BIT    (2ULL << 0)
#define VTD_CTX_DID(id)     ((uint64_t)(id) << 8)

typedef struct __attribute__((packed)) {
    uint64_t lo;
    uint64_t hi;
} vtd_root_entry_t;

typedef struct __attribute__((packed)) {
    uint64_t lo;
    uint64_t hi;
} vtd_context_entry_t;

void iommu_init(void);
