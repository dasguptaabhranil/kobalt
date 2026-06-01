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
#include <stdatomic.h>
#include "percpu.h"

#define SMP_MAX_CPUS            256U
#define SMP_AP_KSTACK_SIZE      0x8000U
#define SMP_AP_IST_SIZE         0x2000U

#define SMP_TRAMP_PHYS          UINT32_C(0x8000)
#define SMP_TRAMP_VECTOR        (SMP_TRAMP_PHYS >> 12)

#define MADT_CPU_ENABLED        (1U << 0)
#define MADT_CPU_ONLINE_CAPABLE (1U << 1)

typedef struct {
    uint32_t        cpu_id;
    uint32_t        apic_id;
    uint64_t        stack_top;
    _Atomic(int)    online;
    uint8_t         _pad[44];
} __attribute__((aligned(64))) cpu_info_t;

_Static_assert(sizeof(cpu_info_t) == 64,
               "cpu_info_t must be exactly 64 bytes (one cache line)");

extern cpu_info_t        g_cpu_table[SMP_MAX_CPUS];
extern volatile uint32_t g_cpu_online_count;
extern uint32_t          g_cpu_total;
extern uint32_t          g_bsp_apic_id;
extern percpu_t          g_percpu_bsp;

void     smp_bsp_early_init(void);
void     smp_init(void);
void     smp_ap_startup(uint32_t cpu_id);
void     smp_ap_gdt_init(uint32_t cpu_id);

uint32_t smp_cpu_count(void);
uint32_t smp_current_cpu_id(void);
int      smp_current_cpu(void);
uint32_t smp_current_apic_id(void);
uint32_t smp_apic_id(unsigned cpu);
