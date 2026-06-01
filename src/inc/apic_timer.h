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

#define APIC_TIMER_VECTOR           (0x40U)
#define APIC_TIMER_DEFAULT_HZ       (1000U)

#define APIC_TIMER_DEFAULT_QUANTUM_MS (5U)

extern volatile uint32_t *lapic_base;
extern uint32_t           apic_ticks_per_ms;
extern uint64_t           tsc_khz;

void     apic_timer_init(void);
void     apic_timer_ap_init(void);
void     apic_timer_tick(void);
void     apic_timer_set_freq(uint32_t hz);
void     apic_timer_set_base(uintptr_t vaddr);
uint64_t apic_timer_ticks(void);
uint64_t apic_timer_uptime_ms(void);

void apic_send_eoi(void);
