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

#ifndef CPUFREQ_H
#define CPUFREQ_H

#include <stdint.h>

typedef enum {
    CPUFREQ_GOV_PERFORMANCE = 0,
    CPUFREQ_GOV_POWERSAVE,
    CPUFREQ_GOV_ONDEMAND,
} cpufreq_gov_t;

typedef enum {
    CPUFREQ_VENDOR_UNKNOWN = 0,
    CPUFREQ_VENDOR_INTEL,
    CPUFREQ_VENDOR_AMD,
} cpufreq_vendor_t;

typedef struct {
    uint8_t         min_pstate;
    uint8_t         max_pstate;
    uint8_t         cur_pstate;
    uint8_t         boost_pstate;
    uint32_t        base_khz;
    uint32_t        cur_khz;
    int             turbo;
    int             hwp;
    int             amd_pstate;
    cpufreq_vendor_t vendor;
} cpufreq_policy_t;

void            cpufreq_init(void);
int             cpufreq_set_gov(cpufreq_gov_t gov);
cpufreq_gov_t   cpufreq_get_gov(void);
int             cpufreq_set_limits(unsigned cpu, uint8_t minp, uint8_t maxp);
void            cpufreq_turbo_set(int en);
int             cpufreq_turbo_get(void);
uint32_t        cpufreq_get_khz(unsigned cpu);
int             cpufreq_get_policy(unsigned cpu, cpufreq_policy_t *pol);
void            cpufreq_sample(void);
const char     *cpufreq_gov_name(cpufreq_gov_t gov);

#endif
