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

typedef uptr ty_iommu_domain_t;

#define TY_IOMMU_DOMAIN_INVALID   ((ty_iommu_domain_t)0)
#define TY_IOMMU_DOMAIN_BLOCKED ((ty_iommu_domain_t)1)

#define TY_IOMMU_PERM_NONE    0x00U
#define TY_IOMMU_PERM_READ    0x01U
#define TY_IOMMU_PERM_WRITE   0x02U
#define TY_IOMMU_PERM_RW      (TY_IOMMU_PERM_READ | TY_IOMMU_PERM_WRITE)

#define TY_IOMMU_MAX_REGIONS 8U

typedef struct {
    u64  phys_base;
    u64  length;
    u8   perm;
} ty_iommu_region_t;

typedef struct {
 u32 bdf;
    ty_iommu_domain_t domain;
    ty_iommu_region_t regions[TY_IOMMU_MAX_REGIONS];
    u8               region_count;
    bool8            attached;
    bool8            blocked;
 u64 canary;
} ty_iommu_binding_t;

#define TY_RADIX_FANOUT   16U
#define TY_RADIX_LEAF_SZ   8U

typedef struct ty_radix_node ty_radix_node_t;
struct ty_radix_node {
    union {
 ty_radix_node_t *children[TY_RADIX_FANOUT];
 ty_iommu_binding_t *leaves[TY_RADIX_LEAF_SZ];
    };
    bool8 is_leaf;
    u8    used;
    u8    _pad[6];
};

typedef struct {
    ty_radix_node_t *root;
    u32              count;
 u64 canary_key;
} ty_iommu_map_t;

extern tykid_status_t kobalt_iommu_create_domain(ty_iommu_domain_t *out);
extern tykid_status_t kobalt_iommu_destroy_domain(ty_iommu_domain_t dom);
extern tykid_status_t kobalt_iommu_attach_device(ty_iommu_domain_t dom,
                                                   u8 bus, u8 slot, u8 func);
extern tykid_status_t kobalt_iommu_detach_device(ty_iommu_domain_t dom,
                                                   u8 bus, u8 slot, u8 func);
extern tykid_status_t kobalt_iommu_map_region(ty_iommu_domain_t dom,
                                               u64 phys, u64 len, u8 perm);
extern tykid_status_t kobalt_iommu_unmap_region(ty_iommu_domain_t dom,
                                                  u64 phys, u64 len);
extern tykid_status_t kobalt_iommu_flush(ty_iommu_domain_t dom);
extern tykid_status_t kobalt_iommu_block_device(u8 bus, u8 slot, u8 func);
extern tykid_status_t kobalt_iommu_probe_bars(u8 bus, u8 slot, u8 func,
                                               ty_iommu_region_t *regions,
                                               u8 *count_out);
extern bool8          kobalt_iommu_present(void);

static TYKID_ALWAYS_INL u64
ty_iommu_make_canary(const tykid_gate_ctx_t *ctx, u32 bdf)
{
    u8 buf[12];
    ty_put_u32le(buf + 0, bdf);
    ty_put_u64le(buf + 4, __TYKID_KOBALT_EXPECTED);
    return ty_siphash24(ctx->session_key, buf, 12);
}

static TYKID_ALWAYS_INL bool8
ty_iommu_canary_ok(const tykid_gate_ctx_t *ctx,
                   const ty_iommu_binding_t *b)
{
    return ty_memeq(&b->canary,
                    &(u64){ ty_iommu_make_canary(ctx, b->bdf) },
                    8);
}

static ty_radix_node_t *
ty_radix_alloc_node(tykid_gate_ctx_t *ctx)
{
    ty_radix_node_t *n = (ty_radix_node_t *)TY_ALLOC(ctx, sizeof(ty_radix_node_t));
    if (!n) return NULL;
    ty_memzero_secure(n, sizeof(*n));
    return n;
}

static tykid_status_t
ty_radix_insert(tykid_gate_ctx_t *ctx, ty_iommu_map_t *map,
                u32 bdf, ty_iommu_binding_t *binding)
{
    if (!map->root) {
        map->root = ty_radix_alloc_node(ctx);
        if (!map->root) return TYKID_ERR_ALLOC;
    }

    u8 l0 = (bdf >> 20) & 0xFU;
    u8 l1 = (bdf >> 16) & 0xFU;
    u8 l2 = (bdf >> 12) & 0xFU;
 u8 leaf_idx = bdf & 0x7U;

    ty_radix_node_t *n = map->root;

    if (!n->children[l0]) {
        n->children[l0] = ty_radix_alloc_node(ctx);
        if (!n->children[l0]) return TYKID_ERR_ALLOC;
    }
    n = n->children[l0];

    if (!n->children[l1]) {
        n->children[l1] = ty_radix_alloc_node(ctx);
        if (!n->children[l1]) return TYKID_ERR_ALLOC;
        n->children[l1]->is_leaf = TYKID_TRUE;
    }
    n = n->children[l1];

    if (!n->children[l2]) {
        n->children[l2] = ty_radix_alloc_node(ctx);
        if (!n->children[l2]) return TYKID_ERR_ALLOC;
        n->children[l2]->is_leaf = TYKID_TRUE;
    }
    n = n->children[l2];

    if (n->leaves[leaf_idx]) return TYKID_ERR_ALREADY_LOADED;
    n->leaves[leaf_idx] = binding;
    n->used++;
    map->count++;
    return TYKID_OK;
}

static ty_iommu_binding_t *
ty_radix_lookup(const ty_iommu_map_t *map, u32 bdf)
{
    if (!map->root) return NULL;
    u8 l0 = (bdf >> 20) & 0xFU;
    u8 l1 = (bdf >> 16) & 0xFU;
    u8 l2 = (bdf >> 12) & 0xFU;
    u8 leaf_idx = bdf & 0x7U;

    ty_radix_node_t *n = map->root;
    if (!n->children[l0])                    return NULL;
    n = n->children[l0];
    if (!n->children[l1])                    return NULL;
    n = n->children[l1];
    if (!n->children[l2])                    return NULL;
    return n->children[l2]->leaves[leaf_idx];
}

typedef struct {
    ty_iommu_map_t  map;
 bool8 present;
 bool8 enforcing;
    u32             domain_count;
 u64 gate_ctx_phys;
} ty_iommu_state_t;

extern u64 kobalt_virt_to_phys(uptr vaddr);

static void
ty_iommu_protect_gate_ctx(tykid_gate_ctx_t *ctx, ty_iommu_state_t *is)
{

    is->gate_ctx_phys = kobalt_virt_to_phys((uptr)ctx);

    extern tykid_status_t kobalt_iommu_register_exclusion(u64 phys, u64 len);
    kobalt_iommu_register_exclusion(is->gate_ctx_phys, 4096);

    TY_LOG(ctx, TY_LOG_INFO,
           "IOMMU: gate context excluded at phys=0x%016llx",
           (unsigned long long)is->gate_ctx_phys);
}

static tykid_status_t
ty_iommu_bind_device(tykid_gate_ctx_t *ctx,
                      ty_iommu_state_t *is,
                      const tykid_hw_device_t *dev)
{
    u32 bdf = ((u32)dev->bus << 16) | ((u32)dev->slot << 8) | dev->func;

    if (ty_radix_lookup(&is->map, bdf)) return TYKID_ERR_ALREADY_LOADED;

    ty_iommu_binding_t *b = (ty_iommu_binding_t *)
                             TY_ALLOC(ctx, sizeof(ty_iommu_binding_t));
    if (!b) return TYKID_ERR_ALLOC;
    ty_memzero_secure(b, sizeof(*b));

    b->bdf    = bdf;
    b->canary = ty_iommu_make_canary(ctx, bdf);

    tykid_status_t st = kobalt_iommu_create_domain(&b->domain);
    if (st != TYKID_OK) {
        TY_LOG(ctx, TY_LOG_ERROR,
               "IOMMU: failed to create domain for %02x:%02x.%x: %d",
               dev->bus, dev->slot, dev->func, st);
        TY_FREE(ctx, b, sizeof(*b));
        return st;
    }

    st = kobalt_iommu_probe_bars(dev->bus, dev->slot, dev->func,
                                  b->regions, &b->region_count);
    if (st != TYKID_OK) {
        TY_LOG(ctx, TY_LOG_WARN,
               "IOMMU: BAR probe failed for %02x:%02x.%x — using MMIO from PCI scan",
               dev->bus, dev->slot, dev->func);

        if (dev->mmio_base && dev->mmio_size) {
            b->regions[0].phys_base = dev->mmio_base;
            b->regions[0].length    = dev->mmio_size;
            b->regions[0].perm      = TY_IOMMU_PERM_RW;
            b->region_count         = 1;
        }
    }

    for (u8 i = 0; i < b->region_count; i++) {
        st = kobalt_iommu_map_region(b->domain,
                                      b->regions[i].phys_base,
                                      b->regions[i].length,
                                      b->regions[i].perm);
        if (st != TYKID_OK) {
            TY_LOG(ctx, TY_LOG_WARN,
                   "IOMMU: region %u map failed for %02x:%02x.%x",
                   i, dev->bus, dev->slot, dev->func);
        }
    }

    kobalt_iommu_unmap_region(b->domain, is->gate_ctx_phys, 4096);

    st = kobalt_iommu_attach_device(b->domain, dev->bus, dev->slot, dev->func);
    if (st != TYKID_OK) {
        TY_LOG(ctx, TY_LOG_ERROR,
               "IOMMU: attach failed for %02x:%02x.%x: %d",
               dev->bus, dev->slot, dev->func, st);
        kobalt_iommu_destroy_domain(b->domain);
        TY_FREE(ctx, b, sizeof(*b));
        return st;
    }

    kobalt_iommu_flush(b->domain);
    b->attached = TYKID_TRUE;

    st = ty_radix_insert(ctx, &is->map, bdf, b);
    if (st != TYKID_OK) {
        kobalt_iommu_detach_device(b->domain, dev->bus, dev->slot, dev->func);
        kobalt_iommu_destroy_domain(b->domain);
        TY_FREE(ctx, b, sizeof(*b));
        return st;
    }

    is->domain_count++;
    TY_LOG(ctx, TY_LOG_DEBUG,
           "IOMMU: %02x:%02x.%x -> domain %016llx (%u BAR regions)",
           dev->bus, dev->slot, dev->func,
           (unsigned long long)b->domain, b->region_count);
    return TYKID_OK;
}

static void
ty_iommu_block_unbound(tykid_gate_ctx_t *ctx,
                         ty_iommu_state_t *is,
                         u8 bus, u8 slot, u8 func)
{
    u32 bdf = ((u32)bus << 16) | ((u32)slot << 8) | func;
 if (ty_radix_lookup(&is->map, bdf)) return;

    TY_LOG(ctx, TY_LOG_INFO,
           "IOMMU: blocking unbound device %02x:%02x.%x",
           bus, slot, func);
    kobalt_iommu_block_device(bus, slot, func);
}

TYKID_INTERNAL tykid_status_t
tykid_iommu_bind_all(tykid_gate_ctx_t *ctx, const tykid_hw_enumset_t *hw)
{
    if (!ctx || !hw) return TYKID_ERR_GENERIC;

    ty_iommu_state_t is;
    ty_memzero_secure(&is, sizeof(is));

    if (!kobalt_iommu_present()) {
        TY_LOG(ctx, TY_LOG_WARN,
               "IOMMU not present — DMA isolation unavailable (degraded mode)");
        return TYKID_OK;
    }
    is.present = TYKID_TRUE;

    ty_iommu_protect_gate_ctx(ctx, &is);

    u32 bound = 0, failed = 0;
    for (u32 i = 0; i < hw->count; i++) {
        tykid_status_t st = ty_iommu_bind_device(ctx, &is, &hw->devices[i]);
        if (st == TYKID_OK) bound++;
        else                 failed++;
    }

    is.enforcing = TYKID_TRUE;

    TY_LOG(ctx, TY_LOG_INFO,
           "IOMMU binding complete: %u bound, %u failed, %u domains active",
           bound, failed, is.domain_count);

    ty_memzero_secure(&is, sizeof(is));
    return (failed == 0) ? TYKID_OK : TYKID_ERR_GENERIC;
}

TYKID_INTERNAL tykid_status_t
tykid_iommu_release_device(tykid_gate_ctx_t *ctx,
                             ty_iommu_state_t *is,
                             u8 bus, u8 slot, u8 func)
{
    u32 bdf = ((u32)bus << 16) | ((u32)slot << 8) | func;
    ty_iommu_binding_t *b = ty_radix_lookup(&is->map, bdf);
    if (!b) return TYKID_ERR_NO_DRIVER;

    if (!ty_iommu_canary_ok(ctx, b)) {

        TY_LOG(ctx, TY_LOG_FATAL,
               "IOMMU: binding canary CORRUPTED for %02x:%02x.%x — "
               "revoking domain immediately (NOT panicking)",
               bus, slot, func);

        kobalt_iommu_block_device(bus, slot, func);

        ty_memzero_secure(b, sizeof(*b));
        return TYKID_ERR_DRIVER_CORRUPT;
    }

    kobalt_iommu_detach_device(b->domain, bus, slot, func);
    kobalt_iommu_destroy_domain(b->domain);
 kobalt_iommu_block_device(bus, slot, func);

    ty_memzero_secure(b, sizeof(*b));
    TY_FREE(ctx, b, sizeof(*b));
    return TYKID_OK;
}
