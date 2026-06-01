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

#include "rbtree.h"
#include <stdint.h>
#include <stddef.h>

typedef enum {
    THREAD_READY   = 0,
    THREAD_RUNNING = 1,
    THREAD_BLOCKED = 2,
    THREAD_ZOMBIE  = 3,
} thread_state_t;

#define SCHED_STATE_RUNNABLE  0
#define SCHED_STATE_RUNNING   1
#define SCHED_STATE_BLOCKED   2
#define SCHED_STATE_DEAD      3

typedef void (*thread_fn_t)(void *arg);

typedef struct sched_thread {
    uint64_t        kernel_rsp;
    uint64_t        kstack_top;
    int64_t         vruntime;
    int64_t         vstart;
    int64_t         vdeadline;
    uint64_t        vlag;
    uint32_t        tid;
    uint32_t        cpu;
    thread_state_t  state;
    uint32_t        slice;
    uint32_t        weight;
    uint32_t        inv_weight;
    uint32_t        amx_permitted;
    void           *amx_context_raw;
    void           *amx_context;
    uint64_t        amx_signature;
    const char     *name;
    void           *kstack_base;
    size_t          kstack_size;
    struct rb_node  rb;
    struct sched_thread *wq_next;
    void          (*fault_fn)(struct sched_thread *);
    struct sched_thread *list_next;
    void           *fpu_state;
} sched_thread_t;

_Static_assert(offsetof(sched_thread_t, kernel_rsp) == 0, "");
_Static_assert(offsetof(sched_thread_t, kstack_top) == 8, "");

#define NICE_0_LOAD           1024u
#define WEIGHT_DEFAULT        NICE_0_LOAD
#define SLICE_VTIME           5
#define SCHED_KSTACK_SIZE     0x4000u
#define SCHED_DEFAULT_QUANTUM 5u

void            sched_init(uint64_t bsp_stack_top);
void            sched_ap_init(uint32_t cpu_id);
sched_thread_t *sched_thread_create(const char *name, thread_fn_t fn,
                                    void *arg, uint32_t weight);
void __attribute__((noreturn)) sched_thread_exit(void);
void __attribute__((noreturn)) sched_kill_current(void);
void            sched_yield(void);
void            sched_block(void);
void            sched_unblock(sched_thread_t *t);
sched_thread_t *sched_current(void);
void            sched_tick(void);

uint32_t        sched_thread_count(void);
sched_thread_t *sched_get_thread(uint32_t idx);
sched_thread_t *sched_get_thread_by_tid(uint32_t tid);
uint32_t        sched_thread_get_tid(sched_thread_t *t);
const char     *sched_thread_get_name(sched_thread_t *t);
uint32_t        sched_thread_get_state(sched_thread_t *t);
