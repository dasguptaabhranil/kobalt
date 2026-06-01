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

typedef u16 ty_cap_t;
#define TY_CAP_NONE        ((ty_cap_t)0x0000U)
#define TY_CAP_OBSERVE     ((ty_cap_t)0x0001U)
#define TY_CAP_CONTROL     ((ty_cap_t)0x0002U)
#define TY_CAP_INTERRUPT   ((ty_cap_t)0x0004U)
#define TY_CAP_DMA_OWN     ((ty_cap_t)0x0008U)
#define TY_CAP_POWER       ((ty_cap_t)0x0010U)
#define TY_CAP_FIRMWARE    ((ty_cap_t)0x0020U)
#define TY_CAP_PRIVILEGED  ((ty_cap_t)0x0040U)
#define TY_CAP_ALL         ((ty_cap_t)0x007FU)

static const ty_cap_t TY_CAP_CLOSURE[8] = {
  TY_CAP_NONE,
  TY_CAP_OBSERVE,
  TY_CAP_OBSERVE | TY_CAP_CONTROL,
  TY_CAP_OBSERVE | TY_CAP_CONTROL | TY_CAP_INTERRUPT,
  TY_CAP_OBSERVE | TY_CAP_CONTROL | TY_CAP_DMA_OWN,
  TY_CAP_OBSERVE | TY_CAP_CONTROL | TY_CAP_INTERRUPT
                   | TY_CAP_DMA_OWN | TY_CAP_POWER,
  TY_CAP_OBSERVE | TY_CAP_CONTROL | TY_CAP_INTERRUPT
 | TY_CAP_DMA_OWN | TY_CAP_POWER | TY_CAP_FIRMWARE,
  TY_CAP_ALL,
};

typedef u8 ty_allow_kind_t;
#define TY_ALLOW_MMIO 0x01U
#define TY_ALLOW_IRQ 0x02U
#define TY_ALLOW_DMA 0x03U
#define TY_ALLOW_SYMBOL 0x04U

#define TY_POLICY_MAX_ENTRIES   64U

typedef struct TYKID_PACKED {
    ty_allow_kind_t kind;
    u8              _pad[3];
 u64 base;
 u64 limit;
} ty_allow_entry_t;

#define TY_BLOOM_SLOTS 8U
typedef struct {
    u64 hashes[TY_BLOOM_SLOTS];
 u8 cursor;
} ty_policy_bloom_t;

typedef struct TYKID_ALIGNED(64) {
 u32 driver_id;
 ty_cap_t granted_caps;
 ty_cap_t declared_caps;
    u32                entry_count;
    ty_allow_entry_t   entries[TY_POLICY_MAX_ENTRIES];
    ty_policy_bloom_t  bloom;
    u64                violation_count;
    u64                check_count;
 bool8 sealed;
 u64 seal_hmac;
} ty_driver_policy_t;

typedef struct {
    ty_driver_policy_t records[TYKID_MAX_DRIVERS];
    u32                count;
} ty_policy_table_t;

static TYKID_SECTION(".tykid.policy") ty_policy_table_t g_policy_table;

static TYKID_ALWAYS_INL u64
ty_bloom_hash(const tykid_gate_ctx_t *ctx, u32 drv_id, u64 addr, u8 kind)
{
    u8 buf[13];
    ty_put_u32le(buf + 0, drv_id);
    ty_put_u64le(buf + 4, addr);
    buf[12] = kind;
    return ty_siphash24(ctx->session_key, buf, 13);
}

static TYKID_ALWAYS_INL bool8
ty_bloom_check(const ty_policy_bloom_t *b, u64 h)
{
    for (u8 i = 0; i < TY_BLOOM_SLOTS; i++) {
        if (b->hashes[i] == h) return TYKID_TRUE;
    }
    return TYKID_FALSE;
}

static TYKID_ALWAYS_INL void
ty_bloom_insert(ty_policy_bloom_t *b, u64 h)
{
    b->hashes[b->cursor & (TY_BLOOM_SLOTS - 1)] = h;
    b->cursor++;
}

static void
ty_allowlist_sort(ty_driver_policy_t *pol)
{
    for (u32 i = 1; i < pol->entry_count; i++) {
        ty_allow_entry_t key = pol->entries[i];
        s32 j = (s32)i - 1;
        while (j >= 0) {
            ty_allow_entry_t *e = &pol->entries[j];
            bool8 greater = (e->kind > key.kind)
                         || (e->kind == key.kind && e->base > key.base);
            if (!greater) break;
            pol->entries[j + 1] = *e;
            j--;
        }
        pol->entries[j + 1] = key;
    }
}

static bool8
ty_allowlist_contains(const ty_driver_policy_t *pol,
                       ty_allow_kind_t kind, u64 addr)
{

    u32 lo = 0, hi = pol->entry_count;
    while (lo < hi) {
        u32 mid = lo + (hi - lo) / 2;
        const ty_allow_entry_t *e = &pol->entries[mid];
        if (e->kind < kind || (e->kind == kind && e->limit < addr)) {
            lo = mid + 1;
        } else if (e->kind > kind || (e->kind == kind && e->base > addr)) {
            hi = mid;
        } else {

            return (e->kind == kind && addr >= e->base && addr <= e->limit)
                   ? TYKID_TRUE : TYKID_FALSE;
        }
    }
    return TYKID_FALSE;
}

static ty_cap_t
ty_default_caps_for_class(tykid_hwclass_t cls)
{
    switch (cls & 0xFFFF0000U) {
 case 0x00010000U:
        return TY_CAP_OBSERVE | TY_CAP_CONTROL | TY_CAP_DMA_OWN | TY_CAP_FIRMWARE;
 case 0x00020000U:
        return TY_CAP_OBSERVE | TY_CAP_CONTROL | TY_CAP_INTERRUPT | TY_CAP_DMA_OWN;
 case 0x00030000U:
        return TY_CAP_OBSERVE | TY_CAP_CONTROL | TY_CAP_INTERRUPT | TY_CAP_DMA_OWN;
 case 0x00040000U:
        return TY_CAP_OBSERVE | TY_CAP_CONTROL | TY_CAP_INTERRUPT
             | TY_CAP_DMA_OWN | TY_CAP_POWER;
 case 0x00050000U:
        return TY_CAP_OBSERVE | TY_CAP_CONTROL | TY_CAP_INTERRUPT | TY_CAP_DMA_OWN;
 case 0x00060000U:
        return TY_CAP_OBSERVE | TY_CAP_INTERRUPT;
 case 0x00070000U:
        return TY_CAP_OBSERVE | TY_CAP_CONTROL;
 case 0x00080000U:
        return TY_CAP_OBSERVE | TY_CAP_POWER;
    default:
 return TY_CAP_OBSERVE;
    }
}

static tykid_status_t
ty_policy_seal(tykid_gate_ctx_t *ctx, ty_driver_policy_t *pol)
{
    if (pol->sealed) return TYKID_ERR_ALREADY_LOADED;

    u8 highest_bit = 0;
    for (u8 i = 7; i > 0; i--) {
        if (pol->declared_caps & (1u << (i-1))) { highest_bit = i; break; }
    }
    ty_cap_t allowed_closure = highest_bit < 8
                              ? TY_CAP_CLOSURE[highest_bit] : TY_CAP_NONE;
    pol->granted_caps &= allowed_closure;

    ty_allowlist_sort(pol);

    pol->seal_hmac = ty_siphash24(ctx->session_key,
                                   pol->entries,
                                   pol->entry_count * sizeof(ty_allow_entry_t));
    pol->sealed = TYKID_TRUE;

    TY_LOG(ctx, TY_LOG_DEBUG,
           "Policy sealed for driver %u: caps=0x%04x entries=%u",
           pol->driver_id, pol->granted_caps, pol->entry_count);
    return TYKID_OK;
}

TYKID_API TYKID_HOT tykid_status_t
tykid_policy_check(tykid_gate_ctx_t *ctx,
                    u32              driver_id,
                    ty_allow_kind_t  kind,
                    u64              addr,
                    ty_cap_t         required_cap)
{
    if (__ty_unlikely(driver_id >= g_policy_table.count))
        return TYKID_ERR_PERM;

    ty_driver_policy_t *pol = &g_policy_table.records[driver_id];

    pol->check_count++;

    if (__ty_unlikely(!(pol->granted_caps & required_cap))) {
        pol->violation_count++;
        TY_LOG(ctx, TY_LOG_WARN,
               "POLICY: driver %u lacks cap 0x%04x (has 0x%04x)",
               driver_id, required_cap, pol->granted_caps);
        return TYKID_ERR_PERM;
    }

    u64 bh = ty_bloom_hash(ctx, driver_id, addr, kind);
    if (__ty_likely(ty_bloom_check(&pol->bloom, bh)))
        return TYKID_OK;

    if (!ty_allowlist_contains(pol, kind, addr)) {
        pol->violation_count++;
        TY_LOG(ctx, TY_LOG_WARN,
               "POLICY: driver %u denied kind=0x%02x addr=0x%016llx",
               driver_id, kind, (unsigned long long)addr);
        return TYKID_ERR_PERM;
    }

    ty_bloom_insert(&pol->bloom, bh);
    return TYKID_OK;
}

TYKID_INTERNAL tykid_status_t
tykid_policy_init_driver(tykid_gate_ctx_t *ctx,
                          u32 driver_id,
                          const tykid_driver_desc_t *drv,
                          const tykid_hw_device_t *dev)
{
    if (driver_id >= TYKID_MAX_DRIVERS) return TYKID_ERR_GENERIC;

    ty_driver_policy_t *pol = &g_policy_table.records[driver_id];
    ty_memzero_secure(pol, sizeof(*pol));

    pol->driver_id     = driver_id;
 pol->declared_caps = (ty_cap_t)(drv->flags >> 16);
    pol->granted_caps  = ty_default_caps_for_class(dev->ty_class);

    if (pol->declared_caps)
        pol->granted_caps &= pol->declared_caps;

    if (dev->mmio_base && dev->mmio_size && pol->entry_count < TY_POLICY_MAX_ENTRIES) {
        ty_allow_entry_t *e = &pol->entries[pol->entry_count++];
        e->kind  = TY_ALLOW_MMIO;
        e->base  = dev->mmio_base;
        e->limit = dev->mmio_base + dev->mmio_size - 1;
    }

    if (dev->irq && pol->entry_count < TY_POLICY_MAX_ENTRIES) {
        ty_allow_entry_t *e = &pol->entries[pol->entry_count++];
        e->kind  = TY_ALLOW_IRQ;
        e->base  = dev->irq;
        e->limit = dev->irq;
    }

    if (dev->mmio_base && dev->mmio_size
     && (pol->granted_caps & TY_CAP_DMA_OWN)
     && pol->entry_count < TY_POLICY_MAX_ENTRIES) {
        ty_allow_entry_t *e = &pol->entries[pol->entry_count++];
        e->kind  = TY_ALLOW_DMA;
        e->base  = dev->mmio_base;
        e->limit = dev->mmio_base + dev->mmio_size - 1;
    }

    tykid_status_t st = ty_policy_seal(ctx, pol);
    if (st == TYKID_OK) g_policy_table.count++;
    return st;
}

TYKID_INTERNAL tykid_status_t
tykid_policy_recheck(tykid_gate_ctx_t *ctx)
{
    u32 violations = 0;
    for (u32 i = 0; i < g_policy_table.count; i++) {
        ty_driver_policy_t *pol = &g_policy_table.records[i];
        if (!pol->sealed) continue;

        u64 current_hmac = ty_siphash24(ctx->session_key,
                                         pol->entries,
                                         pol->entry_count * sizeof(ty_allow_entry_t));
        if (!ty_memeq(&current_hmac, &pol->seal_hmac, 8)) {
            TY_LOG(ctx, TY_LOG_ERROR,
                   "POLICY: allowlist TAMPERED for driver %u", pol->driver_id);

            pol->granted_caps = TY_CAP_NONE;
            violations++;
        }

        if (pol->violation_count > 0) {
            TY_LOG(ctx, TY_LOG_WARN,
                   "POLICY: driver %u has %llu violations (total checks: %llu)",
                   pol->driver_id,
                   (unsigned long long)pol->violation_count,
                   (unsigned long long)pol->check_count);
        }
    }
    return violations == 0 ? TYKID_OK : TYKID_ERR_PERM;
}

TYKID_API u64
tykid_policy_violation_count(const tykid_gate_ctx_t *ctx, u32 driver_id)
{
    (void)ctx;
    if (driver_id >= g_policy_table.count) return (u64)-1;
    return g_policy_table.records[driver_id].violation_count;
}
