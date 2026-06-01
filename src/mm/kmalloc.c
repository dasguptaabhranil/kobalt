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

#include "../inc/kmalloc.h"
#include "../inc/numa.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdatomic.h>

#define HEAP_POOL_SIZE      (16U * 1024U * 1024U)
#define KMALLOC_NUM_CACHES  9U

#define OWNER_BUMP    0xFu
#define OWNER_INVALID 0xFFu

#define SLAB_OWNER_ENTRIES (HEAP_POOL_SIZE / KMALLOC_MIN_SIZE)

static uint8_t heap_pool[HEAP_POOL_SIZE]      __attribute__((aligned(4096)));
static uint8_t slab_owner[SLAB_OWNER_ENTRIES];

typedef struct {
    size_t   obj_size;
    void    *freelist;
    size_t   total;
    size_t   free_count;
    uint8_t *cbase;
} kmem_cache_t;

static const size_t cache_sizes[KMALLOC_NUM_CACHES] = {
    16, 32, 64, 128, 256, 512, 1024, 2048, 4096
};

static const size_t base_quota[KMALLOC_NUM_CACHES] = {
    1024, 512, 512, 256, 256, 128, 64, 64, 32
};

#define BASE_SLAB_TOTAL 557056UL

static kmem_cache_t caches[NUMA_MAX_NODES][KMALLOC_NUM_CACHES];
static uint8_t     *bump_ptr[NUMA_MAX_NODES];
static uint8_t     *bump_end[NUMA_MAX_NODES];
static atomic_flag  node_lock[NUMA_MAX_NODES];

static uint8_t  *node_base[NUMA_MAX_NODES];
static size_t    node_size[NUMA_MAX_NODES];
static uint32_t  active_nodes;

static inline uint64_t irq_save(void)
{
    uint64_t f;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(f) :: "memory");
    return f;
}

static inline void irq_restore(uint64_t f)
{
    __asm__ volatile("pushq %0; popfq" :: "r"(f) : "memory", "cc");
}

static inline void nlock(uint32_t n)
{
    while (atomic_flag_test_and_set_explicit(&node_lock[n], memory_order_acquire))
        __asm__ volatile("pause");
}

static inline void nunlock(uint32_t n)
{
    atomic_flag_clear_explicit(&node_lock[n], memory_order_release);
}

static unsigned int cidx_for(size_t sz)
{
    for (unsigned int i = 0; i < KMALLOC_NUM_CACHES; i++)
        if (sz <= cache_sizes[i])
            return i;
    return KMALLOC_NUM_CACHES;
}

static size_t node_quota(size_t ci, size_t pool_sz)
{
    size_t target = pool_sz * 3 / 4;
    size_t q = base_quota[ci] * target / BASE_SLAB_TOTAL;
    if (q > base_quota[ci]) q = base_quota[ci];
    return q ? q : 1;
}

static void init_node(uint32_t nid, uint8_t *base, size_t psz)
{
    node_base[nid] = base;
    node_size[nid] = psz;

    size_t off = 0;
    for (unsigned int ci = 0; ci < KMALLOC_NUM_CACHES; ci++) {
        size_t obj_sz  = cache_sizes[ci];
        size_t nobj    = node_quota(ci, psz);
        size_t region  = obj_sz * nobj;

        caches[nid][ci].obj_size   = obj_sz;
        caches[nid][ci].total      = nobj;
        caches[nid][ci].free_count = nobj;
        caches[nid][ci].cbase      = base + off;

        void *next = NULL;
        for (size_t j = nobj; j-- > 0; ) {
            uint8_t *obj = base + off + j * obj_sz;
            *(void **)obj = next;
            next = obj;
        }
        caches[nid][ci].freelist = next;

        size_t first = (size_t)(base + off - heap_pool) / KMALLOC_MIN_SIZE;
        size_t ngran  = region / KMALLOC_MIN_SIZE;
        uint8_t tag   = (uint8_t)((nid << 4) | ci);
        for (size_t g = 0; g < ngran; g++)
            slab_owner[first + g] = tag;

        off += region;
    }

    size_t bstart = (size_t)(base + off - heap_pool) / KMALLOC_MIN_SIZE;
    size_t bend   = (size_t)(base + psz - heap_pool) / KMALLOC_MIN_SIZE;
    uint8_t btag  = (uint8_t)((nid << 4) | OWNER_BUMP);
    for (size_t g = bstart; g < bend; g++)
        slab_owner[g] = btag;

    bump_ptr[nid] = base + off;
    bump_end[nid] = base + psz;
}

void kmalloc_init(void)
{
    memset(slab_owner, OWNER_INVALID, sizeof(slab_owner));
    for (uint32_t i = 0; i < NUMA_MAX_NODES; i++)
        atomic_flag_clear(&node_lock[i]);

    active_nodes = numa_node_count();
    if (!active_nodes || active_nodes > NUMA_MAX_NODES)
        active_nodes = 1;

    size_t per_node = HEAP_POOL_SIZE / active_nodes;
    for (uint32_t n = 0; n < active_nodes; n++)
        init_node(n, heap_pool + n * per_node, per_node);
}

static inline int fl_valid(void *p)
{
    uintptr_t op = (uintptr_t)p;
    uintptr_t bp = (uintptr_t)heap_pool;
    return op >= bp && op < bp + HEAP_POOL_SIZE;
}

static void *alloc_from_node(unsigned int ci, uint32_t nid)
{
    kmem_cache_t *c = &caches[nid][ci];
    if (!c->freelist)
        return NULL;
    if (!fl_valid(c->freelist)) {
        c->freelist   = NULL;
        c->free_count = 0;
        return NULL;
    }
    void *obj    = c->freelist;
    c->freelist  = *(void **)obj;
    c->free_count--;
    return obj;
}

static void *bump_alloc(size_t sz, uint32_t nid)
{
    uintptr_t a = ((uintptr_t)bump_ptr[nid] + 63UL) & ~63UL;
    if ((uint8_t *)a + sz > bump_end[nid])
        return NULL;
    bump_ptr[nid] = (uint8_t *)a + sz;
    return (void *)a;
}

void *kmalloc_node(size_t sz, uint32_t node)
{
    if (!sz) return NULL;
    if (node >= active_nodes) node = 0;

    unsigned int ci = cidx_for(sz);

    if (ci < KMALLOC_NUM_CACHES) {
        uint64_t f = irq_save();
        nlock(node);
        void *p = alloc_from_node(ci, node);
        nunlock(node);
        if (p) { irq_restore(f); return p; }

        for (uint32_t n = 0; n < active_nodes; n++) {
            if (n == node) continue;
            nlock(n);
            p = alloc_from_node(ci, n);
            nunlock(n);
            if (p) { irq_restore(f); return p; }
        }
        irq_restore(f);

    }

    {
        uint64_t f = irq_save();
        nlock(node);
        void *p = bump_alloc(sz, node);
        nunlock(node);
        if (p) { irq_restore(f); return p; }

        for (uint32_t n = 0; n < active_nodes; n++) {
            if (n == node) continue;
            nlock(n);
            p = bump_alloc(sz, n);
            nunlock(n);
            if (p) { irq_restore(f); return p; }
        }
        irq_restore(f);
        return NULL;
    }
}

void *kmalloc(size_t sz)
{
    return kmalloc_node(sz, numa_current_node());
}

void kfree(void *ptr)
{
    if (!ptr) return;

    uintptr_t p    = (uintptr_t)ptr;
    uintptr_t base = (uintptr_t)heap_pool;
    if (p < base || p >= base + HEAP_POOL_SIZE) return;

    uint8_t  ow   = slab_owner[(p - base) / KMALLOC_MIN_SIZE];
    uint8_t  nid  = ow >> 4;
    uint8_t  ci   = ow & 0xF;

    if (ow == OWNER_INVALID || ci == OWNER_BUMP || nid >= active_nodes)
        return;

    kmem_cache_t *c = &caches[nid][ci];

    if ((p - (uintptr_t)c->cbase) % c->obj_size != 0)
        return;

    uint64_t f = irq_save();
    nlock(nid);
    if (c->free_count >= c->total) {
        nunlock(nid);
        irq_restore(f);
        return;
    }
    *(void **)ptr = c->freelist;
    c->freelist   = ptr;
    c->free_count++;
    nunlock(nid);
    irq_restore(f);
}

void kmalloc_stats_node(uint32_t node, unsigned int ci,
                        size_t *out_total, size_t *out_free)
{
    if (node >= active_nodes || ci >= KMALLOC_NUM_CACHES) {
        if (out_total) *out_total = 0;
        if (out_free)  *out_free  = 0;
        return;
    }
    const kmem_cache_t *c = &caches[node][ci];
    if (out_total) *out_total = c->total;
    if (out_free)  *out_free  = c->free_count;
}

void kmalloc_stats(unsigned int ci, size_t *out_total, size_t *out_free)
{
    kmalloc_stats_node(0, ci, out_total, out_free);
}
