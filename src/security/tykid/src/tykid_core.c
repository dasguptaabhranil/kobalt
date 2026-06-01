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

static usz ty_u32_to_hex(char *buf, usz cap, u32 v) {
    if (cap < 11) return 0;
    static const char hex[] = "0123456789abcdef";
    buf[0] = '0'; buf[1] = 'x';
    for (u32 i = 0; i < 8; i++)
        buf[2 + i] = hex[(v >> (28 - i*4)) & 0xF];
    buf[10] = '\0';
    return 10;
}

static usz ty_u32_to_dec(char *buf, usz cap, u32 v) {
    if (cap < 12) return 0;
    if (v == 0) { buf[0]='0'; buf[1]='\0'; return 1; }
    char tmp[12]; usz i = 0;
    while (v) { tmp[i++] = (char)('0' + v % 10); v /= 10; }
    for (usz j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];
    buf[i] = '\0'; return i;
}

#define TY_APPEND(buf, pos, cap, str) do {          \
    const char *_s = (str); usz _cap = (cap);       \
    while (*_s && (pos) < _cap - 1)                 \
        (buf)[(pos)++] = *_s++;                     \
    (buf)[(pos)] = '\0';                            \
} while(0)

extern TYKID_NORETURN void kobalt_kernel_panic(const char *msg);

TYKID_INTERNAL TYKID_NORETURN TYKID_COLD void
ty_panic(tykid_gate_ctx_t *ctx, const char *reason)
{
    if (ctx) ty_ctx_poison(ctx);
    kobalt_kernel_panic(reason ? reason : "TYKID: gate context corrupted");

    __builtin_unreachable();
}

TYKID_INTERNAL void
ty_ctx_poison(tykid_gate_ctx_t *ctx)
{

    ty_memzero_secure(ctx->session_key, sizeof(ctx->session_key));
    ty_memzero_secure(ctx->hmac_key,    sizeof(ctx->hmac_key));
    ty_memzero_secure(&ctx->csprng,     sizeof(ctx->csprng));
    ty_memzero_secure(&ctx->entropy,    sizeof(ctx->entropy));

    ty_memzero_secure(&ctx->threat,     sizeof(ctx->threat));

    ctx->magic_a    = 0;
    ctx->magic_b    = 0;
    ctx->seal_check = 0;
}

TYKID_INTERNAL s32
ty_find_driver_by_name(const tykid_gate_ctx_t *ctx, const char *name)
{
    const ty_driver_registry_t *reg = &ctx->reg;
    for (u32 i = 0; i < reg->count; i++) {
        if (ty_strncmp(reg->entries[i].name, name, TYKID_MAX_NAME) == 0)
            return (s32)i;
    }
    return -1;
}

TYKID_API tykid_status_t
tykid_verify_seal(const tykid_gate_ctx_t *ctx)
{
    if (__ty_unlikely(!ctx)) return TYKID_ERR_SEAL_BROKEN;

    const u32 exp_a    = TY_CTX_MAGIC_A;
    const u32 exp_b    = TY_CTX_MAGIC_B;
    const u64 exp_seal = __TYKID_KOBALT_EXPECTED;

    bool8 ok_a = ty_memeq(&ctx->magic_a,    &exp_a,    4);
    bool8 ok_b = ty_memeq(&ctx->magic_b,    &exp_b,    4);
    bool8 ok_s = ty_memeq(&ctx->seal_check, &exp_seal, 8);

    if (__ty_unlikely(!(ok_a & ok_b & ok_s))) {

        return TYKID_ERR_SEAL_BROKEN;
    }
    return TYKID_OK;
}

TYKID_API tykid_status_t
tykid_init(const tykid_config_t *cfg, tykid_gate_ctx_t **ctx_out)
{
    if (__ty_unlikely(!cfg || !ctx_out)) return TYKID_ERR_GENERIC;
    if (__ty_unlikely(!cfg->alloc_fn || !cfg->free_fn))
        return TYKID_ERR_GENERIC;

    tykid_gate_ctx_t *ctx =
        (tykid_gate_ctx_t *)cfg->alloc_fn(sizeof(tykid_gate_ctx_t));
    if (__ty_unlikely(!ctx)) return TYKID_ERR_ALLOC;

    ty_memzero_secure(ctx, sizeof(*ctx));

    ctx->magic_a    = TY_CTX_MAGIC_A;
    ctx->magic_b    = TY_CTX_MAGIC_B;
    ctx->seal_check = __TYKID_KOBALT_EXPECTED;
    ctx->boot_token = cfg->kobalt_boot_token;

    ctx->cfg = *cfg;

    tykid_status_t st = tykid_verify_seal(ctx);
    if (__ty_unlikely(st != TYKID_OK)) {

        ty_memzero_secure(ctx, sizeof(*ctx));
        cfg->free_fn(ctx, sizeof(*ctx));
        return TYKID_ERR_SEAL_BROKEN;
    }

    st = ty_entropy_seed(ctx);
    if (__ty_unlikely(st != TYKID_OK)) {
        ty_ctx_poison(ctx);
        cfg->free_fn(ctx, sizeof(*ctx));
        return st;
    }

    st = ty_derive_session_keys(ctx);
    if (__ty_unlikely(st != TYKID_OK)) {
        ty_ctx_poison(ctx);
        cfg->free_fn(ctx, sizeof(*ctx));
        return st;
    }

    for (u32 i = 0; i < 32; i++) {
        ctx->hmac_key[i] = cfg->hmac_master_key[i]
                         ^ (u8)(ctx->boot_token >> (8 * (i & 7)));
    }

    if (cfg->vfs_read_fn != NULL) {
        st = ty_bearssl_scan_init(ctx);
        if (__ty_unlikely(st != TYKID_OK)) {
            TY_LOG(ctx, TY_LOG_WARN,
                   "BearSSL scan init failed (%d) — threat scanning disabled",
                   st);

        }
    } else {
        TY_LOG(ctx, TY_LOG_WARN,
               "vfs_read_fn not set — threat scanning disabled for this boot");
    }

    st = ty_registry_scan(ctx);
    if (__ty_unlikely(st != TYKID_OK && st != TYKID_ERR_CYCLE)) {

        TY_LOG(ctx, TY_LOG_WARN, "Registry scan returned %d — continuing", st);
    }
    if (st == TYKID_ERR_CYCLE) {
        TY_LOG(ctx, TY_LOG_ERROR, "Dependency cycle in driver registry");
        ty_ctx_poison(ctx);
        cfg->free_fn(ctx, sizeof(*ctx));
        return st;
    }

    ctx->init_timestamp = ty_entropy_u64(ctx);

    TY_LOG(ctx, TY_LOG_INFO,
           "TYKID initialised. version='%s' drivers=%u seal=OK",
           TYKID_VERSION_STRING, ctx->reg.count);

    *ctx_out = ctx;
    return TYKID_OK;
}

TYKID_API void
tykid_shutdown(tykid_gate_ctx_t *ctx)
{
    if (!ctx) return;

    for (s32 i = (s32)ctx->reg.count - 1; i >= 0; i--) {
        tykid_driver_desc_t *drv = &ctx->reg.entries[i];
        if (drv->state == TYKID_DRV_STATE_ACTIVE
         && ctx->cfg.ko_unload_fn
         && drv->base_vaddr) {
            TY_LOG(ctx, TY_LOG_INFO, "Shutdown: unloading '%s'", drv->name);
            ctx->cfg.ko_unload_fn(drv->base_vaddr);
            drv->state      = TYKID_DRV_STATE_UNLOADED;
            drv->base_vaddr = 0;
        }
    }

    tykid_free_fn_t free_fn = ctx->cfg.free_fn;
    ty_ctx_poison(ctx);
    ty_memzero_secure(ctx, sizeof(*ctx));
    if (free_fn) free_fn(ctx, sizeof(*ctx));
}

TYKID_API const char *
tykid_version_string(void)
{
    return TYKID_VERSION_STRING;
}

TYKID_API tykid_drv_state_t
tykid_driver_state(const tykid_gate_ctx_t *ctx, const char *name)
{
    if (!ctx || !name) return TYKID_DRV_STATE_ABSENT;
    s32 idx = ty_find_driver_by_name(ctx, name);
    if (idx < 0) return TYKID_DRV_STATE_ABSENT;
    return ctx->reg.entries[(u32)idx].state;
}

TYKID_API u32
tykid_active_driver_count(const tykid_gate_ctx_t *ctx)
{
    if (!ctx) return 0;
    u32 n = 0;
    for (u32 i = 0; i < ctx->reg.count; i++) {
        if (ctx->reg.entries[i].state == TYKID_DRV_STATE_ACTIVE)
            n++;
    }
    return n;
}

TYKID_API tykid_status_t
tykid_dump_state(const tykid_gate_ctx_t *ctx, char *buf, usz buf_sz)
{
    if (!ctx || !buf || buf_sz < 64) return TYKID_ERR_GENERIC;

    usz pos = 0;
    char tmp[32];

    TY_APPEND(buf, pos, buf_sz, "TYKID State Dump\n");
    TY_APPEND(buf, pos, buf_sz, "Version: ");
    TY_APPEND(buf, pos, buf_sz, TYKID_VERSION_STRING);
    TY_APPEND(buf, pos, buf_sz, "\nSeal: ");
    TY_APPEND(buf, pos, buf_sz,
              tykid_verify_seal(ctx) == TYKID_OK ? "OK" : "BROKEN");
    TY_APPEND(buf, pos, buf_sz, "\nSafe mode: ");
    TY_APPEND(buf, pos, buf_sz, ctx->safe_mode ? "YES" : "no");
    TY_APPEND(buf, pos, buf_sz, "\nEssential mask: ");
    ty_u32_to_hex(tmp, sizeof(tmp), (u32)ctx->essential_mask);
    TY_APPEND(buf, pos, buf_sz, tmp);
    TY_APPEND(buf, pos, buf_sz, "\nDrivers in registry: ");
    ty_u32_to_dec(tmp, sizeof(tmp), ctx->reg.count);
    TY_APPEND(buf, pos, buf_sz, tmp);
    TY_APPEND(buf, pos, buf_sz, "\nActive: ");
    ty_u32_to_dec(tmp, sizeof(tmp), tykid_active_driver_count(ctx));
    TY_APPEND(buf, pos, buf_sz, tmp);
    TY_APPEND(buf, pos, buf_sz, "\nTotal loaded: ");
    ty_u32_to_dec(tmp, sizeof(tmp), ctx->total_loaded);
    TY_APPEND(buf, pos, buf_sz, tmp);
    TY_APPEND(buf, pos, buf_sz, "\nTotal blocked (threat): ");
    ty_u32_to_dec(tmp, sizeof(tmp), ctx->total_blocked);
    TY_APPEND(buf, pos, buf_sz, tmp);
    TY_APPEND(buf, pos, buf_sz, "\nTotal skipped (non-essential): ");
    ty_u32_to_dec(tmp, sizeof(tmp), ctx->total_skipped);
    TY_APPEND(buf, pos, buf_sz, tmp);
    TY_APPEND(buf, pos, buf_sz, "\nTotal failed: ");
    ty_u32_to_dec(tmp, sizeof(tmp), ctx->total_failed);
    TY_APPEND(buf, pos, buf_sz, tmp);
    TY_APPEND(buf, pos, buf_sz, "\nRechecks: ");
    ty_u32_to_dec(tmp, sizeof(tmp), ctx->recheck_count);
    TY_APPEND(buf, pos, buf_sz, tmp);

    if (ctx->scan_summary.scan_complete) {
        TY_APPEND(buf, pos, buf_sz, "\nThreat scan: scanned=");
        ty_u32_to_dec(tmp, sizeof(tmp), ctx->scan_summary.scanned);
        TY_APPEND(buf, pos, buf_sz, tmp);
        TY_APPEND(buf, pos, buf_sz, " clean=");
        ty_u32_to_dec(tmp, sizeof(tmp), ctx->scan_summary.clean);
        TY_APPEND(buf, pos, buf_sz, tmp);
        TY_APPEND(buf, pos, buf_sz, " blocked=");
        ty_u32_to_dec(tmp, sizeof(tmp), ctx->scan_summary.blocked);
        TY_APPEND(buf, pos, buf_sz, tmp);
        TY_APPEND(buf, pos, buf_sz, " suspicious=");
        ty_u32_to_dec(tmp, sizeof(tmp), ctx->scan_summary.suspicious);
        TY_APPEND(buf, pos, buf_sz, tmp);
        TY_APPEND(buf, pos, buf_sz, " unsigned=");
        ty_u32_to_dec(tmp, sizeof(tmp), ctx->scan_summary.unsigned_count);
        TY_APPEND(buf, pos, buf_sz, tmp);
    } else {
        TY_APPEND(buf, pos, buf_sz, "\nThreat scan: not yet run");
    }

    TY_APPEND(buf, pos, buf_sz, "\nDir hash: ");
    ty_u32_to_hex(tmp, sizeof(tmp), (u32)(ctx->reg.dir_hash >> 32));
    TY_APPEND(buf, pos, buf_sz, tmp);
    ty_u32_to_hex(tmp, sizeof(tmp), (u32)(ctx->reg.dir_hash & 0xFFFFFFFFULL));
    TY_APPEND(buf, pos, buf_sz, tmp);
    TY_APPEND(buf, pos, buf_sz, "\n\nDriver List:\n");

    static const char *state_names[] = {
 "ABSENT",
 "REGISTERED",
 "LOADING",
 "ACTIVE",
 "FAILED",
 "SUSPENDED",
 "UNLOADED",
 "BLOCKED",
 "SKIPPED",
    };
#define TY_STATE_NAME_COUNT  9

    for (u32 i = 0; i < ctx->reg.count; i++) {
        const tykid_driver_desc_t *drv = &ctx->reg.entries[i];
        TY_APPEND(buf, pos, buf_sz, "  [");
        ty_u32_to_dec(tmp, sizeof(tmp), i);
        TY_APPEND(buf, pos, buf_sz, tmp);
        TY_APPEND(buf, pos, buf_sz, "] ");
        TY_APPEND(buf, pos, buf_sz, drv->name);
        TY_APPEND(buf, pos, buf_sz, " state=");
        u8 s = drv->state;
        TY_APPEND(buf, pos, buf_sz,
                  s < TY_STATE_NAME_COUNT ? state_names[s] : "INVALID");
        if (drv->state == TYKID_DRV_STATE_BLOCKED) {
            TY_APPEND(buf, pos, buf_sz, "(");
            TY_APPEND(buf, pos, buf_sz, tykid_threat_name(drv->threat_class));
            TY_APPEND(buf, pos, buf_sz, ")");
        }
        TY_APPEND(buf, pos, buf_sz, " prio=");
        ty_u32_to_dec(tmp, sizeof(tmp), drv->priority);
        TY_APPEND(buf, pos, buf_sz, tmp);
        TY_APPEND(buf, pos, buf_sz, " flags=");
        ty_u32_to_hex(tmp, sizeof(tmp), drv->flags);
        TY_APPEND(buf, pos, buf_sz, tmp);
        TY_APPEND(buf, pos, buf_sz, "\n");
    }
#undef TY_STATE_NAME_COUNT

    return TYKID_OK;
}

TYKID_API const char *
tykid_strerror(tykid_status_t st)
{
    switch (st) {
    case TYKID_OK:                  return "OK";
    case TYKID_ERR_GENERIC:         return "Generic error";
    case TYKID_ERR_NULL_PTR:        return "Null pointer";
    case TYKID_ERR_SEAL_BROKEN:     return "Kobalt kernel seal broken";
    case TYKID_ERR_HW_ENUM:         return "Hardware enumeration failed";
    case TYKID_ERR_NO_DRIVER:       return "No matching driver found";
    case TYKID_ERR_DRIVER_CORRUPT:  return "Driver integrity check failed";
    case TYKID_ERR_ENTROPY_LOW:     return "Insufficient entropy";
    case TYKID_ERR_ALLOC:           return "Memory allocation failure";
    case TYKID_ERR_PATH:            return "Bad driver path or unreadable file";
    case TYKID_ERR_PERM:            return "Permission denied";
    case TYKID_ERR_CYCLE:           return "Dependency cycle detected";
    case TYKID_ERR_TIMEOUT:         return "Probe timeout";
    case TYKID_ERR_ALREADY_LOADED:  return "Driver already loaded";
    case TYKID_ERR_VERSION:         return "ABI version mismatch";
    case TYKID_ERR_BLACKLIST:       return "Driver is blacklisted";
    case TYKID_ERR_DEP_FAILED:      return "Dependency failed to load";
    case TYKID_ERR_INTERNAL:        return "Internal unrecoverable error";
    case TYKID_ERR_NOT_INIT:        return "TYKID not yet initialised";
    case TYKID_ERR_THREAT_BLOCKED:  return "Driver blocked by threat scanner";
    case TYKID_ERR_NOT_ESSENTIAL:   return "Driver not essential for present hardware";
    case TYKID_ERR_CERT_INVALID:    return "Driver certificate invalid or missing";
    case TYKID_ERR_ENTROPY_ANOMALY: return "Suspicious entropy (possible packing)";
    case TYKID_ERR_ELF_MALFORMED:   return "ELF binary is malformed or corrupt";
    case TYKID_ERR_PATTERN_MATCH:   return "Known malware byte pattern detected";
    default:                        return "Unknown error code";
    }
}
