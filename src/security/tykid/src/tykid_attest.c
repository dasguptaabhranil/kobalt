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

extern void ty_siphash128_pub(const u8 *key, const u8 *in, usz inlen, u8 out[16]);

#define ATTEST_MAGIC  0x54594B41UL
#define ATTEST_VER    1U

typedef struct TYKID_PACKED {
    u32 magic;
    u8  version;
    u8  iommu_present;
    u8  driver_count;
    u8  _pad;
    u64 tsc;
    u64 boot_token_hash;
    u64 topology_hash;
    u8  hyperseal[32];
    u8  session_fingerprint[16];
    u8  hmac[32];
} ty_attest_blob_t;

TYKID_API tykid_status_t
tykid_attest_export(tykid_gate_ctx_t *ctx, u8 *buf, usz buf_sz, usz *written)
{
    if (!ctx || !buf || !written) return TYKID_ERR_GENERIC;
    if (buf_sz < sizeof(ty_attest_blob_t)) return TYKID_ERR_GENERIC;

    tykid_status_t st = tykid_verify_seal(ctx);
    if (st != TYKID_OK) return st;

    ty_attest_blob_t blob;
    ty_memzero_secure(&blob, sizeof(blob));

    blob.magic             = ATTEST_MAGIC;
    blob.version           = ATTEST_VER;
    blob.iommu_present     = (ctx->essential_mask & TY_ESSENTIAL_IOMMU) ? 1 : 0;
    blob.driver_count      = (u8)(ctx->total_loaded & 0xFF);
    blob.tsc               = kobalt_tsc_read();
    blob.topology_hash     = ctx->hw.probe_hash;

    u8 bt_buf[8];
    ty_put_u64le(bt_buf, ctx->boot_token);
    u64 bt_hash = ty_siphash24(ctx->session_key, bt_buf, 8);
    blob.boot_token_hash   = bt_hash;

    extern ty_ext_seal_ctx_t g_ext_seal;
    if (g_ext_seal.frozen)
        ty_memcpy(blob.hyperseal, g_ext_seal.hyperseal, 32);

    ty_siphash128_pub(ctx->session_key, ctx->session_key + 16, 16,
                      blob.session_fingerprint);

    ty_hmac_blake2s(ctx->hmac_key, 32,
                    (const u8 *)&blob, sizeof(blob) - 32,
                    blob.hmac);

    ty_memcpy(buf, &blob, sizeof(blob));
    *written = sizeof(blob);

    ty_memzero_secure(&blob, sizeof(blob));
    return TYKID_OK;
}
