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

#include "../inc/tykid_internal.h"
#include "../inc/tykid_seal_ext.h"

#define TY_WDT_DEFAULT_INTERVAL_MS 30000U
#define TY_WDT_MIN_INTERVAL_MS 5000U
#define TY_WDT_MAX_INTERVAL_MS 300000U
#define TY_WDT_JITTER_PERCENT 20U
#define TY_WDT_DEADMAN_GRACE_MS 90000U

#define TY_WDT_ESCALATE_TO_SAFE_MODE    3U
#define TY_WDT_ESCALATE_TO_SHUTDOWN     6U

typedef uptr kobalt_thread_t;
typedef uptr kobalt_timer_t;

extern kobalt_thread_t kobalt_kthread_create(void (*fn)(void *),
                                               void *arg, const char *name,
                                               u32 stack_sz, u8 rt_prio);
extern void            kobalt_kthread_exit(void);
extern tykid_status_t  kobalt_kthread_sleep_ms(u32 ms);
extern void            kobalt_kthread_pin_cpu(kobalt_thread_t t, u8 cpu);
extern bool8           kobalt_kthread_should_stop(void);
extern void            kobalt_kthread_stop(kobalt_thread_t t);

extern kobalt_timer_t  kobalt_timer_create_oneshot(u32 timeout_ms,
                                                     void (*cb)(void *), void *arg);
extern void            kobalt_timer_reset(kobalt_timer_t t, u32 ms);
extern void            kobalt_timer_destroy(kobalt_timer_t t);
extern void            kobalt_emergency_shutdown(const char *reason);

typedef struct {

    u32             base_interval_ms;
    u32             deadman_grace_ms;

    kobalt_thread_t thread;
    kobalt_timer_t  deadman;
    bool8           running;
    bool8           armed;
 bool8 in_safe_mode;

    u64  sweep_count;
    u64  seal_fail_count;
    u64  hmac_fail_count;
    u64  policy_fail_count;
    u64  hotplug_event_count;
    u64  consecutive_failures;
 u64 safe_mode_entry_sweep;

    tykid_gate_ctx_t  *ctx;

    ty_ext_seal_ctx_t *ext_seal;

    tykid_hw_enumset_t last_hw;
    bool8              hw_captured;
} ty_watchdog_t;

static TYKID_SECTION(".tykid.watchdog") ty_watchdog_t g_watchdog;

static u32
ty_wdt_jitter_interval(tykid_gate_ctx_t *ctx, u32 base_ms)
{
    u64 rnd = ty_entropy_u64(ctx);
    u32 jitter_range = (base_ms * TY_WDT_JITTER_PERCENT) / 100U;
    s32 delta  = (s32)(rnd % (2 * jitter_range + 1)) - (s32)jitter_range;
    s32 result = (s32)base_ms + delta;
    if (result < (s32)TY_WDT_MIN_INTERVAL_MS) result = (s32)TY_WDT_MIN_INTERVAL_MS;
    if (result > (s32)TY_WDT_MAX_INTERVAL_MS) result = (s32)TY_WDT_MAX_INTERVAL_MS;
    return (u32)result;
}

static void
ty_wdt_enter_safe_mode(ty_watchdog_t *wdt)
{
    tykid_gate_ctx_t *ctx = wdt->ctx;

 if (wdt->in_safe_mode) return;

    TY_LOG(ctx, TY_LOG_FATAL,
           "WDT: entering SAFE MODE after %llu consecutive sweep failures "
           "(sweep #%llu) — unloading non-critical drivers",
           (unsigned long long)wdt->consecutive_failures,
           (unsigned long long)wdt->sweep_count);

    u32 unloaded = 0;
    for (u32 i = 0; i < ctx->reg.count; i++) {
        tykid_driver_desc_t *drv = &ctx->reg.entries[i];
        if (drv->state != TYKID_DRV_STATE_ACTIVE) continue;
        if (drv->flags & TYKID_DRV_FLAG_CRITICAL) continue;

        TY_LOG(ctx, TY_LOG_WARN,
               "WDT safe mode: unloading non-critical driver '%s'", drv->name);

        if (ctx->cfg.ko_unload_fn && drv->base_vaddr) {
            ctx->cfg.ko_unload_fn(drv->base_vaddr);
        }
        drv->state      = TYKID_DRV_STATE_UNLOADED;
        drv->base_vaddr = 0;
        unloaded++;
    }

    ctx->safe_mode            = TYKID_TRUE;
    wdt->in_safe_mode         = TYKID_TRUE;
    wdt->safe_mode_entry_sweep = wdt->sweep_count;

    TY_LOG(ctx, TY_LOG_FATAL,
           "WDT: safe mode active — %u non-critical driver(s) unloaded. "
           "System continues with essential drivers only.",
           unloaded);
}

static void
ty_wdt_hotplug_check(ty_watchdog_t *wdt)
{
    tykid_gate_ctx_t *ctx = wdt->ctx;

    tykid_hw_enumset_t current_hw;
    tykid_status_t st = tykid_enumerate_hardware(ctx, &current_hw);
    if (st != TYKID_OK) {
        TY_LOG(ctx, TY_LOG_WARN, "WDT: hardware re-enumeration failed: %d", st);
        return;
    }

    if (!wdt->hw_captured) {
        wdt->last_hw     = current_hw;
        wdt->hw_captured = TYKID_TRUE;
        return;
    }

    if (current_hw.bus_topology_hash == wdt->last_hw.bus_topology_hash)
        return;

    wdt->hotplug_event_count++;
    TY_LOG(ctx, TY_LOG_WARN,
           "WDT: bus topology CHANGED (old=%016llx new=%016llx) — hot-plug #%llu",
           (unsigned long long)wdt->last_hw.bus_topology_hash,
           (unsigned long long)current_hw.bus_topology_hash,
           (unsigned long long)wdt->hotplug_event_count);

    for (u32 i = 0; i < wdt->last_hw.count; i++) {
        const tykid_hw_device_t *old = &wdt->last_hw.devices[i];
        bool8 found = TYKID_FALSE;
        for (u32 j = 0; j < current_hw.count; j++) {
            if (current_hw.devices[j].vendor_id == old->vendor_id &&
                current_hw.devices[j].device_id == old->device_id &&
                current_hw.devices[j].bus  == old->bus  &&
                current_hw.devices[j].slot == old->slot &&
                current_hw.devices[j].func == old->func) {
                found = TYKID_TRUE; break;
            }
        }
        if (!found) {
            TY_LOG(ctx, TY_LOG_WARN,
                   "WDT: device '%s' (%02x:%02x.%x) REMOVED",
                   old->name, old->bus, old->slot, old->func);
            for (u32 ri = 0; ri < ctx->reg.count; ri++) {
                tykid_driver_desc_t *drv = &ctx->reg.entries[ri];
                if (drv->state != TYKID_DRV_STATE_ACTIVE) continue;
                for (u8 ci = 0; ci < drv->hw_class_count; ci++) {
                    if (drv->hw_classes[ci] == old->ty_class) {
                        TY_LOG(ctx, TY_LOG_WARN,
                               "WDT: force-unloading '%s' (device removed)",
                               drv->name);
                        if (ctx->cfg.ko_unload_fn && drv->base_vaddr)
                            ctx->cfg.ko_unload_fn(drv->base_vaddr);
                        drv->state      = TYKID_DRV_STATE_UNLOADED;
                        drv->base_vaddr = 0;
                        break;
                    }
                }
            }
        }
    }

    for (u32 j = 0; j < current_hw.count; j++) {
        const tykid_hw_device_t *new_dev = &current_hw.devices[j];
        bool8 found = TYKID_FALSE;
        for (u32 i = 0; i < wdt->last_hw.count; i++) {
            if (wdt->last_hw.devices[i].vendor_id == new_dev->vendor_id &&
                wdt->last_hw.devices[i].device_id == new_dev->device_id &&
                wdt->last_hw.devices[i].bus  == new_dev->bus  &&
                wdt->last_hw.devices[i].slot == new_dev->slot &&
                wdt->last_hw.devices[i].func == new_dev->func) {
                found = TYKID_TRUE; break;
            }
        }
        if (!found) {
            TY_LOG(ctx, TY_LOG_WARN,
                   "WDT: NEW device '%s' (%02x:%02x.%x vendor=%04x device=%04x) "
                   "appeared — blocking DMA",
                   new_dev->name, new_dev->bus, new_dev->slot, new_dev->func,
                   new_dev->vendor_id, new_dev->device_id);
            extern tykid_status_t kobalt_iommu_block_device(u8, u8, u8);
            kobalt_iommu_block_device(new_dev->bus, new_dev->slot, new_dev->func);
        }
    }

    wdt->last_hw = current_hw;
}

static TYKID_COLD void
ty_wdt_deadman_fired(void *arg)
{
    (void)arg;
    kobalt_emergency_shutdown("TYKID watchdog dead-man timer expired — "
                               "watchdog thread stalled or killed");
}

static bool8
ty_wdt_sweep(ty_watchdog_t *wdt)
{
    tykid_gate_ctx_t *ctx = wdt->ctx;
    bool8 all_ok = TYKID_TRUE;

    wdt->sweep_count++;
    TY_LOG(ctx, TY_LOG_DEBUG, "WDT sweep #%llu starting%s",
           (unsigned long long)wdt->sweep_count,
           wdt->in_safe_mode ? " [SAFE MODE]" : "");

    if (tykid_verify_seal(ctx) != TYKID_OK) {
        TY_LOG(ctx, TY_LOG_ERROR, "WDT: GATE SEAL BROKEN");
        wdt->seal_fail_count++;
        all_ok = TYKID_FALSE;

    }

    if (wdt->ext_seal) {
        tykid_status_t est = ty_ext_seal_recheck(wdt->ext_seal);
        if (est != TYKID_OK) {
            TY_LOG(ctx, TY_LOG_ERROR,
                   "WDT: HYPER-SEAL BROKEN (hardware vector drift)");
            wdt->seal_fail_count++;
            all_ok = TYKID_FALSE;
        }
    }

    if (tykid_recheck_all(ctx) != TYKID_OK) {
        TY_LOG(ctx, TY_LOG_ERROR, "WDT: one or more drivers TAMPERED");
        wdt->hmac_fail_count++;
        all_ok = TYKID_FALSE;
    }

    extern tykid_status_t tykid_policy_recheck(tykid_gate_ctx_t *);
    if (tykid_policy_recheck(ctx) != TYKID_OK) {
        TY_LOG(ctx, TY_LOG_WARN, "WDT: policy violations detected");
        wdt->policy_fail_count++;

    }

    ty_wdt_hotplug_check(wdt);

    kobalt_timer_reset(wdt->deadman, wdt->deadman_grace_ms);

    return all_ok;
}

static void
ty_wdt_thread_fn(void *arg)
{
    ty_watchdog_t *wdt    = (ty_watchdog_t *)arg;
    tykid_gate_ctx_t *ctx = wdt->ctx;

    TY_LOG(ctx, TY_LOG_INFO,
           "WDT thread started (interval=%ums dead-man=%ums "
           "safe_mode_at=%u shutdown_at=%u)",
           wdt->base_interval_ms, wdt->deadman_grace_ms,
           TY_WDT_ESCALATE_TO_SAFE_MODE, TY_WDT_ESCALATE_TO_SHUTDOWN);

    wdt->consecutive_failures = 0;

    while (!kobalt_kthread_should_stop()) {
        u32 sleep_ms = ty_wdt_jitter_interval(ctx, wdt->base_interval_ms);
        kobalt_kthread_sleep_ms(sleep_ms);

        if (kobalt_kthread_should_stop()) break;

        bool8 ok = ty_wdt_sweep(wdt);

        if (!ok) {
            wdt->consecutive_failures++;
            TY_LOG(ctx, TY_LOG_ERROR,
                   "WDT sweep FAILED (consecutive=%llu safe_mode_at=%u shutdown_at=%u)",
                   (unsigned long long)wdt->consecutive_failures,
                   TY_WDT_ESCALATE_TO_SAFE_MODE,
                   TY_WDT_ESCALATE_TO_SHUTDOWN);

            if (wdt->consecutive_failures >= TY_WDT_ESCALATE_TO_SAFE_MODE &&
                !wdt->in_safe_mode) {
                ty_wdt_enter_safe_mode(wdt);

            }

            if (wdt->consecutive_failures >= TY_WDT_ESCALATE_TO_SHUTDOWN) {
                TY_LOG(ctx, TY_LOG_FATAL,
                       "WDT: %llu consecutive sweep failures — "
                       "initiating controlled emergency shutdown "
                       "(ty_panic is NOT used)",
                       (unsigned long long)wdt->consecutive_failures);

                kobalt_emergency_shutdown(
                    "TYKID watchdog: system integrity unrecoverable "
                    "after repeated sweep failures");

                break;
            }
        } else {
            if (wdt->consecutive_failures > 0) {
                TY_LOG(ctx, TY_LOG_INFO,
                       "WDT sweep recovered after %llu failure(s)",
                       (unsigned long long)wdt->consecutive_failures);
            }
            wdt->consecutive_failures = 0;
        }
    }

    TY_LOG(ctx, TY_LOG_INFO, "WDT thread stopping cleanly "
           "(sweeps=%llu seal_fails=%llu hmac_fails=%llu)",
           (unsigned long long)wdt->sweep_count,
           (unsigned long long)wdt->seal_fail_count,
           (unsigned long long)wdt->hmac_fail_count);

 kobalt_timer_reset(wdt->deadman, 0);
    kobalt_kthread_exit();
}

TYKID_INTERNAL tykid_status_t
tykid_watchdog_start(tykid_gate_ctx_t *ctx,
                      ty_ext_seal_ctx_t *ext_seal,
                      u32 interval_ms)
{
    if (!ctx) return TYKID_ERR_GENERIC;
    ty_watchdog_t *wdt = &g_watchdog;
    ty_memzero_secure(wdt, sizeof(*wdt));

    wdt->ctx              = ctx;
    wdt->ext_seal         = ext_seal;
    wdt->base_interval_ms = interval_ms ? interval_ms : TY_WDT_DEFAULT_INTERVAL_MS;
    wdt->deadman_grace_ms = wdt->base_interval_ms * 3;

    if (wdt->base_interval_ms < TY_WDT_MIN_INTERVAL_MS)
        wdt->base_interval_ms = TY_WDT_MIN_INTERVAL_MS;
    if (wdt->base_interval_ms > TY_WDT_MAX_INTERVAL_MS)
        wdt->base_interval_ms = TY_WDT_MAX_INTERVAL_MS;

    wdt->deadman = kobalt_timer_create_oneshot(wdt->deadman_grace_ms,
                                                ty_wdt_deadman_fired, wdt);
    if (!wdt->deadman) {
        TY_LOG(ctx, TY_LOG_ERROR, "WDT: failed to create dead-man timer");
        return TYKID_ERR_GENERIC;
    }
    wdt->armed = TYKID_TRUE;

    wdt->thread = kobalt_kthread_create(ty_wdt_thread_fn, wdt,
 "tykid-wdt", 4096, 90 );
    if (!wdt->thread) {
        TY_LOG(ctx, TY_LOG_ERROR, "WDT: failed to create kernel thread");
        kobalt_timer_destroy(wdt->deadman);
        wdt->armed = TYKID_FALSE;
        return TYKID_ERR_GENERIC;
    }

    kobalt_kthread_pin_cpu(wdt->thread, 0);
    wdt->running = TYKID_TRUE;

    TY_LOG(ctx, TY_LOG_INFO,
           "WDT started: interval=%ums dead-man=%ums "
           "escalation: safe_mode@%u consecutive, shutdown@%u consecutive",
           wdt->base_interval_ms, wdt->deadman_grace_ms,
           TY_WDT_ESCALATE_TO_SAFE_MODE, TY_WDT_ESCALATE_TO_SHUTDOWN);
    return TYKID_OK;
}

TYKID_INTERNAL void
tykid_watchdog_stop(tykid_gate_ctx_t *ctx)
{
    ty_watchdog_t *wdt = &g_watchdog;
    if (!wdt->running) return;

    TY_LOG(ctx, TY_LOG_INFO,
           "WDT stopping (sweeps=%llu seal_fails=%llu hmac_fails=%llu "
           "safe_mode=%s)",
           (unsigned long long)wdt->sweep_count,
           (unsigned long long)wdt->seal_fail_count,
           (unsigned long long)wdt->hmac_fail_count,
           wdt->in_safe_mode ? "YES" : "no");

    kobalt_kthread_stop(wdt->thread);

    if (wdt->armed) {
        kobalt_timer_destroy(wdt->deadman);
        wdt->armed = TYKID_FALSE;
    }

    wdt->running = TYKID_FALSE;
    ty_memzero_secure(wdt, sizeof(*wdt));
}

TYKID_API u64
tykid_watchdog_sweep_count(void)
{
    return g_watchdog.sweep_count;
}

TYKID_API bool8
tykid_watchdog_is_running(void)
{
    return g_watchdog.running;
}

TYKID_API bool8
tykid_watchdog_in_safe_mode(void)
{
    return g_watchdog.in_safe_mode;
}
