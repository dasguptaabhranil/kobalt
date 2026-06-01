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

#include "flatfs_internal.h"
#include "flatfs_tykid.h"
#include "flatfs_crc.h"

#ifdef FLATFS_TYKID_AVAILABLE

typedef unsigned char      _ty_u8;
typedef unsigned long long _ty_u64;
typedef unsigned int       _ty_u32;
typedef __SIZE_TYPE__       _ty_usz;

extern _ty_u64 ty_siphash24(const _ty_u8 *key16,
                              const void   *data,
                              _ty_usz       len);
extern _ty_u64 ty_entropy_u64(tykid_gate_ctx_t *ctx);
extern void    ty_audit_append(tykid_gate_ctx_t *ctx,
                                _ty_u32           event,
                                _ty_u32           driver_id,
                                unsigned short    flags,
                                const _ty_u8     *detail,
                                unsigned short    detail_len);

#define FLATFS_TYKID_SESSION_KEY_OFF  offsetof_tykid_session_key()
static inline const _ty_u8 *tykid_ctx_session_key(const tykid_gate_ctx_t *ctx)
{

    extern const _ty_u8 *tykid_session_key_ptr(const tykid_gate_ctx_t *);
    return tykid_session_key_ptr(ctx);
}
#endif

#  define AEV_INIT_OK        0x00000001U
#  define AEV_INIT_FAIL      0x00000002U
#  define AEV_SHUTDOWN       0x00000003U
#  define AEV_SEAL_BROKEN    0x00000011U
#  define AEV_KEY_DERIVE_OK  0x00000071U
#  define AEV_IOMMU_CANARY   0x00000052U
#  define AEV_WDT_SWEEP_OK   0x00000061U
#  define AEV_WDT_SWEEP_FAIL 0x00000062U
#  define AEV_WDT_DEADMAN    0x00000063U
#  define AEV_HW_HOTPLUG_ADD 0x00000022U
#  define AEV_HW_HOTPLUG_REM 0x00000023U
#  define AUDIT_FLAG_NONE     0x0000U
#  define AUDIT_FLAG_CRITICAL 0x0001U

#define FLATFS_AUDIT_DRV_ID  0xFF464C54U

static tykid_gate_ctx_t *g_tykid_ctx;
static int               g_tykid_bound;

flatfs_err_t flatfs_tykid_bind(tykid_gate_ctx_t *ctx)
{
    if (!ctx) return FLATFS_OK;

#ifdef FLATFS_TYKID_AVAILABLE
    tykid_status_t st = tykid_verify_seal(ctx);
    if (st != TYKID_OK) {

        return FLATFS_ERR_PERM;
    }
    g_tykid_ctx   = ctx;
    g_tykid_bound = 1;

    uint8_t detail[16];
    FMEMCPY(detail, "flatfs\0\0\0\0\0\0\0\0\0\0", 16);
    ty_audit_append(ctx, AEV_INIT_OK, FLATFS_AUDIT_DRV_ID,
                    AUDIT_FLAG_NONE, detail, 7);
    return FLATFS_OK;
#else
    (void)ctx;
    return FLATFS_OK;
#endif
}

void flatfs_tykid_unbind(void)
{
#ifdef FLATFS_TYKID_AVAILABLE
    if (!g_tykid_bound) return;
    ty_audit_append(g_tykid_ctx, AEV_SHUTDOWN, FLATFS_AUDIT_DRV_ID,
                    AUDIT_FLAG_NONE, NULL, 0);

    g_tykid_ctx   = NULL;
    g_tykid_bound = 0;
#endif
}

flatfs_err_t flatfs_tykid_seal_check(void)
{
#ifdef FLATFS_TYKID_AVAILABLE
    if (!g_tykid_bound) return FLATFS_OK;

    tykid_status_t st = tykid_verify_seal(g_tykid_ctx);
    if (st != TYKID_OK) {
        ty_audit_append(g_tykid_ctx, AEV_SEAL_BROKEN, FLATFS_AUDIT_DRV_ID,
                        AUDIT_FLAG_CRITICAL, NULL, 0);
        return FLATFS_ERR_PERM;
    }
#endif
    return FLATFS_OK;
}

uint64_t flatfs_tykid_entropy(void)
{
#ifdef FLATFS_TYKID_AVAILABLE
    if (g_tykid_bound)
        return (uint64_t)ty_entropy_u64(g_tykid_ctx);
#endif
    return 0;
}

void flatfs_tykid_mac_set(void *blk, size_t sz)
{
#ifdef FLATFS_TYKID_AVAILABLE
    if (!g_tykid_bound || sz < 12) return;

    uint8_t *b = (uint8_t *)blk;

    uint64_t mac = (uint64_t)ty_siphash24(
                       tykid_ctx_session_key(g_tykid_ctx),
                       b, sz - 12);

    b[sz-12] = (uint8_t)(mac);
    b[sz-11] = (uint8_t)(mac >> 8);
    b[sz-10] = (uint8_t)(mac >> 16);
    b[sz- 9] = (uint8_t)(mac >> 24);
    b[sz- 8] = (uint8_t)(mac >> 32);
    b[sz- 7] = (uint8_t)(mac >> 40);
    b[sz- 6] = (uint8_t)(mac >> 48);
    b[sz- 5] = (uint8_t)(mac >> 56);
#else
    (void)blk; (void)sz;
#endif
}

int flatfs_tykid_mac_ok(const void *blk, size_t sz)
{
#ifdef FLATFS_TYKID_AVAILABLE
    if (!g_tykid_bound || sz < 12) return 1;

    const uint8_t *b = (const uint8_t *)blk;

    uint8_t mac_zero = 0;
    for (int _i = 0; _i < 8; _i++) mac_zero |= b[sz - 12 + _i];
    if (!mac_zero) return 1;

    uint64_t stored = (uint64_t)b[sz-12]
                    | ((uint64_t)b[sz-11] << 8)
                    | ((uint64_t)b[sz-10] << 16)
                    | ((uint64_t)b[sz- 9] << 24)
                    | ((uint64_t)b[sz- 8] << 32)
                    | ((uint64_t)b[sz- 7] << 40)
                    | ((uint64_t)b[sz- 6] << 48)
                    | ((uint64_t)b[sz- 5] << 56);

    uint64_t expected = (uint64_t)ty_siphash24(
                            tykid_ctx_session_key(g_tykid_ctx),
                            b, sz - 12);

    if (stored != expected) {
        flatfs_tykid_audit_crc_err(0);
        return 0;
    }
#else
    (void)blk; (void)sz;
#endif
    return 1;
}

void flatfs_tykid_audit_mount(int ok)
{
#ifdef FLATFS_TYKID_AVAILABLE
    if (!g_tykid_bound) return;
    ty_audit_append(g_tykid_ctx,
                    ok ? AEV_INIT_OK : AEV_INIT_FAIL,
                    FLATFS_AUDIT_DRV_ID,
                    ok ? AUDIT_FLAG_NONE : AUDIT_FLAG_CRITICAL,
                    NULL, 0);
#else
    (void)ok;
#endif
}

void flatfs_tykid_audit_unmount(void)
{
#ifdef FLATFS_TYKID_AVAILABLE
    if (!g_tykid_bound) return;
    ty_audit_append(g_tykid_ctx, AEV_SHUTDOWN, FLATFS_AUDIT_DRV_ID,
                    AUDIT_FLAG_NONE, NULL, 0);
#endif
}

void flatfs_tykid_audit_journal(int committed)
{
#ifdef FLATFS_TYKID_AVAILABLE
    if (!g_tykid_bound) return;

    ty_audit_append(g_tykid_ctx,
                    committed ? AEV_WDT_SWEEP_OK : AEV_WDT_SWEEP_FAIL,
                    FLATFS_AUDIT_DRV_ID,
                    committed ? AUDIT_FLAG_NONE : AUDIT_FLAG_CRITICAL,
                    NULL, 0);
#else
    (void)committed;
#endif
}

void flatfs_tykid_audit_crc_err(uint64_t blk)
{
#ifdef FLATFS_TYKID_AVAILABLE
    if (!g_tykid_bound) return;
    uint8_t detail[8];
    detail[0] = (uint8_t)(blk);
    detail[1] = (uint8_t)(blk >> 8);
    detail[2] = (uint8_t)(blk >> 16);
    detail[3] = (uint8_t)(blk >> 24);
    detail[4] = (uint8_t)(blk >> 32);
    detail[5] = (uint8_t)(blk >> 40);
    detail[6] = (uint8_t)(blk >> 48);
    detail[7] = (uint8_t)(blk >> 56);

    ty_audit_append(g_tykid_ctx, AEV_IOMMU_CANARY, FLATFS_AUDIT_DRV_ID,
                    AUDIT_FLAG_CRITICAL, detail, 8);
#else
    (void)blk;
#endif
}

void flatfs_tykid_audit_super_corrupt(void)
{
#ifdef FLATFS_TYKID_AVAILABLE
    if (!g_tykid_bound) return;
    ty_audit_append(g_tykid_ctx, AEV_SEAL_BROKEN, FLATFS_AUDIT_DRV_ID,
                    AUDIT_FLAG_CRITICAL, NULL, 0);
#endif
}

void flatfs_tykid_audit_readonly(void)
{
#ifdef FLATFS_TYKID_AVAILABLE
    if (!g_tykid_bound) return;

    ty_audit_append(g_tykid_ctx, AEV_WDT_DEADMAN, FLATFS_AUDIT_DRV_ID,
                    AUDIT_FLAG_CRITICAL, NULL, 0);
#endif
}

void flatfs_tykid_audit_handoff(int yielding)
{
#ifdef FLATFS_TYKID_AVAILABLE
    if (!g_tykid_bound) return;

    ty_audit_append(g_tykid_ctx,
                    yielding ? AEV_HW_HOTPLUG_REM : AEV_HW_HOTPLUG_ADD,
                    FLATFS_AUDIT_DRV_ID,
                    AUDIT_FLAG_NONE, NULL, 0);
#else
    (void)yielding;
#endif
}
