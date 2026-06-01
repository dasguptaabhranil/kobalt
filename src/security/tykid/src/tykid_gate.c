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

extern void ty_selector_apply(tykid_gate_ctx_t *ctx);

extern tykid_status_t kobalt_vfs_read_binary(const char *path,
                                               u8 *buf, usz buf_sz,
                                               usz *out_len);

TYKID_INTERNAL tykid_status_t
ty_integrity_check_file(tykid_gate_ctx_t *ctx,
                         const char *path,
                         const u8 expected_hmac[TYKID_HMAC_DIGEST_BYTES])
{
    ty_blake2s_ctx_t hash_state;
    ty_blake2s_init(&hash_state, ctx->hmac_key, BLAKE2S_OUTBYTES, BLAKE2S_OUTBYTES);

    u8 chunk[4096];
    usz offset = 0;
    tykid_status_t st;
    bool8 done = TYKID_FALSE;

    while (!done) {
        usz got = 0;
        st = kobalt_vfs_read_binary(path, chunk, sizeof(chunk), &got);
        if (st == TYKID_ERR_GENERIC && got == 0) {
            done = TYKID_TRUE;
        } else if (st != TYKID_OK) {
            ty_memzero_secure(&hash_state, sizeof(hash_state));
            return TYKID_ERR_PATH;
        }
        if (got > 0) {
            ty_blake2s_update(&hash_state, chunk, got);
            offset += got;
        }
        if (got < sizeof(chunk)) done = TYKID_TRUE;
    }

    u8 computed[BLAKE2S_OUTBYTES];
    ty_blake2s_final(&hash_state, computed);

    u8 session_tag[8];
    ty_put_u64le(session_tag, ctx->boot_token ^ __TYKID_KOBALT_EXPECTED);
    u8 final_mac[BLAKE2S_OUTBYTES];
    ty_hmac_blake2s(ctx->hmac_key, BLAKE2S_OUTBYTES,
                    computed, BLAKE2S_OUTBYTES, final_mac);

    bool8 ok = ty_memeq(final_mac, expected_hmac, TYKID_HMAC_DIGEST_BYTES);

    ty_memzero_secure(computed,    sizeof(computed));
    ty_memzero_secure(final_mac,   sizeof(final_mac));
    ty_memzero_secure(chunk,       sizeof(chunk));
    ty_memzero_secure(session_tag, sizeof(session_tag));

    if (!ok) {
        TY_LOG(ctx, TY_LOG_ERROR,
               "HMAC integrity FAILED for driver at '%s' (loaded %zu bytes)",
               path, offset);
        return TYKID_ERR_DRIVER_CORRUPT;
    }

    TY_LOG(ctx, TY_LOG_DEBUG, "Integrity OK: '%s' (%zu bytes)", path, offset);
    return TYKID_OK;
}

TYKID_INTERNAL bool8
ty_driver_matches_hw(const tykid_driver_desc_t *drv,
                      const tykid_hw_device_t  *dev)
{
    if (drv->vendor_mask && drv->vendor_mask != dev->vendor_id) return TYKID_FALSE;
    if (drv->device_mask && drv->device_mask != dev->device_id) return TYKID_FALSE;

    for (u8 i = 0; i < drv->hw_class_count; i++) {
        if (drv->hw_classes[i] == dev->ty_class) return TYKID_TRUE;
    }
    return drv->hw_class_count == 0 ? TYKID_TRUE : TYKID_FALSE;
}

static u32
ty_score_driver(const tykid_driver_desc_t *drv,
                const tykid_hw_device_t   *dev)
{
    u32 score = 0;
    score += (u32)drv->priority * 1000u;

    if (drv->vendor_mask && drv->vendor_mask == dev->vendor_id) {
        score += drv->device_mask && drv->device_mask == dev->device_id
               ? 4000u : 2000u;
    }

    tykid_hwclass_t cls = dev->ty_class;
    u32 major = (cls >> 16) & 0xFFFF;
    u32 minor = (cls >>  0) & 0xFFFF;
    if (major && minor) score += 1000u;
    else if (major)     score += 500u;

    if (drv->flags & TYKID_DRV_FLAG_FALLBACK) {
        score = score > 3000u ? score - 3000u : 0u;
    }
    return score;
}

static void
ty_apply_exclusive_constraint(tykid_gate_ctx_t *ctx,
                               bool8 selected[TYKID_MAX_DRIVERS],
                               tykid_hwclass_t  class_of[TYKID_MAX_DRIVERS])
{
    ty_driver_registry_t *reg = &ctx->reg;
    for (u32 i = 0; i < reg->count; i++) {
        if (!selected[i]) continue;
        const tykid_driver_desc_t *drv = &reg->entries[i];
        if (!(drv->flags & TYKID_DRV_FLAG_EXCLUSIVE)) continue;

        for (u8 ci = 0; ci < drv->hw_class_count; ci++) {
            tykid_hwclass_t cls = drv->hw_classes[ci];
            for (u32 j = 0; j < reg->count; j++) {
                if (j == i || !selected[j]) continue;
                if (class_of[j] == cls) {
                    TY_LOG(ctx, TY_LOG_DEBUG,
                           "EXCLUSIVE: '%s' suppresses '%s' for class %08x",
                           drv->name, reg->entries[j].name, cls);
                    selected[j] = TYKID_FALSE;
                }
            }
        }
    }
}

TYKID_INTERNAL tykid_status_t
ty_gate_load_one(tykid_gate_ctx_t *ctx, tykid_driver_desc_t *drv)
{

    if (drv->state == TYKID_DRV_STATE_ACTIVE)
        return TYKID_ERR_ALREADY_LOADED;

    if (drv->state == TYKID_DRV_STATE_BLOCKED) {
        TY_LOG(ctx, TY_LOG_WARN,
               "Refusing to load threat-blocked driver '%s' (class=%s)",
               drv->name, tykid_threat_name(drv->threat_class));
        return TYKID_ERR_THREAT_BLOCKED;
    }

    if (drv->state == TYKID_DRV_STATE_SKIPPED) {
        TY_LOG(ctx, TY_LOG_DEBUG,
               "Skipping non-essential driver '%s'", drv->name);
        return TYKID_ERR_NOT_ESSENTIAL;
    }

    if (drv->flags & TYKID_DRV_FLAG_BLACKLISTED) {
        TY_LOG(ctx, TY_LOG_WARN, "Refusing to load blacklisted driver '%s'",
               drv->name);
        return TYKID_ERR_BLACKLIST;
    }

    if (drv->flags & TYKID_DRV_FLAG_SINGLETON) {
        for (u32 i = 0; i < ctx->reg.count; i++) {
            tykid_driver_desc_t *other = &ctx->reg.entries[i];
            if (other == drv) continue;
            if (ty_strncmp(other->name, drv->name, TYKID_MAX_NAME) == 0 &&
                other->state == TYKID_DRV_STATE_ACTIVE)
                return TYKID_ERR_ALREADY_LOADED;
        }
    }

    drv->state = TYKID_DRV_STATE_LOADING;

    char full_path[TYKID_MAX_PATH];
    const char *dir = ctx->cfg.drivers_dir ? ctx->cfg.drivers_dir : "../drivers";
    usz dlen = ty_strlen(dir);
    ty_strncpy(full_path, dir, TYKID_MAX_PATH);
    if (dlen < TYKID_MAX_PATH - 1 && dir[dlen-1] != '/') {
        full_path[dlen++] = '/';
        full_path[dlen]   = '\0';
    }
    ty_strncpy(full_path + dlen, drv->path, TYKID_MAX_PATH - dlen);

    tykid_status_t st = ty_integrity_check_file(ctx, full_path, drv->hmac);
    if (st != TYKID_OK) {
        TY_LOG(ctx, TY_LOG_ERROR,
               "Driver '%s' failed integrity check — BLOCKING (not panic)",
               drv->name);
        drv->flags |= TYKID_DRV_FLAG_BLACKLISTED;
        drv->state   = TYKID_DRV_STATE_BLOCKED;
        drv->threat_class = TY_THREAT_TAMPERED;
        ctx->total_blocked++;
        ctx->total_failed++;

        return TYKID_ERR_DRIVER_CORRUPT;
    }

    drv->flags |= TYKID_DRV_FLAG_VERIFIED;

    if (drv->flags & TYKID_DRV_FLAG_LAZY) {
        drv->state = TYKID_DRV_STATE_PENDING;
        TY_LOG(ctx, TY_LOG_INFO, "Driver '%s' deferred (LAZY flag)", drv->name);
        ctx->total_loaded++;
        return TYKID_OK;
    }

    if (__ty_unlikely(!ctx->cfg.ko_load_fn)) {
        TY_LOG(ctx, TY_LOG_ERROR, "ko_load_fn is NULL — cannot load '%s'", drv->name);
        drv->state = TYKID_DRV_STATE_FAILED;
        ctx->total_failed++;
        return TYKID_ERR_GENERIC;
    }

    uptr base     = 0;
    u8   attempts = 0;
    u32  timeout_ms = ctx->cfg.probe_timeout_ms
                    ? ctx->cfg.probe_timeout_ms
                    : 500U;
    u64  deadline = kobalt_tsc_read() + (u64)timeout_ms * 1000000ULL;

    do {
        st = ctx->cfg.ko_load_fn(full_path, &base);
        attempts++;
        if (kobalt_tsc_read() >= deadline) {
            TY_LOG(ctx, TY_LOG_ERROR, "Driver '%s' load TIMEOUT after %ums",
                   drv->name, timeout_ms);
            drv->state = TYKID_DRV_STATE_FAILED;
            ctx->total_failed++;
            return TYKID_ERR_TIMEOUT;
        }
    } while (st != TYKID_OK && attempts < ctx->cfg.max_load_attempts);

    if (st != TYKID_OK || base == 0) {
        TY_LOG(ctx, TY_LOG_ERROR,
               "Driver '%s' load FAILED after %u attempt(s): %s%s",
               drv->name, attempts, tykid_strerror(st),
               (drv->flags & TYKID_DRV_FLAG_CRITICAL)
               ? " [CRITICAL — boot may degrade]" : "");

        drv->state = TYKID_DRV_STATE_FAILED;
        ctx->total_failed++;

        if (drv->flags & TYKID_DRV_FLAG_CRITICAL) {
            TY_LOG(ctx, TY_LOG_FATAL,
                   "CRITICAL DRIVER '%s' FAILED TO LOAD — system may be unstable",
                   drv->name);
        }

        return st;
    }

    drv->base_vaddr     = base;
    drv->state          = TYKID_DRV_STATE_ACTIVE;
    drv->load_timestamp = kobalt_tsc_read();
    ctx->total_loaded++;

    TY_LOG(ctx, TY_LOG_INFO,
           "Driver '%s' loaded at vaddr=0x%016llx (attempt %u)",
           drv->name, (unsigned long long)base, attempts);
    return TYKID_OK;
}

TYKID_API tykid_status_t
tykid_arbitrate(tykid_gate_ctx_t *ctx,
                const tykid_hw_enumset_t *hw,
                tykid_load_result_t *result)
{
    if (__ty_unlikely(!ctx || !hw || !result)) return TYKID_ERR_GENERIC;

    tykid_status_t st = tykid_verify_seal(ctx);
    if (__ty_unlikely(st != TYKID_OK)) return st;

    u64 current_topo = ty_hw_topology_hash(hw, ctx->session_key);
    if (ctx->hw.probed && current_topo != ctx->hw.probe_hash) {
        TY_LOG(ctx, TY_LOG_WARN,
               "HW topology changed (old=%016llx new=%016llx) — re-scanning",
               (unsigned long long)ctx->hw.probe_hash,
               (unsigned long long)current_topo);
        st = ty_registry_scan(ctx);
        if (st != TYKID_OK) return st;
        ctx->hw.probe_hash = current_topo;
    }

    ty_selector_apply(ctx);

    ty_driver_registry_t *reg = &ctx->reg;

    bool8           selected[TYKID_MAX_DRIVERS];
    u32             best_score[TYKID_MAX_DRIVERS];
    tykid_hwclass_t class_of[TYKID_MAX_DRIVERS];
    ty_memzero_secure(selected,   sizeof(selected));
    ty_memzero_secure(best_score, sizeof(best_score));
    ty_memzero_secure(class_of,   sizeof(class_of));

    for (u32 di = 0; di < hw->count; di++) {
        const tykid_hw_device_t *dev = &hw->devices[di];
        if (dev->ty_class == TYKID_HW_UNKNOWN) {
            TY_LOG(ctx, TY_LOG_DEBUG,
                   "Device %04x:%04x UNKNOWN class — no driver",
                   dev->vendor_id, dev->device_id);
            continue;
        }

        u32 hw_best_score = 0;
        s32 hw_best_drv   = -1;

        for (u32 ri = 0; ri < reg->count; ri++) {
            const tykid_driver_desc_t *drv = &reg->entries[ri];

            if (drv->state == TYKID_DRV_STATE_BLOCKED) continue;
            if (drv->state == TYKID_DRV_STATE_SKIPPED) continue;
            if (drv->flags & TYKID_DRV_FLAG_BLACKLISTED) continue;

            if (!ty_driver_matches_hw(drv, dev)) continue;

            u32 score = ty_score_driver(drv, dev);
            TY_LOG(ctx, TY_LOG_TRACE,
                   "Score: drv='%s' hw='%s' score=%u",
                   drv->name, dev->name, score);

            if (score > hw_best_score) {
                hw_best_score = score;
                hw_best_drv   = (s32)ri;
            }

            if (drv->flags & TYKID_DRV_FLAG_FALLBACK) {
                if (!selected[ri] || score > best_score[ri]) {
                    selected[ri]   = TYKID_TRUE;
                    best_score[ri] = score;
                    class_of[ri]   = dev->ty_class;
                }
            }
        }

        if (hw_best_drv >= 0) {
            u32 ri = (u32)hw_best_drv;
            selected[ri]   = TYKID_TRUE;
            best_score[ri] = hw_best_score;
            class_of[ri]   = dev->ty_class;
            TY_LOG(ctx, TY_LOG_DEBUG,
                   "Matched: hw='%s' -> drv='%s' (score=%u)",
                   dev->name, reg->entries[ri].name, hw_best_score);
        } else {
            TY_LOG(ctx, TY_LOG_WARN,
                   "No driver for '%s' (class=%08x vendor=%04x device=%04x)",
                   dev->name, dev->ty_class, dev->vendor_id, dev->device_id);
        }
    }

    ty_apply_exclusive_constraint(ctx, selected, class_of);

    bool8 changed = TYKID_TRUE;
    while (changed) {
        changed = TYKID_FALSE;
        for (u32 ri = 0; ri < reg->count; ri++) {
            if (!selected[ri]) continue;
            const tykid_driver_desc_t *drv = &reg->entries[ri];
            for (u8 di2 = 0; di2 < drv->dep_count; di2++) {
                u8 dep = drv->deps[di2];
                if (dep == 0xFF || dep >= reg->count) continue;

                if (reg->entries[dep].state == TYKID_DRV_STATE_BLOCKED) {
                    TY_LOG(ctx, TY_LOG_ERROR,
                           "Dep '%s' of '%s' is BLOCKED — dep chain broken",
                           reg->entries[dep].name, drv->name);
                    continue;
                }
                if (!selected[dep]) {
                    selected[dep] = TYKID_TRUE;
                    changed = TYKID_TRUE;
                    TY_LOG(ctx, TY_LOG_DEBUG,
                           "Dep pulled: '%s' requires '%s'",
                           drv->name, reg->entries[dep].name);
                }
            }
        }
    }

    ty_memzero_secure(result, sizeof(*result));

    const u8 *topo = reg->deps.topo;
    u8 topo_n      = reg->deps.topo_count;

    u8 seq[TYKID_MAX_DRIVERS];
    if (topo_n == 0) {
        for (u32 i = 0; i < reg->count; i++) seq[i] = (u8)i;
        topo   = seq;
        topo_n = (u8)reg->count;
    }

    for (u8 ti = 0; ti < topo_n; ti++) {
        u8 ri = topo[ti];
        if (ri >= reg->count || !selected[ri]) continue;

        tykid_driver_desc_t *drv = &reg->entries[ri];

        if (drv->state == TYKID_DRV_STATE_BLOCKED ||
            drv->state == TYKID_DRV_STATE_SKIPPED) {
            result->skipped_count++;
            continue;
        }

        bool8 dep_ok = TYKID_TRUE;
        for (u8 di2 = 0; di2 < drv->dep_count; di2++) {
            u8 dep = drv->deps[di2];
            if (dep == 0xFF || dep >= reg->count) continue;
            tykid_drv_state_t dep_state = reg->entries[dep].state;
            if (dep_state != TYKID_DRV_STATE_ACTIVE &&
                dep_state != TYKID_DRV_STATE_PENDING) {
                TY_LOG(ctx, TY_LOG_ERROR,
                       "Driver '%s': dep '%s' not active (state=%d) — skip",
                       drv->name, reg->entries[dep].name, dep_state);
                dep_ok = TYKID_FALSE;
                break;
            }
        }
        if (!dep_ok) {
            drv->state = TYKID_DRV_STATE_FAILED;
            ctx->total_failed++;
            result->failed_count++;
            continue;
        }

        st = ty_gate_load_one(ctx, drv);

        if (st == TYKID_OK) {
            result->loaded[result->loaded_count++] = drv;
        } else if (st == TYKID_ERR_ALREADY_LOADED ||
                   st == TYKID_ERR_NOT_ESSENTIAL) {
            result->skipped_count++;
        } else if (st == TYKID_ERR_THREAT_BLOCKED) {

            result->skipped_count++;
        } else {
            result->failed_count++;
        }
    }

    TY_LOG(ctx, TY_LOG_INFO,
           "Arbitration complete: loaded=%u skipped=%u failed=%u blocked=%u",
           result->loaded_count, result->skipped_count,
           result->failed_count, ctx->total_blocked);

    return (result->failed_count == 0) ? TYKID_OK : TYKID_ERR_GENERIC;
}

TYKID_API tykid_status_t
tykid_load_driver(tykid_gate_ctx_t *ctx, const char *name)
{
    if (!ctx || !name) return TYKID_ERR_GENERIC;
    tykid_status_t st = tykid_verify_seal(ctx);
    if (st != TYKID_OK) return st;

    s32 idx = ty_find_driver_by_name(ctx, name);
    if (idx < 0) return TYKID_ERR_NO_DRIVER;
    return ty_gate_load_one(ctx, &ctx->reg.entries[(u32)idx]);
}

TYKID_API tykid_status_t
tykid_unload_driver(tykid_gate_ctx_t *ctx, const char *name)
{
    if (!ctx || !name) return TYKID_ERR_GENERIC;
    tykid_status_t st = tykid_verify_seal(ctx);
    if (st != TYKID_OK) return st;

    s32 idx = ty_find_driver_by_name(ctx, name);
    if (idx < 0) return TYKID_ERR_NO_DRIVER;

    tykid_driver_desc_t *drv = &ctx->reg.entries[(u32)idx];
    if (drv->state != TYKID_DRV_STATE_ACTIVE) return TYKID_ERR_GENERIC;
    if (!ctx->cfg.ko_unload_fn)               return TYKID_ERR_GENERIC;

    st = ctx->cfg.ko_unload_fn(drv->base_vaddr);
    if (st == TYKID_OK) {
        drv->state      = TYKID_DRV_STATE_UNLOADED;
        drv->base_vaddr = 0;
        ctx->total_loaded = ctx->total_loaded > 0 ? ctx->total_loaded - 1 : 0;
    }
    return st;
}

TYKID_API tykid_status_t
tykid_blacklist_driver(tykid_gate_ctx_t *ctx, const char *name)
{
    if (!ctx || !name) return TYKID_ERR_GENERIC;
    s32 idx = ty_find_driver_by_name(ctx, name);
    if (idx < 0) return TYKID_ERR_NO_DRIVER;
    ctx->reg.entries[(u32)idx].flags |= TYKID_DRV_FLAG_BLACKLISTED;
    TY_LOG(ctx, TY_LOG_INFO, "Driver '%s' blacklisted by request", name);
    return TYKID_OK;
}

TYKID_API tykid_status_t
tykid_verify_driver_integrity(tykid_gate_ctx_t *ctx,
                                const tykid_driver_desc_t *drv)
{
    if (!ctx || !drv) return TYKID_ERR_GENERIC;
    char full_path[TYKID_MAX_PATH];
    const char *dir = ctx->cfg.drivers_dir ? ctx->cfg.drivers_dir : "../drivers";
    usz dlen = ty_strlen(dir);
    ty_strncpy(full_path, dir, TYKID_MAX_PATH);
    if (dlen < TYKID_MAX_PATH - 1 && dir[dlen-1] != '/') {
        full_path[dlen++] = '/';
        full_path[dlen]   = '\0';
    }
    ty_strncpy(full_path + dlen, drv->path, TYKID_MAX_PATH - dlen);
    return ty_integrity_check_file(ctx, full_path, drv->hmac);
}

TYKID_API tykid_status_t
tykid_recheck_all(tykid_gate_ctx_t *ctx)
{
    if (!ctx) return TYKID_ERR_GENERIC;
    tykid_status_t st = tykid_verify_seal(ctx);
    if (st != TYKID_OK) return st;

    u32 fail_count = 0;
    for (u32 i = 0; i < ctx->reg.count; i++) {
        tykid_driver_desc_t *drv = &ctx->reg.entries[i];
        if (drv->state != TYKID_DRV_STATE_ACTIVE) continue;

        tykid_status_t ds = tykid_verify_driver_integrity(ctx, drv);
        if (ds != TYKID_OK) {
            const char *crit = (drv->flags & TYKID_DRV_FLAG_CRITICAL)
                               ? " [CRITICAL]" : "";
            TY_LOG(ctx, TY_LOG_FATAL,
                   "Recheck: driver '%s' TAMPERED post-load%s — blocking",
                   drv->name, crit);

            drv->flags    |= TYKID_DRV_FLAG_BLACKLISTED;
            drv->state     = TYKID_DRV_STATE_BLOCKED;
            drv->threat_class = TY_THREAT_TAMPERED;
            ctx->total_blocked++;

            if (ctx->cfg.ko_unload_fn && drv->base_vaddr) {
                ctx->cfg.ko_unload_fn(drv->base_vaddr);
                drv->base_vaddr = 0;
            }

            fail_count++;

        }
    }

    ctx->recheck_count++;
    return fail_count == 0 ? TYKID_OK : TYKID_ERR_DRIVER_CORRUPT;
}

const u8 *tykid_session_key_ptr(const tykid_gate_ctx_t *ctx)
{
    return ctx->session_key;
}

TYKID_API tykid_status_t
tykid_revoke_driver(tykid_gate_ctx_t *ctx, const char *name)
{
    if (!ctx || !name) return TYKID_ERR_GENERIC;
    tykid_status_t st = tykid_verify_seal(ctx);
    if (st != TYKID_OK) return st;

    s32 idx = ty_find_driver_by_name(ctx, name);
    if (idx < 0) return TYKID_ERR_NO_DRIVER;

    tykid_driver_desc_t *drv = &ctx->reg.entries[(u32)idx];

    if (ctx->cfg.iommu_block_fn)
        ctx->cfg.iommu_block_fn(drv->base_vaddr);

    if (ctx->cfg.ko_unload_fn && drv->base_vaddr) {
        ctx->cfg.ko_unload_fn(drv->base_vaddr);
        drv->base_vaddr = 0;
    }

    drv->state        = TYKID_DRV_STATE_BLOCKED;
    drv->threat_class = TY_THREAT_REVOKED;
    drv->flags       |= TYKID_DRV_FLAG_BLACKLISTED;
    ctx->total_revoked++;

    TY_LOG(ctx, TY_LOG_WARN, "Driver '%s' revoked at runtime", name);
    return TYKID_OK;
}
