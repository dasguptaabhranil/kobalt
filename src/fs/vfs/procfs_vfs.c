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

#include "vfs.h"
#include "procfs_vfs.h"
#include "../procfs/procfs.h"
#include <kernel.h>
#include <kmalloc.h>
#include <sched.h>
#include <string.h>

static proc_node_t *path_resolve(const char *rel)
{
    if (!g_procfs) return NULL;
    if (rel[0] == '\0') return g_procfs->root;

    const char *sl = rel;
    while (*sl && *sl != '/') sl++;

    if (*sl == '\0') return procfs_lookup(g_procfs->root, rel);

    size_t dlen = (size_t)(sl - rel);
    char dname[PROC_NAME_MAX];
    if (dlen >= PROC_NAME_MAX) return NULL;
    memcpy(dname, rel, dlen);
    dname[dlen] = '\0';

    proc_node_t *dir = procfs_lookup(g_procfs->root, dname);
    if (dir) return procfs_lookup(dir, sl + 1);

    uint32_t tid = 0;
    for (size_t i = 0; dname[i]; i++) {
        if (dname[i] < '0' || dname[i] > '9') return NULL;
        tid = tid * 10 + (uint32_t)(dname[i] - '0');
    }
    if (!sched_get_thread_by_tid(tid)) return NULL;
    return procfs_open_tid_file(tid, sl + 1);
}

static int proc_vop_open(const char *rel, int flags, uint32_t mode, void **out)
{
    (void)flags; (void)mode;
    proc_node_t *n = path_resolve(rel);
    if (!n) return -2;
    *out = n;
    return 0;
}

static void proc_vop_close(void *priv)
{
    proc_node_t *n = priv;
    if (n && n->transient) kfree(n);
}

static int proc_vop_read(void *priv, void *buf,
                          uint64_t off, size_t len, size_t *got)
{
    ssize_t r = procfs_read((proc_node_t *)priv, buf, off, len);
    if (r < 0) return -5;
    *got = (size_t)r;
    return 0;
}

static void fill_stat(vfs_stat_t *st, proc_node_t *n)
{
    memset(st, 0, sizeof *st);
    st->ino   = n->ino;
    st->nlink = 1;
    st->mode  = (n->type == PROC_DT_DIR)
                ? ((4u << 12) | 0555u)
                : ((8u << 12) | 0444u);
}

static int proc_vop_stat(const char *rel, vfs_stat_t *st)
{
    proc_node_t *n = path_resolve(rel);
    if (!n) return -2;
    fill_stat(st, n);
    if (n->transient) kfree(n);
    return 0;
}

static int proc_vop_fstat(void *priv, vfs_stat_t *st)
{
    fill_stat(st, (proc_node_t *)priv);
    return 0;
}

static int proc_vop_readdir(const char *rel, uint64_t idx, vfs_dirent_t *de)
{
    if (!g_procfs) return -2;

    proc_node_t *dir;
    if (rel[0] == '\0') {
        dir = g_procfs->root;
    } else {
        dir = procfs_lookup(g_procfs->root, rel);
        if (!dir || dir->type != PROC_DT_DIR) return -20;
    }

    uint64_t n_static = procfs_child_count(dir);

    if (idx < n_static) {
        proc_node_t *child = procfs_nth_child(dir, (uint32_t)idx);
        if (!child) return -2;
        memset(de, 0, sizeof *de);
        de->ino  = child->ino;
        de->type = (child->type == PROC_DT_DIR) ? VFS_DT_DIR : VFS_DT_REG;
        size_t nl = strlen(child->name);
        if (nl >= sizeof de->name) nl = sizeof de->name - 1;
        memcpy(de->name, child->name, nl);
        de->name[nl] = '\0';
        return 0;
    }

    if (dir != g_procfs->root) return -2;

    uint64_t dyn_idx  = idx - n_static;
    uint32_t nthreads = sched_thread_count();
    if (dyn_idx >= (uint64_t)nthreads) return -2;

    sched_thread_t *t = sched_get_thread((uint32_t)dyn_idx);
    if (!t) return -2;

    uint32_t tid = sched_thread_get_tid(t);
    memset(de, 0, sizeof *de);
    de->ino  = PROC_INO_TID_BASE + tid;
    de->type = VFS_DT_DIR;
    ksnprintf(de->name, sizeof de->name, "%u", tid);
    return 0;
}

static const vfs_file_ops_t s_procfs_ops = {
    .open     = proc_vop_open,
    .close    = proc_vop_close,
    .read     = proc_vop_read,
    .write    = NULL,
    .truncate = NULL,
    .stat     = proc_vop_stat,
    .lstat    = proc_vop_stat,
    .fstat    = proc_vop_fstat,
    .unlink   = NULL,
    .mkdir    = NULL,
    .rmdir    = NULL,
    .readdir  = proc_vop_readdir,
};

static int procfs_mount_noop(void)  { return 0; }
static int procfs_umount_noop(void) { return 0; }

void procfs_vfs_register(void)
{
    g_procfs = procfs_init();
    if (!g_procfs) { klog_fail("procfs", "init failed"); return; }
    procfs_populate(g_procfs);

    int id = vfs_register_driver("procfs", "/proc",
                                 procfs_mount_noop, procfs_umount_noop);
    if (id < 0) { klog_fail("procfs", "register failed"); return; }
    vfs_set_driver_ops(id, &s_procfs_ops);
    klog_ok("procfs", "/proc registered in VFS");
}
