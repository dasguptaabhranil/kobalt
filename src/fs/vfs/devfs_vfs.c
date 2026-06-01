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
#include "devfs_vfs.h"
#include "../../fs/vfs/vfs.h"
#include "../../inc/string.h"
#include "../../inc/kernel.h"

typedef struct {
    char    name[DEVFS_NAME_MAX];
    dev_t   dev;
    uint8_t type;
} devfs_dirent_t;

int devfs_dir_readdir(devfs_node_t *dir, devfs_dirent_t *de, int idx)
{
    if (!dir || dir->type != DEVFS_TYPE_DIR || !de) return -1;

    if (idx == 0) {
        memcpy(de->name, ".", 2);
        de->dev = dir->dev; de->type = DEVFS_TYPE_DIR;
        return 0;
    }
    if (idx == 1) {
        memcpy(de->name, "..", 3);
        de->dev  = dir->parent ? dir->parent->dev : dir->dev;
        de->type = DEVFS_TYPE_DIR;
        return 0;
    }

    int ri = idx - 2, i = 0;
    for (devfs_node_t *n = dir->children; n; n = n->sibling, i++) {
        if (i == ri) {
            strncpy(de->name, n->name, DEVFS_NAME_MAX - 1);
            de->name[DEVFS_NAME_MAX - 1] = '\0';
            de->dev  = n->dev;
            de->type = n->type;
            return 0;
        }
    }
    return -1;
}

int devfs_dir_count(devfs_node_t *dir)
{
    if (!dir || dir->type != DEVFS_TYPE_DIR) return 0;
    int n = 0;
    for (devfs_node_t *c = dir->children; c; c = c->sibling) n++;
    return n;
}

void devfs_dir_walk(devfs_node_t *dir,
                    void (*cb)(devfs_node_t *, void *), void *arg)
{
    if (!dir || !cb) return;
    for (devfs_node_t *n = dir->children; n; n = n->sibling) {
        cb(n, arg);
        if (n->type == DEVFS_TYPE_DIR) devfs_dir_walk(n, cb, arg);
    }
}

devfs_node_t *devfs_dir_find_dev(devfs_node_t *dir, dev_t target)
{
    for (devfs_node_t *n = dir->children; n; n = n->sibling) {
        if (n->type == DEVFS_TYPE_DIR) {
            devfs_node_t *f = devfs_dir_find_dev(n, target);
            if (f) return f;
        } else if (n->dev == target) return n;
    }
    return NULL;
}

devfs_node_t *devfs_dir_find_tag(devfs_node_t *dir, uint32_t tag)
{
    for (devfs_node_t *n = dir->children; n; n = n->sibling) {
        if (n->type == DEVFS_TYPE_DIR) {
            devfs_node_t *f = devfs_dir_find_tag(n, tag);
            if (f) return f;
        } else if (n->tykid_tag == tag) return n;
    }
    return NULL;
}

static int devfs_vop_open(const char *rel, int flags, uint32_t mode, void **out)
{
    (void)mode;
    devfs_node_t *node = devfs_lookup_path(rel);
    if (!node) return -2;
    if (node->type == DEVFS_TYPE_DIR) return -21;
    devfs_file_t *f = devfs_cdev_open(node, flags);
    if (!f) return -13;
    *out = f;
    return 0;
}

static void devfs_vop_close(void *priv)
{
    devfs_cdev_close((devfs_file_t *)priv);
}

static int devfs_vop_read(void *priv, void *buf,
                           uint64_t off, size_t len, size_t *got)
{
    devfs_file_t *f = priv;
    f->pos = off;
    ssize_t n = devfs_cdev_read(f, buf, len);
    if (n < 0) return (int)n;
    *got = (size_t)n;
    return 0;
}

static int devfs_vop_write(void *priv, const void *buf,
                            uint64_t off, size_t len, size_t *put)
{
    devfs_file_t *f = priv;
    f->pos = off;
    ssize_t n = devfs_cdev_write(f, buf, len);
    if (n < 0) return (int)n;
    *put = (size_t)n;
    return 0;
}

static int devfs_vop_stat(const char *rel, vfs_stat_t *st)
{
    devfs_node_t *node = rel[0] ? devfs_lookup_path(rel) : g_devfs.root;
    if (!node) return -2;
    memset(st, 0, sizeof *st);
    switch (node->type) {
    case DEVFS_TYPE_DIR:  st->mode = (4u << 12); break;
    case DEVFS_TYPE_CDEV: st->mode = (2u << 12); break;
    case DEVFS_TYPE_BDEV: st->mode = (6u << 12); break;
    default:              st->mode = (1u << 12); break;
    }
    st->mode  |= node->mode & 0777u;
    st->nlink  = 1;
    st->ino    = (uint64_t)(uintptr_t)node;
    return 0;
}

static int devfs_vop_fstat(void *priv, vfs_stat_t *st)
{
    devfs_file_t *f = priv;
    if (!f || !f->node) return -9;
    memset(st, 0, sizeof *st);
    st->mode  = (2u << 12) | (f->node->mode & 0777u);
    st->nlink = 1;
    st->ino   = (uint64_t)(uintptr_t)f->node;
    return 0;
}

static int devfs_vop_readdir(const char *rel, uint64_t idx, vfs_dirent_t *de)
{
    devfs_node_t *dir = rel[0] ? devfs_lookup_path(rel) : g_devfs.root;
    if (!dir) return -2;
    if (dir->type != DEVFS_TYPE_DIR) return -20;
    if (idx > (uint64_t)0x7fffffff) return -2;

    devfs_dirent_t dde;
    if (devfs_dir_readdir(dir, &dde, (int)idx) < 0) return -2;

    memset(de, 0, sizeof *de);
    de->ino  = (uint64_t)dde.dev;
    de->type = dde.type == DEVFS_TYPE_DIR ? VFS_DT_DIR : VFS_DT_REG;
    strncpy(de->name, dde.name, VFS_NAME_MAX);
    return 0;
}

static const vfs_file_ops_t s_devfs_vfs_ops = {
    .open     = devfs_vop_open,
    .close    = devfs_vop_close,
    .read     = devfs_vop_read,
    .write    = devfs_vop_write,
    .truncate = NULL,
    .stat     = devfs_vop_stat,
    .lstat    = devfs_vop_stat,
    .fstat    = devfs_vop_fstat,
    .unlink   = NULL,
    .mkdir    = NULL,
    .rmdir    = NULL,
    .readdir  = devfs_vop_readdir,
};

static int devfs_vfs_mount_noop(void)  { return 0; }
static int devfs_vfs_umount_noop(void) { return 0; }

void devfs_vfs_register(void)
{
    int id = vfs_register_driver("devfs", "/dev",
                                 devfs_vfs_mount_noop, devfs_vfs_umount_noop);
    if (id < 0) { klog_warn("devfs", "vfs_register_driver failed"); return; }
    vfs_set_driver_ops(id, &s_devfs_vfs_ops);
    klog_ok("devfs", "/dev registered in VFS");
}

void *devfs_vfs_open_path(const char *path, int flags)
{
    (void)path; (void)flags;
    return NULL;
}
