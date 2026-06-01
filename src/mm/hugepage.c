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
#include <kmalloc.h>
#include <spinlock.h>
#include <hugepage.h>

#define HP_2M_POOL_BASE     0x30000000UL
#define HP_2M_POOL_COUNT    64
#define HP_1G_POOL_BASE     0x100000000UL
#define HP_1G_POOL_COUNT    4

#define PTE_PRESENT     (1ULL << 0)
#define PTE_RW          (1ULL << 1)
#define PTE_USER        (1ULL << 2)
#define PTE_PWT         (1ULL << 3)
#define PTE_PCD         (1ULL << 4)
#define PTE_PS          (1ULL << 7)
#define PTE_GLOBAL      (1ULL << 8)
#define PTE_NX          (1ULL << 63)
#define PTE_PHYS_MASK   0x000FFFFFFFFFF000ULL

#define PML4_IDX(v)     (((v) >> 39) & 0x1FF)
#define PDP_IDX(v)      (((v) >> 30) & 0x1FF)
#define PD_IDX(v)       (((v) >> 21) & 0x1FF)

#define BITMAP_WORDS(n) (((n) + 63) / 64)

static inline void cpuid_ex(uint32_t leaf, uint32_t sub,
                             uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    __asm__ volatile("cpuid"
        : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
        : "a"(leaf), "c"(sub));
}

static inline uint64_t read_cr3(void)
{
    uint64_t v;
    __asm__ volatile("mov %%cr3, %0" : "=r"(v));
    return v;
}

static inline void invlpg(uintptr_t va)
{
    __asm__ volatile("invlpg (%0)" :: "r"(va) : "memory");
}

static uint64_t g_2m_bitmap[BITMAP_WORDS(HP_2M_POOL_COUNT)];
static uint64_t g_1g_bitmap[BITMAP_WORDS(HP_1G_POOL_COUNT)];
static size_t   g_2m_free  = 0;
static size_t   g_1g_free  = 0;
static int      g_pdpe1gb  = 0;
static spinlock_t g_lock   = SPINLOCK_INIT;

static int detect_pdpe1gb(void)
{
    uint32_t a, b, c, d;
    cpuid_ex(0x80000001, 0, &a, &b, &c, &d);
    return (d >> 26) & 1;
}

static int bitmap_alloc(uint64_t *bm, size_t n, size_t *out)
{
    size_t words = BITMAP_WORDS(n);
    for (size_t w = 0; w < words; w++) {
        if (bm[w] == ~0ULL) continue;
        for (int b = 0; b < 64; b++) {
            size_t idx = w * 64 + (size_t)b;
            if (idx >= n) return -1;
            if (!(bm[w] & (1ULL << b))) {
                bm[w] |= (1ULL << b);
                *out = idx;
                return 0;
            }
        }
    }
    return -1;
}

static void bitmap_free(uint64_t *bm, size_t idx)
{
    bm[idx / 64] &= ~(1ULL << (idx % 64));
}

uintptr_t hugepage_alloc(hp_order_t order)
{
    uint64_t fl = spin_lock_irqsave(&g_lock);

    size_t idx;
    uintptr_t phys = 0;

    if (order == HP_ORDER_2M) {
        if (!g_2m_free || bitmap_alloc(g_2m_bitmap, HP_2M_POOL_COUNT, &idx) != 0)
            goto out;
        g_2m_free--;
        phys = HP_2M_POOL_BASE + idx * HP_2M_SIZE;
    } else {
        if (!g_pdpe1gb || !g_1g_free ||
            bitmap_alloc(g_1g_bitmap, HP_1G_POOL_COUNT, &idx) != 0)
            goto out;
        g_1g_free--;
        phys = HP_1G_POOL_BASE + idx * HP_1G_SIZE;
    }

out:
    spin_unlock_irqrestore(&g_lock, fl);
    return phys;
}

void hugepage_free(uintptr_t phys, hp_order_t order)
{
    uint64_t fl = spin_lock_irqsave(&g_lock);

    if (order == HP_ORDER_2M) {
        if (phys < HP_2M_POOL_BASE) goto out;
        size_t idx = (phys - HP_2M_POOL_BASE) / HP_2M_SIZE;
        if (idx >= HP_2M_POOL_COUNT) goto out;
        bitmap_free(g_2m_bitmap, idx);
        g_2m_free++;
    } else {
        if (!g_pdpe1gb || phys < HP_1G_POOL_BASE) goto out;
        size_t idx = (phys - HP_1G_POOL_BASE) / HP_1G_SIZE;
        if (idx >= HP_1G_POOL_COUNT) goto out;
        bitmap_free(g_1g_bitmap, idx);
        g_1g_free++;
    }

out:
    spin_unlock_irqrestore(&g_lock, fl);
}

static uint64_t hp_flags_to_pte(uint32_t fl)
{
    uint64_t pte = PTE_PRESENT | PTE_PS;

    if (fl & HP_FLAG_RW)     pte |= PTE_RW;
    if (fl & HP_FLAG_USER)   pte |= PTE_USER;
    if (fl & HP_FLAG_GLOBAL) pte |= PTE_GLOBAL;
    if (fl & HP_FLAG_NX)     pte |= PTE_NX;
    if (fl & HP_FLAG_WC) {
        pte |= PTE_PWT;
        pte &= ~PTE_PCD;
    }
    if (fl & HP_FLAG_UC)
        pte |= PTE_PWT | PTE_PCD;

    return pte;
}

static uint64_t *pt_virt(uintptr_t phys)
{
    return (uint64_t *)phys;
}

static uintptr_t ensure_table(uint64_t *parent, int idx)
{
    if (parent[idx] & PTE_PRESENT) {
        if (parent[idx] & PTE_PS)
            return 0;
        return (uintptr_t)(parent[idx] & PTE_PHYS_MASK);
    }
    void *p = kmalloc(4096);
    if (!p) return 0;
    uint64_t *tbl = (uint64_t *)p;
    for (int i = 0; i < 512; i++) tbl[i] = 0;
    uintptr_t phys = (uintptr_t)p;
    parent[idx] = phys | PTE_PRESENT | PTE_RW | PTE_USER;
    return phys;
}

int hugepage_map_cr3(uint64_t cr3, uintptr_t virt, uintptr_t phys,
                      hp_order_t order, uint32_t flags)
{
    virt &= (order == HP_ORDER_2M) ? ~HP_2M_MASK : ~HP_1G_MASK;

    uint64_t *pml4 = pt_virt((uintptr_t)(cr3 & PTE_PHYS_MASK));
    int pi = (int)PML4_IDX(virt);

    uintptr_t pdp_phys = ensure_table(pml4, pi);
    if (!pdp_phys) return -1;

    uint64_t *pdp = pt_virt(pdp_phys);
    int di = (int)PDP_IDX(virt);

    if (order == HP_ORDER_1G) {
        if (!g_pdpe1gb) return -1;
        pdp[di] = (phys & ~HP_1G_MASK) | hp_flags_to_pte(flags);
        invlpg(virt);
        return 0;
    }

    uintptr_t pd_phys = ensure_table(pdp, di);
    if (!pd_phys) return -1;

    uint64_t *pd = pt_virt(pd_phys);
    int xi = (int)PD_IDX(virt);

    pd[xi] = (phys & ~HP_2M_MASK) | hp_flags_to_pte(flags);
    invlpg(virt);
    return 0;
}

int hugepage_map(uintptr_t virt, uintptr_t phys, hp_order_t order, uint32_t flags)
{
    return hugepage_map_cr3(read_cr3(), virt, phys, order, flags);
}

int hugepage_unmap(uintptr_t virt, hp_order_t order)
{
    uint64_t cr3 = read_cr3();
    uint64_t *pml4 = pt_virt((uintptr_t)(cr3 & PTE_PHYS_MASK));
    int pi = (int)PML4_IDX(virt);

    if (!(pml4[pi] & PTE_PRESENT)) return -1;
    uint64_t *pdp = pt_virt((uintptr_t)(pml4[pi] & PTE_PHYS_MASK));
    int di = (int)PDP_IDX(virt);

    if (order == HP_ORDER_1G) {
        if (!(pdp[di] & PTE_PRESENT)) return -1;
        pdp[di] = 0;
        invlpg(virt);
        return 0;
    }

    if (!(pdp[di] & PTE_PRESENT)) return -1;
    uint64_t *pd = pt_virt((uintptr_t)(pdp[di] & PTE_PHYS_MASK));
    int xi = (int)PD_IDX(virt);

    if (!(pd[xi] & PTE_PRESENT)) return -1;
    pd[xi] = 0;
    invlpg(virt);
    return 0;
}

int hugepage_available(hp_order_t order)
{
    if (order == HP_ORDER_1G) return g_pdpe1gb;
    return 1;
}

size_t hugepage_free_count(hp_order_t order)
{
    return (order == HP_ORDER_2M) ? g_2m_free : g_1g_free;
}

size_t hugepage_total_count(hp_order_t order)
{
    return (order == HP_ORDER_2M) ? HP_2M_POOL_COUNT : HP_1G_POOL_COUNT;
}

int hugepage_pdpe1gb_supported(void)
{
    return g_pdpe1gb;
}

void hugepage_init(void)
{
    g_pdpe1gb = detect_pdpe1gb();

    for (size_t i = 0; i < BITMAP_WORDS(HP_2M_POOL_COUNT); i++)
        g_2m_bitmap[i] = 0;

    g_2m_free = HP_2M_POOL_COUNT;

    if (g_pdpe1gb) {
        for (size_t i = 0; i < BITMAP_WORDS(HP_1G_POOL_COUNT); i++)
            g_1g_bitmap[i] = 0;
        g_1g_free = HP_1G_POOL_COUNT;
    }

    char msg[96];
    ksnprintf(msg, sizeof(msg),
              "pool_2m=0x%x+%u*2M  1G=%s  PDPE1GB=%s",
              (unsigned)HP_2M_POOL_BASE, HP_2M_POOL_COUNT,
              g_pdpe1gb ? "ok" : "no",
              g_pdpe1gb ? "yes" : "no");
    klog_ok("hugepage", msg);
}
