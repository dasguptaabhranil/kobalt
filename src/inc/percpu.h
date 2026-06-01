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
#include "../arch/x86_64/msr.h"

typedef struct percpu {
    struct percpu *self;
    uint32_t       cpu_id;
    uint32_t       apic_id;
    uint64_t       kernel_rsp0;
    void          *current_thread;
    uint64_t       idle_ticks;
    uint64_t       sched_ticks;
    uint32_t       preempt_depth;
    uint32_t       nmi_count;
    uint8_t        _pad[8];
} __attribute__((aligned(64))) percpu_t;

_Static_assert(sizeof(percpu_t) == 64, "percpu_t must be exactly 64 bytes");

#define PERCPU_OFF_SELF         0
#define PERCPU_OFF_CPU_ID       8
#define PERCPU_OFF_APIC_ID      12
#define PERCPU_OFF_RSP0         16
#define PERCPU_OFF_CURRENT      24
#define PERCPU_OFF_IDLE_TICKS   32
#define PERCPU_OFF_SCHED_TICKS  40
#define PERCPU_OFF_PREEMPT      48

static inline void percpu_install(percpu_t *p)
{
    p->self = p;
    wrmsr(MSR_GS_BASE, (uint64_t)(uintptr_t)p);
}

static inline percpu_t *percpu_get(void)
{
    percpu_t *p;
    __asm__ volatile ("movq %%gs:%c1, %0"
                      : "=r"(p)
                      : "i"(PERCPU_OFF_SELF)
                      : "memory");
    return p;
}

#define PERCPU_ID()   ({ uint32_t _id; \
    __asm__ volatile ("movl %%gs:%c1, %0" : "=r"(_id) : "i"(PERCPU_OFF_CPU_ID)); \
    _id; })

#define PERCPU_APIC() ({ uint32_t _id; \
    __asm__ volatile ("movl %%gs:%c1, %0" : "=r"(_id) : "i"(PERCPU_OFF_APIC_ID)); \
    _id; })

static inline void preempt_disable(void)
{
    __asm__ volatile ("incl %%gs:%c0" : : "i"(PERCPU_OFF_PREEMPT) : "memory", "cc");
}

static inline void preempt_enable(void)
{
    __asm__ volatile ("decl %%gs:%c0" : : "i"(PERCPU_OFF_PREEMPT) : "memory", "cc");
}

static inline uint32_t preempt_depth(void)
{
    uint32_t d;
    __asm__ volatile ("movl %%gs:%c1, %0"
                      : "=r"(d)
                      : "i"(PERCPU_OFF_PREEMPT)
                      : "memory");
    return d;
}
