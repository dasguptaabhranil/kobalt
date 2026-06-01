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
#include "../../inc/kernel.h"
#include "../../inc/string.h"
#include "../../inc/spinlock.h"

devfs_state_t g_devfs;

devfs_node_t *devfs_alloc_node(void)
{
    spin_lock(&g_devfs.lock);
    for (uint32_t i = 0; i < DEVFS_MAX_NODES; i++) {
        if (!g_devfs.pool[i]._inuse) {
            devfs_node_t *n = &g_devfs.pool[i];
            memset(n, 0, sizeof(*n));
            n->_inuse = 1;
            g_devfs.n_nodes++;
            spin_unlock(&g_devfs.lock);
            return n;
        }
    }
    spin_unlock(&g_devfs.lock);
    return NULL;
}

void devfs_free_node(devfs_node_t *n)
{
    if (!n) return;
    spin_lock(&g_devfs.lock);
    n->_inuse = 0;
    g_devfs.n_nodes--;
    spin_unlock(&g_devfs.lock);
}

devfs_file_t *devfs_alloc_file(void)
{
    spin_lock(&g_devfs.lock);
    for (uint32_t i = 0; i < DEVFS_MAX_OPEN; i++) {
        if (!g_devfs.fpool[i]._inuse) {
            devfs_file_t *f = &g_devfs.fpool[i];
            memset(f, 0, sizeof(*f));
            f->_inuse = 1;
            spin_unlock(&g_devfs.lock);
            return f;
        }
    }
    spin_unlock(&g_devfs.lock);
    return NULL;
}

void devfs_free_file(devfs_file_t *f)
{
    if (!f) return;
    spin_lock(&g_devfs.lock);
    f->_inuse = 0;
    spin_unlock(&g_devfs.lock);
}

int devfs_link_child(devfs_node_t *parent, devfs_node_t *child)
{
    if (!parent || !child) return -1;
    if (parent->type != DEVFS_TYPE_DIR) return -1;
    child->parent = parent;
    child->sibling = parent->children;
    parent->children = child;
    return 0;
}

void devfs_unlink_child(devfs_node_t *parent, devfs_node_t *child)
{
    if (!parent || !child) return;
    devfs_node_t **pp = &parent->children;
    while (*pp) {
        if (*pp == child) {
            *pp = child->sibling;
            child->sibling = NULL;
            child->parent  = NULL;
            return;
        }
        pp = &(*pp)->sibling;
    }
}

devfs_node_t *devfs_lookup(const char *name, devfs_node_t *dir)
{
    if (!dir || dir->type != DEVFS_TYPE_DIR) return NULL;
    for (devfs_node_t *n = dir->children; n; n = n->sibling)
        if (strncmp(n->name, name, DEVFS_NAME_MAX) == 0)
            return n;
    return NULL;
}

devfs_node_t *devfs_lookup_path(const char *path)
{
    if (!path || !g_devfs.root) return NULL;

    if (path[0] == '/') {
        path++;
        if (strncmp(path, "dev/", 4) == 0)
            path += 4;
    }

    devfs_node_t *cur = g_devfs.root;

    char comp[DEVFS_NAME_MAX];
    while (*path) {
        const char *slash = path;
        while (*slash && *slash != '/') slash++;
        size_t len = (size_t)(slash - path);
        if (len == 0) { path = slash + 1; continue; }
        if (len >= DEVFS_NAME_MAX) return NULL;
        memcpy(comp, path, len);
        comp[len] = '\0';
        cur = devfs_lookup(comp, cur);
        if (!cur) return NULL;
        path = (*slash) ? slash + 1 : slash;
    }
    return cur;
}

static int register_node(uint8_t type, uint32_t major, uint32_t minor,
                         const char *name, uint32_t cls,
                         devfs_ops_t *ops, void *priv)
{
    if (!g_devfs.mounted) return -1;
    if (!name || !ops)    return -1;

    devfs_node_t *parent = g_devfs.root;
    const char   *base   = name;
    const char   *slash  = NULL;
    for (const char *p = name; *p; p++) {
        if (*p == '/') { slash = p; break; }
    }

    if (slash) {

        char dirname[DEVFS_NAME_MAX];
        size_t dlen = (size_t)(slash - name);
        if (dlen >= DEVFS_NAME_MAX) return -1;
        memcpy(dirname, name, dlen);
        dirname[dlen] = '\0';
        parent = devfs_lookup(dirname, g_devfs.root);
        if (!parent) {
            devfs_mkdir(dirname);
            parent = devfs_lookup(dirname, g_devfs.root);
            if (!parent) return -1;
        }
        base = slash + 1;
    }

    if (devfs_lookup(base, parent)) {
        char msg[80];
        ksnprintf(msg, sizeof(msg), "duplicate registration: %s", name);
        klog_warn("devfs", msg);
        return -1;
    }

    devfs_node_t *n = devfs_alloc_node();
    if (!n) return -1;

    strncpy(n->name, base, DEVFS_NAME_MAX - 1);
    n->dev         = MKDEV(major, minor);
    n->type        = type;
    n->tykid_class = cls;
    n->ops         = ops;
    n->priv        = priv;
    n->mode        = DEVFS_MODE_RUSR | DEVFS_MODE_WUSR
                   | DEVFS_MODE_RGRP | DEVFS_MODE_ROTH;

    if (devfs_tykid_register_node(n) < 0) {
        devfs_free_node(n);
        return -1;
    }

    spin_lock(&g_devfs.lock);
    devfs_link_child(parent, n);
    spin_unlock(&g_devfs.lock);

    return 0;
}

int devfs_register_cdev(uint32_t maj, uint32_t min, const char *name,
                        uint32_t cls, devfs_ops_t *ops, void *priv)
{
    return register_node(DEVFS_TYPE_CDEV, maj, min, name, cls, ops, priv);
}

int devfs_register_bdev(uint32_t maj, uint32_t min, const char *name,
                        uint32_t cls, devfs_ops_t *ops, void *priv)
{
    return register_node(DEVFS_TYPE_BDEV, maj, min, name, cls, ops, priv);
}

int devfs_unregister(uint32_t maj, uint32_t min)
{
    dev_t target = MKDEV(maj, min);
    spin_lock(&g_devfs.lock);
    for (uint32_t i = 0; i < DEVFS_MAX_NODES; i++) {
        devfs_node_t *n = &g_devfs.pool[i];
        if (!n->_inuse || n->type == DEVFS_TYPE_DIR) continue;
        if (n->dev == target) {
            if (n->parent) devfs_unlink_child(n->parent, n);
            spin_unlock(&g_devfs.lock);
            devfs_free_node(n);
            return 0;
        }
    }
    spin_unlock(&g_devfs.lock);
    return -1;
}

int devfs_mkdir(const char *dirname)
{
    if (!g_devfs.mounted || !dirname) return -1;
    if (devfs_lookup(dirname, g_devfs.root)) return 0;

    devfs_node_t *d = devfs_alloc_node();
    if (!d) return -1;
    strncpy(d->name, dirname, DEVFS_NAME_MAX - 1);
    d->type        = DEVFS_TYPE_DIR;
    d->tykid_class = DEVFS_CLASS_MEM;
    d->mode        = DEVFS_MODE_RUSR | DEVFS_MODE_RGRP | DEVFS_MODE_ROTH;

    spin_lock(&g_devfs.lock);
    devfs_link_child(g_devfs.root, d);
    spin_unlock(&g_devfs.lock);
    return 0;
}

int devfs__register_node(devfs_node_t *n)
{
    if (!g_devfs.root) return -1;
    spin_lock(&g_devfs.lock);
    devfs_link_child(g_devfs.root, n);
    spin_unlock(&g_devfs.lock);
    return 0;
}
