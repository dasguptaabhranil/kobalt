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

#ifndef CPUIDLE_H
#define CPUIDLE_H

#include <stdint.h>

#define CPUIDLE_MAX_STATES  8

typedef struct {
    const char *name;
    uint32_t    hint;
    uint32_t    exit_latency_us;
    uint32_t    target_residency_us;
    uint64_t    usage_count;
    uint64_t    total_time_us;
} cpuidle_state_t;

typedef struct {
    int             nstates;
    int             cur_state;
    int             mwait_supported;
    cpuidle_state_t states[CPUIDLE_MAX_STATES];
} cpuidle_dev_t;

void    cpuidle_init(void);
void    cpuidle_enter(void);
int     cpuidle_nstates(void);
int     cpuidle_get_state(unsigned cpu, int idx, cpuidle_state_t *out);
void    cpuidle_set_latency_budget(uint32_t us);

#endif
