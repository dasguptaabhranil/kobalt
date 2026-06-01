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

#ifndef SWAP_H
#define SWAP_H

#include <stdint.h>
#include <stddef.h>

#define SWAP_SLOT_NONE      (~0ULL)
#define SWAP_MAX_DEVS       4
#define SWAP_PAGE_SIZE      4096

typedef uint64_t swap_slot_t;

typedef struct {
    int         blkdev_idx;
    uint64_t    start_sector;
    size_t      nslots;
    size_t      free_count;
    uint8_t    *bitmap;
    size_t      scan_hint;
    int         active;
} swap_dev_t;

void        swap_init(void);
int         swap_add_dev(int blkdev_idx, uint64_t start_lba, size_t nslots);
int         swap_remove_dev(int dev_idx);
swap_slot_t swap_alloc_slot(void);
void        swap_free_slot(swap_slot_t slot);
int         swap_write_page(swap_slot_t slot, const void *page);
int         swap_read_page(swap_slot_t slot, void *page);
int         swap_out_page(uintptr_t phys, uint64_t *pte_out);
int         swap_in_page(uint64_t swap_pte, uintptr_t *phys_out);
int         swap_active(void);
size_t      swap_total_slots(void);
size_t      swap_free_slots(void);
int         swap_probe_blkdev(void);

uint64_t    swap_encode_pte(int dev_idx, size_t slot_off);
int         swap_decode_pte(uint64_t pte, int *dev_idx, size_t *slot_off);

#endif
