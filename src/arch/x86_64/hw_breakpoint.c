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

#include "hw_breakpoint.h"

static hwb_slot_t slots[HWB_SLOTS];

void hw_breakpoint_init(void)
{
    hwb_clear_all();
}

int hwb_set(int n, uintptr_t addr, hwb_cond_t cond, hwb_size_t size)
{
    if ((unsigned)n >= HWB_SLOTS) return -1;
    slots[n].addr   = addr;
    slots[n].cond   = cond;
    slots[n].size   = size;
    slots[n].active = 1;
    hwb_commit();
    return n;
}

void hwb_clear(int n)
{
    if ((unsigned)n >= HWB_SLOTS) return;
    slots[n].active = 0;
    hwb_commit();
}

void hwb_clear_all(void)
{
    for (int i = 0; i < HWB_SLOTS; i++)
        slots[i].active = 0;
    debug_init();
}

void hwb_commit(void)
{
    uintptr_t addrs[4] = {0,0,0,0};
    uint64_t dr7 = 0;

    for (int i = 0; i < HWB_SLOTS; i++) {
        if (!slots[i].active) continue;
        addrs[i] = slots[i].addr;
        dr7 |= (uint64_t)DR7_G0 << (i * 2);
        dr7 |= DR7_COND(i, (unsigned)slots[i].cond);
        dr7 |= DR7_SIZE(i, (unsigned)slots[i].size);
    }

    debug_set_dr(0, addrs[0]);
    debug_set_dr(1, addrs[1]);
    debug_set_dr(2, addrs[2]);
    debug_set_dr(3, addrs[3]);
    debug_set_dr7(dr7);
}

int hwb_triggered(void)
{
    uint64_t dr6 = debug_dr6();
    int mask = (int)(dr6 & 0xFU);
    if (mask)
        debug_clear_dr6();
    return mask;
}
