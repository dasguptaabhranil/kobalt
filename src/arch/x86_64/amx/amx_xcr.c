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

#include "amx_xcr.h"
#include <amx.h>

void amx_xcr0_enable(void)
{
    xsetbv(0, xgetbv(0) | XCR0_AMX_MASK);
}

void amx_xcr0_disable(void)
{
    xsetbv(0, xgetbv(0) & ~XCR0_AMX_MASK);
}
