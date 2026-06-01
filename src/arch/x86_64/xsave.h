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

#define XCR0_X87        (1ULL << 0)
#define XCR0_SSE        (1ULL << 1)
#define XCR0_AVX        (1ULL << 2)
#define XCR0_OPMASK     (1ULL << 5)
#define XCR0_ZMM_HI256  (1ULL << 6)
#define XCR0_HI16_ZMM   (1ULL << 7)
#define XCR0_PT         (1ULL << 8)
#define XCR0_PKRU       (1ULL << 9)

#define XSAVE_LEGACY_SZ 512U

extern size_t   g_xsave_size;
extern uint64_t g_xcr0;
extern int      g_xsave_avail;

void  xsave_arch_init(void);
void *xsave_alloc(void);
void  xsave_free(void *area);
void  xsave_zero(void *area);
void  xsave_save(void *area);
void  xsave_restore(const void *area);
