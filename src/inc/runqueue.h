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

#include <rbtree.h>
#include <spinlock.h>
#include <percpu.h>
#include <stdint.h>

#define SCHED_MAX_CPUS 16

struct sched_thread;

typedef struct runqueue {
    spinlock_t           lock;
    rb_root_t            tree;
    int64_t              min_vruntime;
    uint32_t             nr_running;
    struct sched_thread *current;
    struct sched_thread *idle;
    struct sched_thread *corpse;
    uint8_t              _pad[8];
} runqueue_t __attribute__((aligned(64)));

extern runqueue_t rqs[SCHED_MAX_CPUS];

static inline uint32_t cpu_id(void)      { return PERCPU_ID(); }
static inline runqueue_t *this_rq(void)      { return &rqs[cpu_id()]; }
static inline runqueue_t *cpu_rq(uint32_t c) { return &rqs[c]; }
