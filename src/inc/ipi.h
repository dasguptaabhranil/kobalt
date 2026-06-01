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

#pragma once

#include <stdint.h>

#define IPI_VECTOR_TLB_FLUSH    (0x50U)
#define IPI_VECTOR_FUNC_CALL    (0x51U)
#define IPI_VECTOR_HALT         (0x52U)
#define IPI_VECTOR_RESCHED      (0x53U)

typedef struct {
    uintptr_t       vaddr;
    uint64_t        npages;
    volatile int    ack_count;
} __attribute__((aligned(64))) tlb_shootdown_t;

typedef void (*ipi_fn_t)(void *arg);

typedef struct {
    ipi_fn_t        fn;
    void           *arg;
    volatile int    ack_count;
} __attribute__((aligned(64))) ipi_call_t;

void ipi_init(void);

void ipi_send_single(uint32_t apic_id, uint8_t vector);
void ipi_send_to_cpu(uint32_t cpu_id, uint8_t vector);
void ipi_send_others(uint8_t vector);
void ipi_send_all(uint8_t vector);

void ipi_tlb_flush_page(uintptr_t vaddr);
void ipi_tlb_flush_range(uintptr_t vaddr, uint64_t npages);
void ipi_tlb_flush_all(void);

void ipi_call_function(ipi_fn_t fn, void *arg);
void ipi_panic_halt(void);

extern void ipi_entry_tlb_flush(void);
extern void ipi_entry_func_call(void);
extern void ipi_entry_halt(void);
extern void ipi_entry_resched(void);
