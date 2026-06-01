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

#include <hrtimer.h>
#include <apic_timer.h>
#include <sched.h>
#include <stdint.h>

#define LAPIC_ICR  (0x380U)

static inline void lapic_wr(uint32_t off, uint32_t val)
{
    if (!lapic_base) return;
    lapic_base[off >> 2] = val;
}

uint64_t hrtimer_now(void)
{
    if (!tsc_khz) return 0;
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    uint64_t tsc = ((uint64_t)hi << 32) | lo;
    return tsc * 1000000ULL / tsc_khz;
}

uint64_t ktime_get_ns(void)
{
    return hrtimer_now();
}

void hrtimer_arm(uint64_t ns)
{
    if (!apic_ticks_per_ms) return;
    if (ns < HRTIMER_MIN_NS) ns = HRTIMER_MIN_NS;

    uint64_t ticks = ns * (uint64_t)apic_ticks_per_ms / 1000000ULL;
    if (!ticks)              ticks = 1;
    if (ticks > 0xffffffffULL) ticks = 0xffffffffULL;

    lapic_wr(LAPIC_ICR, (uint32_t)ticks);
}

void hrtimer_arm_vdeadline(int64_t vd_remaining, uint32_t weight)
{
    if (vd_remaining <= 0) { hrtimer_arm(HRTIMER_MIN_NS); return; }

    if (vd_remaining > (int64_t)NICE_0_LOAD * 1000)
        vd_remaining = (int64_t)NICE_0_LOAD * 1000;

    uint64_t real_ns = (uint64_t)vd_remaining * weight / NICE_0_LOAD * 1000000ULL;
    if (!real_ns) real_ns = HRTIMER_MIN_NS;
    hrtimer_arm(real_ns);
}
