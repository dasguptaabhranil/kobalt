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
#include "msr.h"
#include "../inc/kernel.h"

typedef struct {
    uint64_t arch_caps;
    uint8_t  has_ibrs;
    uint8_t  has_ibpb;
    uint8_t  has_stibp;
    uint8_t  has_ssbd;
    uint8_t  has_arch_caps;
    uint8_t  has_l1d_flush;
    uint8_t  mds_no;
    uint8_t  rdcl_no;
    uint8_t  taa_no;
    uint8_t  ibrs_always_on;
} spec_state_t;

extern spec_state_t g_spec;

void speculation_init(void);

static __inline__ void ibpb_flush(void)
{
    if (g_spec.has_ibpb)
        wrmsr(MSR_IA32_PRED_CMD, PRED_CMD_IBPB);
}

static __inline__ void l1d_flush(void)
{
    if (g_spec.has_l1d_flush)
        wrmsr(MSR_IA32_FLUSH_CMD, FLUSH_CMD_L1D);
}

static __inline__ void swapgs_barrier(void)
{
    __asm__ volatile ("lfence" ::: "memory");
}
