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

#include "waitqueue.h"
#include "sched.h"

void wq_wait(wait_queue_t *wq)
{
    uint64_t f;
    __asm__ volatile("pushfq; pop %0; cli" : "=rm"(f) :: "memory");

    spin_lock(&wq->lock);
    sched_thread_t *cur = sched_current();
    cur->wq_next = wq->head;
    wq->head     = cur;
    spin_unlock(&wq->lock);

    sched_block();

    __asm__ volatile("push %0; popfq" :: "rm"(f) : "memory", "cc");
}

void wq_wake_one(wait_queue_t *wq)
{
    uint64_t f = spin_lock_irqsave(&wq->lock);
    sched_thread_t *t = wq->head;
    if (t) {
        wq->head    = t->wq_next;
        t->wq_next  = NULL;
    }
    spin_unlock_irqrestore(&wq->lock, f);
    if (t) sched_unblock(t);
}

void wq_wake_all(wait_queue_t *wq)
{
    uint64_t f = spin_lock_irqsave(&wq->lock);
    sched_thread_t *list = wq->head;
    wq->head = NULL;
    spin_unlock_irqrestore(&wq->lock, f);

    while (list) {
        sched_thread_t *next = list->wq_next;
        list->wq_next = NULL;
        sched_unblock(list);
        list = next;
    }
}
