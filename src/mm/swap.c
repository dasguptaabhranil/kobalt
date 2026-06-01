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
#include <blkdev.h>
#include <swap.h>
#include <mreclaim.h>

#define SLOTS_PER_SECTOR    (512 / SWAP_PAGE_SIZE)
#define SECTORS_PER_PAGE    (SWAP_PAGE_SIZE / 512)

#define SWAP_PTE_DEV_SHIFT  1
#define SWAP_PTE_DEV_MASK   0x7ULL
#define SWAP_PTE_SLOT_SHIFT 12

#define BITMAP_WORDS(n)     (((n) + 63) / 64)

static swap_dev_t   g_devs[SWAP_MAX_DEVS];
static int          g_ndevs     = 0;
static spinlock_t   g_lock      = SPINLOCK_INIT;
static int          g_active    = 0;

static size_t g_total_slots = 0;
static size_t g_free_slots  = 0;

static int blkdev_read_page(int bdev_idx, uint64_t lba, void *buf)
{
    return blkdev_read(blkdev_get(bdev_idx), lba, SECTORS_PER_PAGE, buf);
}

static int blkdev_write_page(int bdev_idx, uint64_t lba, const void *buf)
{
    return blkdev_write(blkdev_get(bdev_idx), lba, SECTORS_PER_PAGE, buf);
}

static swap_slot_t slot_alloc_dev(swap_dev_t *dev, int dev_idx)
{
    if (!dev->active || !dev->free_count || !dev->bitmap)
        return SWAP_SLOT_NONE;

    size_t words = BITMAP_WORDS(dev->nslots);
    size_t start = dev->scan_hint / 64;

    for (size_t wi = 0; wi < words; wi++) {
        size_t w = (wi + start) % words;

        uint64_t word;
        __builtin_memcpy(&word, dev->bitmap + w * 8, 8);
        if (word == ~0ULL) continue;

        for (int b = 0; b < 64; b++) {
            size_t idx = w * 64 + (size_t)b;
            if (idx >= dev->nslots) break;
            if (!(word & (1ULL << b))) {
                word |= (1ULL << b);
                __builtin_memcpy(dev->bitmap + w * 8, &word, 8);
                dev->free_count--;
                dev->scan_hint = (idx + 1) % dev->nslots;
                return swap_encode_pte(dev_idx, idx);
            }
        }
    }
    return SWAP_SLOT_NONE;
}

uint64_t swap_encode_pte(int dev_idx, size_t slot_off)
{
    return ((uint64_t)slot_off << SWAP_PTE_SLOT_SHIFT) |
           ((uint64_t)(dev_idx & SWAP_PTE_DEV_MASK) << SWAP_PTE_DEV_SHIFT) |
           0ULL;
}

int swap_decode_pte(uint64_t pte, int *dev_idx, size_t *slot_off)
{
    if (pte & 1) return -1;
    *dev_idx  = (int)((pte >> SWAP_PTE_DEV_SHIFT) & SWAP_PTE_DEV_MASK);
    *slot_off = (size_t)(pte >> SWAP_PTE_SLOT_SHIFT);
    return 0;
}

swap_slot_t swap_alloc_slot(void)
{
    uint64_t fl = spin_lock_irqsave(&g_lock);

    for (int i = 0; i < g_ndevs; i++) {
        swap_slot_t s = slot_alloc_dev(&g_devs[i], i);
        if (s != SWAP_SLOT_NONE) {
            g_free_slots--;
            spin_unlock_irqrestore(&g_lock, fl);
            return s;
        }
    }

    spin_unlock_irqrestore(&g_lock, fl);
    return SWAP_SLOT_NONE;
}

void swap_free_slot(swap_slot_t slot)
{
    int   dev_idx;
    size_t slot_off;

    if (swap_decode_pte(slot, &dev_idx, &slot_off) != 0) return;
    if (dev_idx >= g_ndevs) return;

    swap_dev_t *dev = &g_devs[dev_idx];
    if (!dev->active || slot_off >= dev->nslots) return;

    uint64_t fl = spin_lock_irqsave(&g_lock);

    size_t w = slot_off / 64;
    int    b = (int)(slot_off % 64);
    uint64_t word;
    __builtin_memcpy(&word, dev->bitmap + w * 8, 8);
    word &= ~(1ULL << b);
    __builtin_memcpy(dev->bitmap + w * 8, &word, 8);
    dev->free_count++;
    g_free_slots++;

    spin_unlock_irqrestore(&g_lock, fl);
}

int swap_write_page(swap_slot_t slot, const void *page)
{
    int    dev_idx;
    size_t slot_off;

    if (swap_decode_pte(slot, &dev_idx, &slot_off) != 0) return -1;
    if (dev_idx >= g_ndevs) return -1;

    swap_dev_t *dev = &g_devs[dev_idx];
    if (!dev->active || slot_off >= dev->nslots) return -1;

    uint64_t lba = dev->start_sector + slot_off * (uint64_t)SECTORS_PER_PAGE;
    return blkdev_write_page(dev->blkdev_idx, lba, page);
}

int swap_read_page(swap_slot_t slot, void *page)
{
    int    dev_idx;
    size_t slot_off;

    if (swap_decode_pte(slot, &dev_idx, &slot_off) != 0) return -1;
    if (dev_idx >= g_ndevs) return -1;

    swap_dev_t *dev = &g_devs[dev_idx];
    if (!dev->active || slot_off >= dev->nslots) return -1;

    uint64_t lba = dev->start_sector + slot_off * (uint64_t)SECTORS_PER_PAGE;
    return blkdev_read_page(dev->blkdev_idx, lba, page);
}

int swap_out_page(uintptr_t phys, uint64_t *pte_out)
{
    if (!g_active || !g_free_slots) return -1;

    swap_slot_t slot = swap_alloc_slot();
    if (slot == SWAP_SLOT_NONE) return -1;

    void *vaddr = (void *)phys;
    if (swap_write_page(slot, vaddr) != 0) {
        swap_free_slot(slot);
        return -1;
    }

    *pte_out = slot;
    mreclaim_account_free(1);
    return 0;
}

int swap_in_page(uint64_t swap_pte, uintptr_t *phys_out)
{
    if (!g_active) return -1;

    void *page = kmalloc(SWAP_PAGE_SIZE);
    if (!page) return -1;

    if (swap_read_page((swap_slot_t)swap_pte, page) != 0) {
        kfree(page);
        return -1;
    }

    swap_free_slot((swap_slot_t)swap_pte);
    *phys_out = (uintptr_t)page;
    mreclaim_account_alloc(1);
    return 0;
}

int swap_add_dev(int blkdev_idx, uint64_t start_lba, size_t nslots)
{
    uint64_t fl = spin_lock_irqsave(&g_lock);

    if (g_ndevs >= SWAP_MAX_DEVS) {
        spin_unlock_irqrestore(&g_lock, fl);
        return -1;
    }

    size_t bm_bytes = BITMAP_WORDS(nslots) * 8;
    uint8_t *bm = kmalloc(bm_bytes);
    if (!bm) {
        spin_unlock_irqrestore(&g_lock, fl);
        return -1;
    }

    for (size_t i = 0; i < bm_bytes; i++) bm[i] = 0;

    int idx = g_ndevs++;
    swap_dev_t *dev    = &g_devs[idx];
    dev->blkdev_idx    = blkdev_idx;
    dev->start_sector  = start_lba;
    dev->nslots        = nslots;
    dev->free_count    = nslots;
    dev->bitmap        = bm;
    dev->scan_hint     = 0;
    dev->active        = 1;

    g_total_slots += nslots;
    g_free_slots  += nslots;
    g_active       = 1;

    spin_unlock_irqrestore(&g_lock, fl);

    char msg[80];
    ksnprintf(msg, sizeof(msg),
              "dev=%d  blkdev=%d  lba=0x%llx  slots=%zu  size=%zu MiB",
              idx, blkdev_idx, (unsigned long long)start_lba,
              nslots, (nslots * SWAP_PAGE_SIZE) >> 20);
    klog_ok("swap", msg);

    return idx;
}

int swap_remove_dev(int dev_idx)
{
    uint64_t fl = spin_lock_irqsave(&g_lock);

    if (dev_idx < 0 || dev_idx >= g_ndevs || !g_devs[dev_idx].active) {
        spin_unlock_irqrestore(&g_lock, fl);
        return -1;
    }

    swap_dev_t *dev = &g_devs[dev_idx];
    g_total_slots  -= dev->nslots;
    g_free_slots   -= dev->free_count;
    dev->active     = 0;

    if (dev->bitmap) {
        kfree(dev->bitmap);
        dev->bitmap = NULL;
    }

    g_active = (g_total_slots > 0);
    spin_unlock_irqrestore(&g_lock, fl);

    char msg[32];
    ksnprintf(msg, sizeof(msg), "swap dev %d removed", dev_idx);
    klog_info("swap", msg);
    return 0;
}

int swap_active(void)         { return g_active; }
size_t swap_total_slots(void) { return g_total_slots; }
size_t swap_free_slots(void)  { return g_free_slots; }

int swap_probe_blkdev(void)
{
    int n = blkdev_count();
    if (n <= 0) {
        klog_info("swap", "no block devices -- swap disabled");
        return 0;
    }
    klog_info("swap", "no auto-detect -- use swap_add_dev() to activate");
    return 0;
}

void swap_init(void)
{
    for (int i = 0; i < SWAP_MAX_DEVS; i++) {
        g_devs[i].active = 0;
        g_devs[i].bitmap = NULL;
    }
    klog_ok("swap", "subsystem ready");
}
