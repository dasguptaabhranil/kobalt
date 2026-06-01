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

#ifndef DEVFS_INTERNAL_H
#define DEVFS_INTERNAL_H

#include "../../inc/devfs.h"
#include "../../inc/spinlock.h"
#include "../../inc/kernel.h"
#include "../../inc/kobalt_ident.h"

#define DEVFS_NAME_MAX    64u
#define DEVFS_MAX_NODES  512u
#define DEVFS_MAX_OPEN    64u

#define DEVFS_TYPE_CDEV   0x01u
#define DEVFS_TYPE_BDEV   0x02u
#define DEVFS_TYPE_DIR    0x04u
#define DEVFS_TYPE_LINK   0x08u

#define DEVFS_MODE_RUSR  0400u
#define DEVFS_MODE_WUSR  0200u
#define DEVFS_MODE_RGRP  0040u
#define DEVFS_MODE_WGRP  0020u
#define DEVFS_MODE_ROTH  0004u
#define DEVFS_MODE_WOTH  0002u

typedef struct devfs_node {
    char     name[DEVFS_NAME_MAX];
    dev_t    dev;
    uint8_t  type;
    uint16_t mode;
    uint32_t tykid_class;

    uint32_t tykid_tag;

    devfs_ops_t *ops;
    void        *priv;

    struct devfs_node *parent;
    struct devfs_node *sibling;
    struct devfs_node *children;

    uint8_t  sealed;
    uint8_t  _inuse;
} devfs_node_t;

typedef struct devfs_file {
    devfs_node_t *node;
    int           flags;
    uint64_t      pos;
    uint8_t       _inuse;
} devfs_file_t;

typedef struct {
    devfs_node_t  pool[DEVFS_MAX_NODES];
    devfs_file_t  fpool[DEVFS_MAX_OPEN];
    devfs_node_t *root;
    spinlock_t    lock;
    uint32_t      n_nodes;
    uint8_t       mounted;
    uint8_t       sealed;
} devfs_state_t;

extern devfs_state_t g_devfs;

devfs_node_t *devfs_alloc_node(void);
void          devfs_free_node(devfs_node_t *n);
devfs_file_t *devfs_alloc_file(void);
void          devfs_free_file(devfs_file_t *f);

int           devfs_link_child(devfs_node_t *parent, devfs_node_t *child);
void          devfs_unlink_child(devfs_node_t *parent, devfs_node_t *child);
devfs_node_t *devfs_lookup(const char *name, devfs_node_t *dir);
devfs_node_t *devfs_lookup_path(const char *path);

int devfs__register_node(devfs_node_t *n);

#endif
