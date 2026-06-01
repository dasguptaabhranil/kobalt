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

void smap_init(void);

static __inline__ __attribute__((always_inline)) void stac(void)
{
    __asm__ volatile (".byte 0x0f,0x01,0xcb" ::: "memory");
}

static __inline__ __attribute__((always_inline)) void clac(void)
{
    __asm__ volatile (".byte 0x0f,0x01,0xca" ::: "memory");
}
