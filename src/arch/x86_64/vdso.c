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

#include "vdso.h"
#include "../../inc/kernel.h"
#include <string.h>

vdso_data_t g_vdso_data __attribute__((aligned(4096)));

void vdso_init(void)
{
    memset(&g_vdso_data, 0, sizeof(g_vdso_data));

}

void vdso_update_clocks(uint64_t wall_ns, uint64_t mono_ns)
{

    __atomic_fetch_add(&g_vdso_data.seq, 1, __ATOMIC_RELEASE);
    store_barrier();

    g_vdso_data.wall_clock_ns = wall_ns;
    g_vdso_data.monotonic_ns  = mono_ns;

    store_barrier();
    __atomic_fetch_add(&g_vdso_data.seq, 1, __ATOMIC_RELEASE);
}
