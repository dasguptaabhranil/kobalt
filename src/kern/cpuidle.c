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

#include <stdint.h>
#include <stddef.h>
#include <kernel.h>
#include <percpu.h>
#include <smp.h>
#include <cpuidle.h>

#define CPUID_LEAF_MWAIT    0x05

#define MWAIT_C1            0x00
#define MWAIT_C1E           0x01
#define MWAIT_C3            0x10
#define MWAIT_C6            0x20
#define MWAIT_C7            0x30
#define MWAIT_C8            0x40
#define MWAIT_C10           0x60

#define MAX_CPUS            64

static inline void cpuid_ex(uint32_t leaf, uint32_t sub,
                             uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    __asm__ volatile("cpuid"
        : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
        : "a"(leaf), "c"(sub));
}

static inline uint64_t rdtsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline void do_monitor(const volatile void *addr)
{
    __asm__ volatile("monitor" :: "a"(addr), "c"(0UL), "d"(0UL) : "memory");
}

static inline void do_mwait(uint32_t hint, uint32_t ext)
{
    __asm__ volatile("mwait" :: "a"(hint), "c"(ext) : "memory");
}

static cpuidle_dev_t    g_cpuidle[MAX_CPUS];
static int              g_mwait_ok      = 0;
static int              g_ibe           = 0;
static int              g_ncpus         = 0;
static uint32_t         g_lat_budget_us = 50;

static volatile int     g_idle_poll[MAX_CPUS];

static void probe_mwait(void)
{
    uint32_t a, b, c, d;
    cpuid_ex(0, 0, &a, &b, &c, &d);
    if (a < CPUID_LEAF_MWAIT) return;

    cpuid_ex(CPUID_LEAF_MWAIT, 0, &a, &b, &c, &d);
    if (!(c & 1)) return;

    g_mwait_ok = 1;
    g_ibe      = (c >> 1) & 1;
    (void)a; (void)b; (void)d;
}

static void build_cstate_table(unsigned cpu)
{
    cpuidle_dev_t *dev = &g_cpuidle[cpu];
    dev->mwait_supported = g_mwait_ok;

    int n = 0;

    dev->states[n].name                = "C1-halt";
    dev->states[n].hint                = 0xFFFFFFFFUL;
    dev->states[n].exit_latency_us     = 1;
    dev->states[n].target_residency_us = 2;
    n++;

    if (!g_mwait_ok) {
        dev->nstates  = n;
        dev->cur_state = 0;
        return;
    }

    dev->states[n].name                = "C1";
    dev->states[n].hint                = MWAIT_C1;
    dev->states[n].exit_latency_us     = 2;
    dev->states[n].target_residency_us = 4;
    n++;

    dev->states[n].name                = "C1E";
    dev->states[n].hint                = MWAIT_C1E;
    dev->states[n].exit_latency_us     = 10;
    dev->states[n].target_residency_us = 20;
    n++;

    dev->states[n].name                = "C3";
    dev->states[n].hint                = MWAIT_C3;
    dev->states[n].exit_latency_us     = 59;
    dev->states[n].target_residency_us = 156;
    n++;

    dev->states[n].name                = "C6";
    dev->states[n].hint                = MWAIT_C6;
    dev->states[n].exit_latency_us     = 300;
    dev->states[n].target_residency_us = 600;
    n++;

    dev->states[n].name                = "C7";
    dev->states[n].hint                = MWAIT_C7;
    dev->states[n].exit_latency_us     = 400;
    dev->states[n].target_residency_us = 1000;
    n++;

    dev->states[n].name                = "C8";
    dev->states[n].hint                = MWAIT_C8;
    dev->states[n].exit_latency_us     = 900;
    dev->states[n].target_residency_us = 3000;
    n++;

    dev->states[n].name                = "C10";
    dev->states[n].hint                = MWAIT_C10;
    dev->states[n].exit_latency_us     = 890;
    dev->states[n].target_residency_us = 5000;
    n++;

    if (n > CPUIDLE_MAX_STATES) n = CPUIDLE_MAX_STATES;
    dev->nstates   = n;
    dev->cur_state = 0;
}

static int select_state(unsigned cpu)
{
    cpuidle_dev_t *dev = &g_cpuidle[cpu];
    int best = 0;

    for (int i = dev->nstates - 1; i >= 0; i--) {
        if (dev->states[i].exit_latency_us <= g_lat_budget_us &&
            dev->states[i].target_residency_us <= g_lat_budget_us * 4) {
            best = i;
            break;
        }
    }
    return best;
}

static void enter_cstate(unsigned cpu, int sidx)
{
    cpuidle_dev_t *dev  = &g_cpuidle[cpu];
    cpuidle_state_t *cs = &dev->states[sidx];

    dev->cur_state = sidx;
    cs->usage_count++;

    uint64_t t0 = rdtsc();

    if (cs->hint == 0xFFFFFFFFUL) {
        __asm__ volatile("hlt" ::: "memory");
    } else {
        g_idle_poll[cpu] = 0;
        do_monitor((const volatile void *)&g_idle_poll[cpu]);
        do_mwait(cs->hint, g_ibe ? 1 : 0);
    }

    uint64_t dt = rdtsc() - t0;
    cs->total_time_us += dt / 1000;
}

void cpuidle_enter(void)
{
    unsigned cpu = PERCPU_ID();
    if (cpu >= (unsigned)g_ncpus) {
        __asm__ volatile("hlt" ::: "memory");
        return;
    }

    int sidx = select_state(cpu);
    enter_cstate(cpu, sidx);
}

void cpuidle_set_latency_budget(uint32_t us)
{
    g_lat_budget_us = us;
}

int cpuidle_nstates(void)
{
    return g_ncpus > 0 ? g_cpuidle[0].nstates : 1;
}

int cpuidle_get_state(unsigned cpu, int idx, cpuidle_state_t *out)
{
    if (cpu >= (unsigned)g_ncpus || !out)
        return -1;

    cpuidle_dev_t *dev = &g_cpuidle[cpu];
    if (idx < 0 || idx >= dev->nstates)
        return -1;

    *out = dev->states[idx];
    return 0;
}

void cpuidle_init(void)
{
    g_ncpus = (int)smp_cpu_count();

    probe_mwait();

    for (int i = 0; i < g_ncpus; i++)
        build_cstate_table((unsigned)i);

    char msg[72];
    ksnprintf(msg, sizeof(msg),
              "%d CPUs  MWAIT=%s  IBE=%s  states=%d  lat_budget=%u us",
              g_ncpus,
              g_mwait_ok ? "yes" : "no",
              g_ibe      ? "yes" : "no",
              g_cpuidle[0].nstates,
              g_lat_budget_us);
    klog_ok("cpuidle", msg);
}
