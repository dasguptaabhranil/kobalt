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
#include <spinlock.h>
#include <sched.h>
#include <mreclaim.h>

#define KSWAPD_PERIOD_MS        200
#define DEFAULT_TOTAL_PAGES     (256 * 1024)
#define DEFAULT_HIGH_FRAC       15
#define DEFAULT_LOW_FRAC        10
#define DEFAULT_MIN_FRAC        5

static spinlock_t           g_lock      = SPINLOCK_INIT;
static volatile size_t      g_free_pg   = 0;
static size_t               g_total_pg  = DEFAULT_TOTAL_PAGES;
static size_t               g_wm_high   = 0;
static size_t               g_wm_low    = 0;
static size_t               g_wm_min    = 0;
static int                  g_nshrinkers = 0;
static mreclaim_shrinker_t  g_shrinkers[MRECLAIM_MAX_SHRINKERS];

static void recalc_watermarks(void)
{
    g_wm_high = g_total_pg * DEFAULT_HIGH_FRAC / 100;
    g_wm_low  = g_total_pg * DEFAULT_LOW_FRAC  / 100;
    g_wm_min  = g_total_pg * DEFAULT_MIN_FRAC  / 100;
}

static mem_pressure_t pressure_nolock(void)
{
    size_t f = g_free_pg;
    if (f >= g_wm_high)  return MEM_PRESSURE_NONE;
    if (f >= g_wm_low)   return MEM_PRESSURE_LOW;
    if (f >= g_wm_min)   return MEM_PRESSURE_MEDIUM;
    return MEM_PRESSURE_CRITICAL;
}

size_t mreclaim_run(size_t target_pages)
{
    mem_pressure_t lvl = mreclaim_pressure();
    size_t freed = 0;

    for (int i = 0; i < g_nshrinkers && freed < target_pages; i++) {
        if (!g_shrinkers[i]) continue;
        freed += g_shrinkers[i](target_pages - freed, lvl);
    }
    return freed;
}

static void kswapd(void *arg)
{
    extern uint32_t sys_now(void);
    (void)arg;

    for (;;) {
        uint32_t t0 = sys_now();
        while (sys_now() - t0 < KSWAPD_PERIOD_MS)
            sched_yield();

        mem_pressure_t p = mreclaim_pressure();
        if (p == MEM_PRESSURE_NONE)
            continue;

        size_t target;
        switch (p) {
        case MEM_PRESSURE_LOW:
            target = g_wm_high - g_free_pg;
            break;
        case MEM_PRESSURE_MEDIUM:
            target = g_wm_high - g_free_pg + g_wm_low;
            break;
        case MEM_PRESSURE_CRITICAL:
            target = g_wm_high - g_free_pg + g_total_pg / 10;
            break;
        default:
            target = 0;
            break;
        }

        if (!target) continue;

        size_t freed = mreclaim_run(target);

        if (p == MEM_PRESSURE_CRITICAL && freed < target / 2)
            klog_warn("kswapd", "low reclaim yield under critical pressure");
    }
}

void mreclaim_init(void)
{
    recalc_watermarks();
    g_free_pg = g_total_pg;

    sched_thread_create("kswapd", kswapd, NULL, 0);

    char msg[80];
    ksnprintf(msg, sizeof(msg),
              "total=%u pages  wm_high=%u  wm_low=%u  wm_min=%u",
              (unsigned)g_total_pg, (unsigned)g_wm_high,
              (unsigned)g_wm_low,   (unsigned)g_wm_min);
    klog_ok("mreclaim", msg);
}

int mreclaim_register_shrinker(mreclaim_shrinker_t fn)
{
    uint64_t fl = spin_lock_irqsave(&g_lock);
    if (g_nshrinkers >= MRECLAIM_MAX_SHRINKERS) {
        spin_unlock_irqrestore(&g_lock, fl);
        return -1;
    }
    g_shrinkers[g_nshrinkers++] = fn;
    spin_unlock_irqrestore(&g_lock, fl);
    return 0;
}

void mreclaim_unregister_shrinker(mreclaim_shrinker_t fn)
{
    uint64_t fl = spin_lock_irqsave(&g_lock);
    for (int i = 0; i < g_nshrinkers; i++) {
        if (g_shrinkers[i] == fn) {
            g_shrinkers[i] = g_shrinkers[--g_nshrinkers];
            g_shrinkers[g_nshrinkers] = NULL;
            break;
        }
    }
    spin_unlock_irqrestore(&g_lock, fl);
}

mem_pressure_t mreclaim_pressure(void)
{
    return pressure_nolock();
}

void mreclaim_set_watermarks(size_t high_pg, size_t low_pg, size_t min_pg)
{
    uint64_t fl = spin_lock_irqsave(&g_lock);
    g_wm_high = high_pg;
    g_wm_low  = low_pg;
    g_wm_min  = min_pg;
    spin_unlock_irqrestore(&g_lock, fl);
}

void mreclaim_account_alloc(size_t pages)
{
    uint64_t fl = spin_lock_irqsave(&g_lock);
    if (g_free_pg >= pages)
        g_free_pg -= pages;
    else
        g_free_pg = 0;
    spin_unlock_irqrestore(&g_lock, fl);
}

void mreclaim_account_free(size_t pages)
{
    uint64_t fl = spin_lock_irqsave(&g_lock);
    g_free_pg += pages;
    if (g_free_pg > g_total_pg)
        g_free_pg = g_total_pg;
    spin_unlock_irqrestore(&g_lock, fl);
}

size_t mreclaim_free_pages(void)
{
    return g_free_pg;
}

size_t mreclaim_total_pages(void)
{
    return g_total_pg;
}

void mreclaim_set_total(size_t total_pages)
{
    uint64_t fl = spin_lock_irqsave(&g_lock);
    g_total_pg = total_pages;
    g_free_pg  = total_pages;
    recalc_watermarks();
    spin_unlock_irqrestore(&g_lock, fl);
}
