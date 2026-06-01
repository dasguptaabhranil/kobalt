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

#include "flatfs_internal.h"
#include "flatfs_monitor.h"
#include "flatfs_super.h"

static int g_arb_handle = -1;
static int g_arb_degraded = 0;

__attribute__((weak)) int  fs_arbiter_register(const char *name, int priority)
    { (void)name; (void)priority; return 1; }
__attribute__((weak)) void fs_arbiter_unregister(int handle)
    { (void)handle; }
__attribute__((weak)) int  fs_arbiter_set_priority(int handle, int priority)
    { (void)handle; (void)priority; return 0; }
__attribute__((weak)) int  fs_arbiter_drain_pending(int handle)
    { (void)handle; return 0; }

#define FLATFS_ARB_NORMAL_PRIO    100
#define FLATFS_ARB_DEGRADED_PRIO   10

flatfs_err_t flatfs_arbiter_register_fs(void)
{
    if (!(g_fs.super->features & FLATFS_FEAT_ARBITER))
        return FLATFS_OK;

    g_arb_handle = fs_arbiter_register("flatfs", FLATFS_ARB_NORMAL_PRIO);
    return (g_arb_handle >= 0) ? FLATFS_OK : FLATFS_ERR_HANDOFF;
}

flatfs_err_t flatfs_arbiter_unregister_fs(void)
{
    if (g_arb_handle >= 0) {
        fs_arbiter_unregister(g_arb_handle);
        g_arb_handle = -1;
    }
    return FLATFS_OK;
}

flatfs_err_t flatfs_arbiter_notify_degraded(void)
{
    if (g_arb_handle < 0) return FLATFS_ERR_HANDOFF;
    if (g_arb_degraded) return FLATFS_OK;

    int rc = fs_arbiter_set_priority(g_arb_handle, FLATFS_ARB_DEGRADED_PRIO);
    if (rc < 0) return FLATFS_ERR_HANDOFF;

    g_arb_degraded = 1;
    return FLATFS_OK;
}

flatfs_err_t flatfs_arbiter_notify_recovered(void)
{
    if (g_arb_handle < 0 || !g_arb_degraded) return FLATFS_OK;

    fs_arbiter_drain_pending(g_arb_handle);
    fs_arbiter_set_priority(g_arb_handle, FLATFS_ARB_NORMAL_PRIO);
    g_arb_degraded = 0;
    return FLATFS_OK;
}

int flatfs_arbiter_is_degraded(void)
{
    return g_arb_degraded;
}
