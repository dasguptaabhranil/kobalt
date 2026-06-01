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

#ifndef HUGEPAGE_H
#define HUGEPAGE_H

#include <stdint.h>
#include <stddef.h>

#define HP_2M_SHIFT     21
#define HP_1G_SHIFT     30
#define HP_2M_SIZE      (1UL << HP_2M_SHIFT)
#define HP_1G_SIZE      (1UL << HP_1G_SHIFT)
#define HP_2M_MASK      (HP_2M_SIZE - 1UL)
#define HP_1G_MASK      (HP_1G_SIZE - 1UL)

#define HP_FLAG_RW      (1U << 0)
#define HP_FLAG_USER    (1U << 1)
#define HP_FLAG_NX      (1U << 2)
#define HP_FLAG_GLOBAL  (1U << 3)
#define HP_FLAG_WC      (1U << 4)
#define HP_FLAG_UC      (1U << 5)

typedef enum {
    HP_ORDER_2M = 0,
    HP_ORDER_1G,
} hp_order_t;

void        hugepage_init(void);
uintptr_t   hugepage_alloc(hp_order_t order);
void        hugepage_free(uintptr_t phys, hp_order_t order);
int         hugepage_map(uintptr_t virt, uintptr_t phys, hp_order_t order, uint32_t flags);
int         hugepage_unmap(uintptr_t virt, hp_order_t order);
int         hugepage_map_cr3(uint64_t cr3, uintptr_t virt, uintptr_t phys,
                              hp_order_t order, uint32_t flags);
int         hugepage_available(hp_order_t order);
size_t      hugepage_free_count(hp_order_t order);
size_t      hugepage_total_count(hp_order_t order);
int         hugepage_pdpe1gb_supported(void);

#endif
