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

#include "devfs.h"
#include "devfs_tykid.h"
#include "devfs_kobalt.h"
#include "devfs_bdev.h"
#include "../../inc/blkdev.h"
#include "../../inc/kernel.h"
#include "../../inc/string.h"

extern devfs_ops_t devfs_null_ops;
extern devfs_ops_t devfs_zero_ops;
extern devfs_ops_t devfs_random_ops;
extern devfs_ops_t devfs_urandom_ops;
extern devfs_ops_t devfs_tty_ops;
extern devfs_ops_t devfs_uart_ops;

static void register_builtins(void)
{
    devfs_register_cdev(DEVFS_MAJOR_MEM, DEVFS_MEM_NULL,    "null",
                        DEVFS_CLASS_MEM, &devfs_null_ops,    NULL);
    devfs_register_cdev(DEVFS_MAJOR_MEM, DEVFS_MEM_ZERO,    "zero",
                        DEVFS_CLASS_MEM, &devfs_zero_ops,    NULL);
    devfs_register_cdev(DEVFS_MAJOR_MEM, DEVFS_MEM_RANDOM,  "random",
                        DEVFS_CLASS_MEM | DEVFS_CLASS_PRIV,
                        &devfs_random_ops, NULL);
    devfs_register_cdev(DEVFS_MAJOR_MEM, DEVFS_MEM_URANDOM, "urandom",
                        DEVFS_CLASS_MEM, &devfs_urandom_ops, NULL);
    devfs_register_cdev(DEVFS_MAJOR_TTY, 0,  "tty",
                        DEVFS_CLASS_TTY, &devfs_tty_ops,  NULL);
    devfs_register_cdev(DEVFS_MAJOR_TTY, 64, "ttyS0",
                        DEVFS_CLASS_TTY, &devfs_uart_ops, NULL);
}

void devfs_kobalt_init(tykid_gate_ctx_t *ctx)
{
    (void)ctx;

    spin_lock(&g_devfs.lock);
    if (g_devfs.mounted) {
        spin_unlock(&g_devfs.lock);
        return;
    }
    spin_unlock(&g_devfs.lock);

    devfs_node_t *root = devfs_alloc_node();
    if (!root) return;

    strncpy(root->name, "dev", DEVFS_NAME_MAX - 1);
    root->type        = DEVFS_TYPE_DIR;
    root->tykid_class = DEVFS_CLASS_MEM;
    root->mode        = DEVFS_MODE_RUSR | DEVFS_MODE_RGRP | DEVFS_MODE_ROTH;

    spin_lock(&g_devfs.lock);

    if (g_devfs.mounted) {
        spin_unlock(&g_devfs.lock);
        devfs_free_node(root);
        return;
    }
    g_devfs.root    = root;
    g_devfs.mounted = 1;
    spin_unlock(&g_devfs.lock);

    register_builtins();
    blkdev_set_hotplug_cb(devfs_bdev_hotplug);
    klog_ok("devfs", "/dev mounted");
}

int devfs_kobalt_is_mounted(void)
{
    return g_devfs.mounted;
}

void devfs_kobalt_seal(void)
{
    g_devfs.sealed = 1;
}
