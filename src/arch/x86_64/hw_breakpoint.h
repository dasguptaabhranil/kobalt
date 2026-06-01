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
#include "debug.h"

#define HWB_SLOTS   4

typedef enum { HWB_EXEC = 0, HWB_WRITE = 1, HWB_RW = 3 } hwb_cond_t;
typedef enum { HWB_1B = 0, HWB_2B = 1, HWB_8B = 2, HWB_4B = 3 } hwb_size_t;

typedef struct {
    uintptr_t  addr;
    hwb_cond_t cond;
    hwb_size_t size;
    int        active;
} hwb_slot_t;

int  hwb_set(int slot, uintptr_t addr, hwb_cond_t cond, hwb_size_t size);
void hwb_clear(int slot);
void hwb_clear_all(void);

void hwb_commit(void);

int  hwb_triggered(void);
