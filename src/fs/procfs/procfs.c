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

#include "procfs.h"
#include <kernel.h>
#include <kmalloc.h>
#include <string.h>

procfs_t *g_procfs;

static proc_node_t *node_alloc(procfs_t *pfs, const char *name, uint8_t type)
{
    proc_node_t *n = kmalloc(sizeof(*n));
    if (!n) return NULL;

    memset(n, 0, sizeof(*n));
    n->ino  = pfs->nxt++;
    n->type = type;

    size_t nl = strlen(name);
    if (nl >= PROC_NAME_MAX) nl = PROC_NAME_MAX - 1;
    memcpy(n->name, name, nl);
    n->name[nl] = '\0';

    return n;
}

static void dir_link(proc_node_t *dir, proc_node_t *child)
{
    child->parent = dir;
    if (!dir->child) { dir->child = child; return; }
    proc_node_t *p = dir->child;
    while (p->next) p = p->next;
    p->next = child;
}

procfs_t *procfs_init(void)
{
    procfs_t *pfs = kmalloc(sizeof(*pfs));
    if (!pfs) return NULL;
    memset(pfs, 0, sizeof(*pfs));
    pfs->nxt = PROC_INO_ROOT;

    proc_node_t *root = node_alloc(pfs, "/", PROC_DT_DIR);
    if (!root) { kfree(pfs); return NULL; }
    pfs->root = root;
    return pfs;
}

proc_node_t *procfs_mkdir(procfs_t *pfs, proc_node_t *par, const char *name)
{
    proc_node_t *n = node_alloc(pfs, name, PROC_DT_DIR);
    if (n && par) dir_link(par, n);
    return n;
}

proc_node_t *procfs_mkfile(procfs_t *pfs, proc_node_t *par, const char *name,
                            proc_fill_t fn, void *arg)
{
    proc_node_t *n = node_alloc(pfs, name, PROC_DT_FILE);
    if (!n) return NULL;
    n->fill = fn;
    n->arg  = arg;
    if (par) dir_link(par, n);
    return n;
}

proc_node_t *procfs_lookup(proc_node_t *dir, const char *name)
{
    if (!dir || dir->type != PROC_DT_DIR) return NULL;
    for (proc_node_t *c = dir->child; c; c = c->next)
        if (strcmp(c->name, name) == 0) return c;
    return NULL;
}

static proc_node_t *ino_walk(proc_node_t *n, uint32_t ino)
{
    if (!n) return NULL;
    if (n->ino == ino) return n;
    proc_node_t *r = ino_walk(n->child, ino);
    if (r) return r;
    return ino_walk(n->next, ino);
}

proc_node_t *procfs_by_ino(procfs_t *pfs, uint32_t ino)
{
    return ino_walk(pfs->root, ino);
}

proc_node_t *procfs_nth_child(proc_node_t *dir, uint32_t n)
{
    if (!dir || dir->type != PROC_DT_DIR) return NULL;
    uint32_t i = 0;
    for (proc_node_t *c = dir->child; c; c = c->next, i++)
        if (i == n) return c;
    return NULL;
}

uint32_t procfs_child_count(proc_node_t *dir)
{
    if (!dir || dir->type != PROC_DT_DIR) return 0;
    uint32_t n = 0;
    for (proc_node_t *c = dir->child; c; c = c->next) n++;
    return n;
}

ssize_t procfs_read(proc_node_t *n, void *buf, uint64_t off, size_t len)
{
    if (!n || n->type != PROC_DT_FILE || !n->fill) return -1;

    char *tmp = kmalloc(PROC_BUF_MAX);
    if (!tmp) return -1;

    ssize_t tot = n->fill(tmp, PROC_BUF_MAX, n->arg);
    if (tot <= 0) { kfree(tmp); return tot; }
    if (off >= (uint64_t)tot) { kfree(tmp); return 0; }

    size_t avail = (size_t)((uint64_t)tot - off);
    size_t copy  = avail < len ? avail : len;
    memcpy(buf, tmp + off, copy);
    kfree(tmp);
    return (ssize_t)copy;
}
