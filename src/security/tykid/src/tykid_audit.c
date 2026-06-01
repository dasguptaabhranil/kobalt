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

#include "tykid_internal.h"

typedef u32 ty_audit_event_t;

#define AEV_INIT_OK            0x00000001U
#define AEV_INIT_FAIL          0x00000002U
#define AEV_SHUTDOWN           0x00000003U
#define AEV_SEAL_OK            0x00000010U
#define AEV_SEAL_BROKEN        0x00000011U
#define AEV_HYPERSEAL_OK       0x00000012U
#define AEV_HYPERSEAL_BROKEN   0x00000013U
#define AEV_HW_ENUM_OK         0x00000020U
#define AEV_HW_ENUM_FAIL       0x00000021U
#define AEV_HW_HOTPLUG_ADD     0x00000022U
#define AEV_HW_HOTPLUG_REM     0x00000023U
#define AEV_HW_ACPI_MISMATCH   0x00000024U
#define AEV_DRV_LOAD_OK        0x00000030U
#define AEV_DRV_LOAD_FAIL      0x00000031U
#define AEV_DRV_UNLOAD         0x00000032U
#define AEV_DRV_BLACKLIST      0x00000033U
#define AEV_DRV_HMAC_FAIL      0x00000034U
#define AEV_DRV_RECHECK_FAIL   0x00000035U
#define AEV_DRV_REVOKED        0x00000036U
#define AEV_POL_VIOLATION      0x00000040U
#define AEV_POL_TAMPER         0x00000041U
#define AEV_POL_CAP_DENY       0x00000042U
#define AEV_IOMMU_BIND         0x00000050U
#define AEV_IOMMU_FAULT        0x00000051U
#define AEV_IOMMU_CANARY       0x00000052U
#define AEV_WDT_START          0x00000060U
#define AEV_WDT_SWEEP_OK       0x00000061U
#define AEV_WDT_SWEEP_FAIL     0x00000062U
#define AEV_WDT_DEADMAN        0x00000063U
#define AEV_WDT_DRV_TIMEOUT    0x00000064U
#define AEV_ENTROPY_LOW        0x00000070U
#define AEV_KEY_DERIVE_OK      0x00000071U
#define AEV_LOG_WRAP           0x00000080U
#define AEV_LOG_TAMPER         0x00000081U
#define AEV_SANDBOX_VIOLATION  0x00000090U
#define AEV_ATTEST_EXPORT      0x000000A0U

#define AUDIT_FLAG_NONE      0x0000U
#define AUDIT_FLAG_CRITICAL  0x0001U
#define AUDIT_FLAG_WRAP      0x0002U
#define AUDIT_FLAG_CHAIN_OK  0x0004U
#define AUDIT_FLAG_COMPACT   0x0008U

typedef struct TYKID_PACKED TYKID_ALIGNED(64) {
    u32 seq;
    u32 event_code;
    u64 timestamp;
    u32 driver_id;
    u16 detail_len;
    u16 flags;
    u8  detail[16];
    u8  prev_hash[16];
    u8  record_hmac[8];
} ty_audit_record_t;

TYKID_STATIC_ASSERT(sizeof(ty_audit_record_t) == 64, "audit record must be 64 bytes");

#define TY_AUDIT_RING_COUNT  512U
#define TY_AUDIT_RING_MASK   (TY_AUDIT_RING_COUNT - 1U)

TYKID_STATIC_ASSERT((TY_AUDIT_RING_COUNT & (TY_AUDIT_RING_COUNT - 1)) == 0,
                    "ring count must be power of 2");

typedef struct TYKID_ALIGNED(4096) {
    ty_audit_record_t ring[TY_AUDIT_RING_COUNT];
    u32               head;
    u32               total_written;
    u32               wrap_count;
    u8                last_hash[16];
    u64               chain_epoch;
    bool8             initialised;
} ty_audit_ring_t;

static TYKID_SECTION(".tykid.audit") ty_audit_ring_t g_audit_ring;
static ty_spinlock_t g_audit_lock = TY_SPIN_INIT;

static void ty_siphash128(const u8 *key16, const void *data, usz len, u8 out[16])
{
    u64 h0 = ty_siphash24(key16,     data, len);
    u64 h1 = ty_siphash24(key16 + 8, data, len);
    ty_put_u64le(out + 0, h0);
    ty_put_u64le(out + 8, h1);
}

static void ty_audit_record_hmac(const tykid_gate_ctx_t *ctx,
                                  const ty_audit_record_t *rec, u8 out[8])
{
    u8 digest[BLAKE2S_OUTBYTES];
    ty_hmac_blake2s(ctx->hmac_key, 32, (const u8 *)rec, 56, digest);
    ty_memcpy(out, digest, 8);
    ty_memzero_secure(digest, sizeof(digest));
}

static void ty_forward_critical(const ty_audit_record_t *rec)
{
    kobalt_log_critical("tykid.audit", rec, sizeof(*rec));
}

TYKID_INTERNAL void
ty_audit_append(tykid_gate_ctx_t *ctx,
                ty_audit_event_t  event,
                u32               driver_id,
                u16               flags,
                const u8         *detail,
                u16               detail_len)
{
    ty_audit_ring_t *ring = &g_audit_ring;
    if (!ring->initialised) return;

    ty_spin_lock(&g_audit_lock);

    u32 pos     = ring->head & TY_AUDIT_RING_MASK;
    bool8 wrap  = (ring->head > 0 && pos == 0);

    if (wrap) {
        ring->wrap_count++;

        if (ring->ring[pos].flags & AUDIT_FLAG_CRITICAL)
            ty_forward_critical(&ring->ring[pos]);
    }

    ty_audit_record_t *rec = &ring->ring[pos];
    rec->seq        = ring->total_written;
    rec->event_code = event;
    rec->timestamp  = kobalt_tsc_read();
    rec->driver_id  = driver_id;
    rec->flags      = flags;
    if (wrap) rec->flags |= AUDIT_FLAG_WRAP;

    u16 clen = detail_len < 16 ? detail_len : 16;
    if (detail && clen)
        ty_memcpy(rec->detail, detail, clen);
    else
        ty_memzero_secure(rec->detail, 16);
    rec->detail_len = clen;

    ty_memcpy(rec->prev_hash, ring->last_hash, 16);
    if (ring->total_written > 0) rec->flags |= AUDIT_FLAG_CHAIN_OK;

    ty_audit_record_hmac(ctx, rec, rec->record_hmac);

    ty_siphash128(ctx->session_key, rec, sizeof(*rec), ring->last_hash);
    ring->chain_epoch++;
    ring->head++;
    ring->total_written++;

    if (flags & AUDIT_FLAG_CRITICAL)
        ty_forward_critical(rec);

    ty_spin_unlock(&g_audit_lock);
}

TYKID_INTERNAL tykid_status_t ty_audit_init(tykid_gate_ctx_t *ctx)
{
    ty_audit_ring_t *ring = &g_audit_ring;
    ty_memzero_secure(ring, sizeof(*ring));
    ring->initialised = TYKID_TRUE;
    ty_audit_append(ctx, AEV_INIT_OK, 0xFFFFFFFFU, AUDIT_FLAG_NONE, NULL, 0);
    return TYKID_OK;
}

TYKID_INTERNAL tykid_status_t ty_audit_verify_chain(tykid_gate_ctx_t *ctx)
{
    ty_audit_ring_t *ring = &g_audit_ring;
    if (!ring->initialised || ring->total_written == 0) return TYKID_OK;

    u32 count = ring->total_written < TY_AUDIT_RING_COUNT
              ? ring->total_written : TY_AUDIT_RING_COUNT;
    u32 start = ring->total_written >= TY_AUDIT_RING_COUNT
              ? ring->head : 0;

    u8    prev_hash[16];
    bool8 first = TYKID_TRUE;
    u32   errors = 0;
    ty_memzero_secure(prev_hash, 16);

    for (u32 i = 0; i < count; i++) {
        u32 pos = (start + i) & TY_AUDIT_RING_MASK;
        const ty_audit_record_t *rec = &ring->ring[pos];

        u8 expected[8];
        ty_audit_record_hmac(ctx, rec, expected);
        if (!ty_memeq(rec->record_hmac, expected, 8)) {
            TY_LOG(ctx, TY_LOG_ERROR, "audit: seq=%u HMAC invalid", rec->seq);
            errors++;
        }

        if (!first && !ty_memeq(rec->prev_hash, prev_hash, 16)) {
            TY_LOG(ctx, TY_LOG_ERROR, "audit: seq=%u chain broken", rec->seq);
            errors++;
        }
        first = TYKID_FALSE;
        ty_siphash128(ctx->session_key, rec, sizeof(*rec), prev_hash);
    }

    if (errors) {
        ty_audit_append(ctx, AEV_LOG_TAMPER, 0xFFFFFFFFU, AUDIT_FLAG_CRITICAL, NULL, 0);
        return TYKID_ERR_DRIVER_CORRUPT;
    }
    return TYKID_OK;
}

TYKID_API tykid_status_t
tykid_audit_export(tykid_gate_ctx_t *ctx,
                   u32 start_seq, ty_audit_record_t *out, u32 cap, u32 *written)
{
    if (!ctx || !out || !written || !cap) return TYKID_ERR_GENERIC;
    tykid_status_t st = tykid_verify_seal(ctx);
    if (st != TYKID_OK) return st;

    ty_audit_ring_t *ring = &g_audit_ring;
    u32 n = 0;
    u32 count = ring->total_written < TY_AUDIT_RING_COUNT
              ? ring->total_written : TY_AUDIT_RING_COUNT;
    u32 s = ring->total_written >= TY_AUDIT_RING_COUNT ? ring->head : 0;

    for (u32 i = 0; i < count && n < cap; i++) {
        const ty_audit_record_t *rec = &ring->ring[(s + i) & TY_AUDIT_RING_MASK];
        if (rec->seq >= start_seq) out[n++] = *rec;
    }
    *written = n;
    return TYKID_OK;
}

TYKID_INTERNAL void
ty_audit_driver_event(tykid_gate_ctx_t *ctx, u32 event, u32 drv_id, const char *name)
{
    u8 detail[16];
    ty_memzero_secure(detail, sizeof(detail));
    usz nlen = ty_strlen(name);
    if (nlen > 16) nlen = 16;
    ty_memcpy(detail, name, nlen);
    u16 fl = (event >= AEV_DRV_HMAC_FAIL) ? AUDIT_FLAG_CRITICAL : AUDIT_FLAG_NONE;
    ty_audit_append(ctx, event, drv_id, fl, detail, (u16)nlen);
}

TYKID_INTERNAL void ty_audit_seal_event(tykid_gate_ctx_t *ctx, bool8 broken)
{
    ty_audit_append(ctx,
        broken ? AEV_SEAL_BROKEN : AEV_SEAL_OK,
        0xFFFFFFFFU,
        broken ? AUDIT_FLAG_CRITICAL : AUDIT_FLAG_NONE,
        NULL, 0);
}

TYKID_INTERNAL void ty_audit_wdt_event(tykid_gate_ctx_t *ctx, bool8 ok, u64 n)
{
    u8 d[8]; ty_put_u64le(d, n);
    ty_audit_append(ctx,
        ok ? AEV_WDT_SWEEP_OK : AEV_WDT_SWEEP_FAIL,
        0xFFFFFFFFU,
        ok ? AUDIT_FLAG_NONE : AUDIT_FLAG_CRITICAL,
        d, 8);
}
