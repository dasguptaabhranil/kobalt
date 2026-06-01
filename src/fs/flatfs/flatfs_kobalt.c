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

#include "../../inc/kernel.h"
#include "flatfs.h"
#include "flatfs_super.h"
#include "flatfs_kobalt.h"

#define FLATFS_KOBALT_BUF_MB   8u
#define FLATFS_KOBALT_BUF_SIZE (FLATFS_KOBALT_BUF_MB * 1024u * 1024u)

static uint8_t g_flatfs_buf[FLATFS_KOBALT_BUF_SIZE]
    __attribute__((aligned(4096)));

static int g_flatfs_mounted;

void flatfs_kobalt_mount(tykid_gate_ctx_t *ty_ctx)
{
    if (g_flatfs_mounted) return;

    flatfs_err_t e = flatfs_mount(g_flatfs_buf, FLATFS_KOBALT_BUF_SIZE,
                                   ty_ctx);

    if (e == FLATFS_ERR_MAGIC || e == FLATFS_ERR_CORRUPT) {
        e = flatfs_super_init(g_flatfs_buf, FLATFS_KOBALT_BUF_SIZE,
                              "kobalt", 2048);
        if (e != FLATFS_OK) {
            klog_warn("flatfs", "format failed");
            return;
        }
        e = flatfs_mount(g_flatfs_buf, FLATFS_KOBALT_BUF_SIZE, ty_ctx);
    }

    if (e != FLATFS_OK) {
        klog_warn("flatfs", flatfs_strerror(e));
        return;
    }

    g_flatfs_mounted = 1;
    klog_ok("flatfs", "mounted (8 MiB static workspace)");
}

void flatfs_kobalt_unmount(void)
{
    if (!g_flatfs_mounted) return;
    flatfs_unmount();
    g_flatfs_mounted = 0;
}

int flatfs_kobalt_is_mounted(void)
{
    return g_flatfs_mounted;
}
