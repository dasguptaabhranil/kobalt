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
#include <smp.h>
#include <percpu.h>
#include <spinlock.h>
#include <sched.h>
#include <cpufreq.h>
#include "../arch/x86_64/msr.h"

#ifndef MSR_IA32_MPERF
#define MSR_IA32_MPERF          0x000000E7UL
#endif
#ifndef MSR_IA32_APERF
#define MSR_IA32_APERF          0x000000E8UL
#endif
#ifndef MSR_PLATFORM_INFO
#define MSR_PLATFORM_INFO       0x000000CEUL
#endif
#ifndef MSR_IA32_PERF_STATUS
#define MSR_IA32_PERF_STATUS    0x00000198UL
#endif
#ifndef MSR_IA32_PERF_CTL
#define MSR_IA32_PERF_CTL       0x00000199UL
#endif
#ifndef MSR_IA32_MISC_ENABLE
#define MSR_IA32_MISC_ENABLE    0x000001A0UL
#endif
#ifndef MSR_TURBO_RATIO_LIMIT
#define MSR_TURBO_RATIO_LIMIT   0x000001ADUL
#endif
#ifndef MSR_IA32_PM_ENABLE
#define MSR_IA32_PM_ENABLE      0x00000770UL
#endif
#ifndef MSR_IA32_HWP_CAP
#define MSR_IA32_HWP_CAP        0x00000771UL
#endif
#ifndef MSR_IA32_HWP_REQUEST
#define MSR_IA32_HWP_REQUEST    0x00000774UL
#endif
#ifndef MSR_AMD_PERF_CTL
#define MSR_AMD_PERF_CTL        0xC0010062UL
#endif
#ifndef MSR_AMD_PERF_STATUS
#define MSR_AMD_PERF_STATUS     0xC0010063UL
#endif
#ifndef MSR_AMD_PSTATE_DEF0
#define MSR_AMD_PSTATE_DEF0     0xC0010064UL
#endif

#define IDA_ENGAGE_BIT          (1ULL << 38)
#define HWP_ENABLE_BIT          (1ULL << 0)

#define ONDEMAND_UP_THRESH      85
#define ONDEMAND_DOWN_THRESH    40
#define ONDEMAND_SAMPLE_MS      50

#define MAX_CPUS                64

static inline uint64_t rdmsr64(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr64(uint32_t msr, uint64_t v)
{
    __asm__ volatile("wrmsr" :: "c"(msr), "a"((uint32_t)v), "d"((uint32_t)(v >> 32)));
}

static inline void cpuid_ex(uint32_t leaf, uint32_t sub,
                             uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    __asm__ volatile("cpuid"
        : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
        : "a"(leaf), "c"(sub));
}

typedef struct {
    uint8_t     min_p;
    uint8_t     max_p;
    uint8_t     cur_p;
    uint8_t     boost_p;
    uint32_t    base_khz;
    uint64_t    aperf_last;
    uint64_t    mperf_last;
    int         hwp;
    int         amd;
} cpu_pstate_t;

static cpu_pstate_t     g_pstate[MAX_CPUS];
static cpufreq_gov_t    g_gov     = CPUFREQ_GOV_PERFORMANCE;
static cpufreq_vendor_t g_vendor  = CPUFREQ_VENDOR_UNKNOWN;
static spinlock_t       g_lock    = SPINLOCK_INIT;
static int              g_turbo_en  = 1;
static int              g_ncpus     = 0;
static int              g_hwp_global = 0;

static cpufreq_vendor_t detect_vendor(void)
{
    uint32_t a, b, c, d;
    cpuid_ex(0, 0, &a, &b, &c, &d);
    if (b == 0x756e6547 && d == 0x49656e69 && c == 0x6c65746e)
        return CPUFREQ_VENDOR_INTEL;
    if (b == 0x68747541 && d == 0x69746e65 && c == 0x444d4163)
        return CPUFREQ_VENDOR_AMD;
    return CPUFREQ_VENDOR_UNKNOWN;
}

static int intel_hwp_supported(void)
{
    uint32_t a, b, c, d;
    cpuid_ex(6, 0, &a, &b, &c, &d);
    return (a >> 7) & 1;
}

static int intel_turbo_supported(void)
{
    uint32_t a, b, c, d;
    cpuid_ex(6, 0, &a, &b, &c, &d);
    return (a >> 1) & 1;
}

static uint32_t intel_base_khz(void)
{
    uint32_t a, b, c, d;
    cpuid_ex(0x16, 0, &a, &b, &c, &d);
    if (a)
        return (uint32_t)a * 1000u;
    uint64_t plat = rdmsr64(MSR_PLATFORM_INFO);
    uint8_t ratio = (plat >> 8) & 0xFF;
    return ratio ? (uint32_t)ratio * 100000u : 2000000u;
}

static uint8_t intel_max_pstate(void)
{
    uint64_t plat = rdmsr64(MSR_PLATFORM_INFO);
    uint8_t  r    = (plat >> 8) & 0xFF;
    return r ? r : 20;
}

static uint8_t intel_boost_pstate(void)
{
    if (!g_turbo_en)
        return intel_max_pstate();
    uint64_t turbo = rdmsr64(MSR_TURBO_RATIO_LIMIT);
    uint8_t r = turbo & 0xFF;
    return r ? r : intel_max_pstate();
}

static void intel_set_pstate(unsigned cpu, uint8_t p)
{
    (void)cpu;
    wrmsr64(MSR_IA32_PERF_CTL, (uint64_t)p << 8);
    g_pstate[cpu].cur_p = p;
}

static void intel_hwp_init(unsigned cpu)
{
    wrmsr64(MSR_IA32_PM_ENABLE, 1);
    uint64_t cap = rdmsr64(MSR_IA32_HWP_CAP);
    uint8_t hwp_min  = cap & 0xFF;
    uint8_t hwp_max  = (cap >> 8) & 0xFF;
    uint8_t hwp_eff  = (cap >> 16) & 0xFF;
    (void)hwp_eff;

    uint64_t req = ((uint64_t)hwp_min)       |
                   ((uint64_t)hwp_max << 8)  |
                   ((uint64_t)hwp_max << 16) |
                   (0ULL << 24);
    wrmsr64(MSR_IA32_HWP_REQUEST, req);
    g_pstate[cpu].hwp = 1;
    g_pstate[cpu].min_p = hwp_min;
    g_pstate[cpu].max_p = hwp_max;
    g_pstate[cpu].cur_p = hwp_max;
}

static void intel_hwp_set_request(unsigned cpu, uint8_t minp, uint8_t maxp, uint8_t desired)
{
    uint64_t req = ((uint64_t)minp)          |
                   ((uint64_t)maxp << 8)     |
                   ((uint64_t)desired << 16) |
                   (0ULL << 24);
    wrmsr64(MSR_IA32_HWP_REQUEST, req);
    g_pstate[cpu].cur_p = desired;
}

static int amd_pstate_supported(void)
{
    uint32_t a, b, c, d;
    cpuid_ex(0x80000007, 0, &a, &b, &c, &d);
    return (d >> 7) & 1;
}

static uint8_t amd_num_pstates(void)
{
    uint32_t a, b, c, d;
    cpuid_ex(0x80000008, 0, &a, &b, &c, &d);
    uint8_t n = (c >> 4) & 0xF;
    return n ? n + 1 : 1;
}

static uint32_t amd_pstate_khz(uint8_t idx)
{
    uint64_t def = rdmsr64(MSR_AMD_PSTATE_DEF0 + idx);
    uint8_t  fid = def & 0xFF;
    uint8_t  did = (def >> 8) & 0x3F;
    if (!did) did = 1;
    return ((uint32_t)fid * 25000u) / did;
}

static void amd_set_pstate(unsigned cpu, uint8_t p)
{
    (void)cpu;
    uint64_t cur = rdmsr64(MSR_AMD_PERF_CTL);
    cur = (cur & ~0x7ULL) | (p & 0x7);
    wrmsr64(MSR_AMD_PERF_CTL, cur);
    g_pstate[cpu].cur_p = p;
}

static void init_cpu(unsigned cpu)
{
    cpu_pstate_t *cp = &g_pstate[cpu];

    if (g_vendor == CPUFREQ_VENDOR_INTEL) {
        cp->base_khz = intel_base_khz();
        cp->max_p    = intel_max_pstate();
        cp->boost_p  = intel_boost_pstate();
        cp->min_p    = 4;

        if (g_hwp_global) {
            intel_hwp_init(cpu);
        } else {
            cp->cur_p = cp->max_p;
            intel_set_pstate(cpu, cp->max_p);
        }
    } else if (g_vendor == CPUFREQ_VENDOR_AMD) {
        cp->amd      = 1;
        cp->max_p    = 0;
        cp->min_p    = amd_num_pstates() - 1;
        cp->boost_p  = 0;
        cp->base_khz = amd_pstate_khz(0);
        cp->cur_p    = 0;
        amd_set_pstate(cpu, 0);
    }

    cp->aperf_last = rdmsr64(MSR_IA32_APERF);
    cp->mperf_last = rdmsr64(MSR_IA32_MPERF);
}

static uint32_t cpu_cur_khz(unsigned cpu)
{
    cpu_pstate_t *cp = &g_pstate[cpu];
    if (!cp->base_khz)
        return 0;

    uint64_t ap = rdmsr64(MSR_IA32_APERF);
    uint64_t mp = rdmsr64(MSR_IA32_MPERF);
    uint64_t dap = ap - cp->aperf_last;
    uint64_t dmp = mp - cp->mperf_last;

    if (!dmp)
        return cp->base_khz;

    return (uint32_t)((uint64_t)cp->base_khz * dap / dmp);
}

static void apply_gov_cpu(unsigned cpu)
{
    cpu_pstate_t *cp = &g_pstate[cpu];

    switch (g_gov) {
    case CPUFREQ_GOV_PERFORMANCE:
        if (cp->hwp)
            intel_hwp_set_request(cpu, cp->min_p, cp->boost_p, cp->boost_p);
        else if (cp->amd)
            amd_set_pstate(cpu, cp->max_p);
        else
            intel_set_pstate(cpu, cp->boost_p);
        break;

    case CPUFREQ_GOV_POWERSAVE:
        if (cp->hwp)
            intel_hwp_set_request(cpu, cp->min_p, cp->min_p, cp->min_p);
        else if (cp->amd)
            amd_set_pstate(cpu, cp->min_p);
        else
            intel_set_pstate(cpu, cp->min_p);
        break;

    case CPUFREQ_GOV_ONDEMAND:
        break;
    }
}

static void ondemand_sample_cpu(unsigned cpu)
{
    cpu_pstate_t *cp = &g_pstate[cpu];

    uint64_t ap = rdmsr64(MSR_IA32_APERF);
    uint64_t mp = rdmsr64(MSR_IA32_MPERF);
    uint64_t dap = ap - cp->aperf_last;
    uint64_t dmp = mp - cp->mperf_last;

    cp->aperf_last = ap;
    cp->mperf_last = mp;

    if (!dmp) return;

    uint32_t util = (uint32_t)((dap * 100ULL) / dmp);
    uint8_t  cur  = cp->cur_p;
    uint8_t  tgt  = cur;

    if (util > ONDEMAND_UP_THRESH) {
        tgt = cp->hwp ? cp->boost_p : (uint8_t)(cur + 1 < cp->max_p ? cur + 1 : cp->max_p);
    } else if (util < ONDEMAND_DOWN_THRESH) {
        tgt = (cur > cp->min_p + 1) ? cur - 1 : cp->min_p;
    }

    if (tgt == cur) return;

    if (cp->hwp)
        intel_hwp_set_request(cpu, cp->min_p, cp->max_p, tgt);
    else if (cp->amd)
        amd_set_pstate(cpu, tgt);
    else
        intel_set_pstate(cpu, tgt);
}

static void cpufreq_daemon(void *arg)
{
    extern uint32_t sys_now(void);
    (void)arg;

    for (;;) {
        uint32_t t0 = sys_now();
        while (sys_now() - t0 < ONDEMAND_SAMPLE_MS)
            sched_yield();

        if (g_gov != CPUFREQ_GOV_ONDEMAND)
            continue;

        for (int i = 0; i < g_ncpus; i++)
            ondemand_sample_cpu((unsigned)i);
    }
}

void cpufreq_init(void)
{
    g_vendor = detect_vendor();
    g_ncpus  = (int)smp_cpu_count();

    if (g_vendor == CPUFREQ_VENDOR_INTEL) {
        g_hwp_global = intel_hwp_supported();
        if (!intel_turbo_supported())
            g_turbo_en = 0;
    } else if (g_vendor == CPUFREQ_VENDOR_AMD) {
        if (!amd_pstate_supported())
            g_vendor = CPUFREQ_VENDOR_UNKNOWN;
    }

    if (g_vendor == CPUFREQ_VENDOR_UNKNOWN) {
        klog_info("cpufreq", "no supported P-state interface");
        return;
    }

    for (int i = 0; i < g_ncpus; i++)
        init_cpu((unsigned)i);

    sched_thread_create("k_cpufreq", cpufreq_daemon, NULL, 0);

    char msg[64];
    ksnprintf(msg, sizeof(msg), "%s  %u CPUs  HWP=%s  turbo=%s  gov=performance",
              g_vendor == CPUFREQ_VENDOR_INTEL ? "Intel" : "AMD",
              (unsigned)g_ncpus,
              g_hwp_global ? "yes" : "no",
              g_turbo_en   ? "on"  : "off");
    klog_ok("cpufreq", msg);
}

int cpufreq_set_gov(cpufreq_gov_t gov)
{
    if (gov > CPUFREQ_GOV_ONDEMAND)
        return -1;

    uint64_t fl = spin_lock_irqsave(&g_lock);
    g_gov = gov;
    if (gov != CPUFREQ_GOV_ONDEMAND) {
        for (int i = 0; i < g_ncpus; i++)
            apply_gov_cpu((unsigned)i);
    }
    spin_unlock_irqrestore(&g_lock, fl);
    return 0;
}

cpufreq_gov_t cpufreq_get_gov(void)
{
    return g_gov;
}

int cpufreq_set_limits(unsigned cpu, uint8_t minp, uint8_t maxp)
{
    if (cpu >= (unsigned)g_ncpus || minp > maxp)
        return -1;

    uint64_t fl = spin_lock_irqsave(&g_lock);
    g_pstate[cpu].min_p = minp;
    g_pstate[cpu].max_p = maxp;
    apply_gov_cpu(cpu);
    spin_unlock_irqrestore(&g_lock, fl);
    return 0;
}

void cpufreq_turbo_set(int en)
{
    uint64_t fl = spin_lock_irqsave(&g_lock);
    g_turbo_en = en;
    if (g_vendor == CPUFREQ_VENDOR_INTEL) {
        uint64_t misc = rdmsr64(MSR_IA32_MISC_ENABLE);
        if (en)
            misc &= ~IDA_ENGAGE_BIT;
        else
            misc |=  IDA_ENGAGE_BIT;
        wrmsr64(MSR_IA32_MISC_ENABLE, misc);

        for (int i = 0; i < g_ncpus; i++)
            g_pstate[i].boost_p = intel_boost_pstate();
    }
    spin_unlock_irqrestore(&g_lock, fl);
}

int cpufreq_turbo_get(void)
{
    return g_turbo_en;
}

uint32_t cpufreq_get_khz(unsigned cpu)
{
    if (cpu >= (unsigned)g_ncpus)
        return 0;
    return cpu_cur_khz(cpu);
}

int cpufreq_get_policy(unsigned cpu, cpufreq_policy_t *pol)
{
    if (cpu >= (unsigned)g_ncpus || !pol)
        return -1;

    cpu_pstate_t *cp = &g_pstate[cpu];
    pol->min_pstate   = cp->min_p;
    pol->max_pstate   = cp->max_p;
    pol->cur_pstate   = cp->cur_p;
    pol->boost_pstate = cp->boost_p;
    pol->base_khz     = cp->base_khz;
    pol->cur_khz      = cpu_cur_khz(cpu);
    pol->turbo        = g_turbo_en;
    pol->hwp          = cp->hwp;
    pol->amd_pstate   = cp->amd;
    pol->vendor       = g_vendor;
    return 0;
}

void cpufreq_sample(void)
{
    if (g_gov != CPUFREQ_GOV_ONDEMAND)
        return;
    for (int i = 0; i < g_ncpus; i++)
        ondemand_sample_cpu((unsigned)i);
}

const char *cpufreq_gov_name(cpufreq_gov_t gov)
{
    switch (gov) {
    case CPUFREQ_GOV_PERFORMANCE: return "performance";
    case CPUFREQ_GOV_POWERSAVE:   return "powersave";
    case CPUFREQ_GOV_ONDEMAND:    return "ondemand";
    default:                       return "unknown";
    }
}
