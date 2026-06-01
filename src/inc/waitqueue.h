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

#include "spinlock.h"
#include "sched.h"

typedef struct {
    spinlock_t      lock;
    sched_thread_t *head;
} wait_queue_t;

#define WAIT_QUEUE_INIT  { SPINLOCK_INIT, NULL }
#define DEFINE_WAIT_QUEUE(name)  wait_queue_t name = WAIT_QUEUE_INIT

void wq_wait(wait_queue_t *wq);
void wq_wake_one(wait_queue_t *wq);
void wq_wake_all(wait_queue_t *wq);
