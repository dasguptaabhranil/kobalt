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

typedef union {
    uint32_t raw;
    struct {
        uint16_t now_serving;
        uint16_t next_ticket;
    };
} spinlock_t;

#define SPINLOCK_INIT   ((spinlock_t){ .raw = 0U })

static inline void spinlock_init(spinlock_t *l) { l->raw = 0U; }

static inline void spin_lock(spinlock_t *lock)
{
    uint16_t ticket;
    __asm__ volatile (
        "lock xaddw %w0, %1"
        : "=r"(ticket), "+m"(lock->next_ticket)
        : "0"((uint16_t)1U)
        : "memory"
    );
    while (__builtin_expect(
               (volatile uint16_t)lock->now_serving != ticket, 0)) {
        __asm__ volatile ("pause" ::: "memory");
    }
}

static inline void spin_unlock(spinlock_t *lock)
{
    __asm__ volatile ("" ::: "memory");
    lock->now_serving++;
}

static inline int spin_trylock(spinlock_t *lock)
{
    spinlock_t old, new_val;
    old.raw = lock->raw;
    if (old.now_serving != old.next_ticket)
        return 0;
    new_val.now_serving  = old.now_serving;
    new_val.next_ticket  = (uint16_t)(old.next_ticket + 1U);
    return __sync_bool_compare_and_swap(&lock->raw, old.raw, new_val.raw);
}

static inline uint64_t spin_lock_irqsave(spinlock_t *lock)
{
    uint64_t flags;
    __asm__ volatile (
        "pushfq         \n"
        "popq  %0       \n"
        "cli            \n"
        : "=r"(flags)
        :
        : "memory"
    );
    spin_lock(lock);
    return flags;
}

static inline void spin_unlock_irqrestore(spinlock_t *lock, uint64_t flags)
{
    spin_unlock(lock);
    __asm__ volatile (
        "pushq %0       \n"
        "popfq          \n"
        :
        : "r"(flags)
        : "memory", "cc"
    );
}

static inline int spin_is_locked(const spinlock_t *lock)
{
    spinlock_t s = { .raw = lock->raw };
    return s.now_serving != s.next_ticket;
}
