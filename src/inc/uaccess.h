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
#include <stddef.h>

typedef struct {
    uint64_t fault_va;
    uint64_t fixup_va;
} uaccess_ex_entry_t;

extern uaccess_ex_entry_t __uaccess_ex_start[];
extern uaccess_ex_entry_t __uaccess_ex_end[];

uint64_t uaccess_find_fixup(uint64_t rip);

int copy_to_user  (void       *udst, const void *ksrc, size_t n);
int copy_from_user(void       *kdst, const void *usrc, size_t n);

int put_user_8 (uint8_t  val, uint8_t  *uaddr);
int put_user_32(uint32_t val, uint32_t *uaddr);
int put_user_64(uint64_t val, uint64_t *uaddr);

int get_user_8 (uint8_t  *out, const uint8_t  *uaddr);
int get_user_32(uint32_t *out, const uint32_t *uaddr);
int get_user_64(uint64_t *out, const uint64_t *uaddr);
