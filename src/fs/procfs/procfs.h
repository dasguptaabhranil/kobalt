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

#ifndef PROCFS_H
#define PROCFS_H

#include <stddef.h>
#include <stdint.h>
#include <spinlock.h>

#ifndef __ssize_t_defined
#define __ssize_t_defined
typedef long ssize_t;
#endif

#define PROC_NAME_MAX    64
#define PROC_BUF_MAX     4096

#define PROC_INO_ROOT    2
#define PROC_INO_TID_BASE 0x10000u

#define PROC_DT_FILE     0
#define PROC_DT_DIR      1

typedef ssize_t (*proc_fill_t)(char *buf, size_t sz, void *arg);

typedef struct proc_node {
    uint32_t          ino;
    uint8_t           type;
    uint8_t           transient;
    char              name[PROC_NAME_MAX];
    proc_fill_t       fill;
    void             *arg;
    struct proc_node *parent;
    struct proc_node *child;
    struct proc_node *next;
} proc_node_t;

typedef struct {
    proc_node_t *root;
    spinlock_t   lock;
    uint32_t     nxt;
    uint32_t     n_static;
} procfs_t;

procfs_t    *procfs_init(void);
proc_node_t *procfs_mkdir(procfs_t *pfs, proc_node_t *par, const char *name);
proc_node_t *procfs_mkfile(procfs_t *pfs, proc_node_t *par, const char *name,
                            proc_fill_t fn, void *arg);
proc_node_t *procfs_lookup(proc_node_t *dir, const char *name);
proc_node_t *procfs_by_ino(procfs_t *pfs, uint32_t ino);
proc_node_t *procfs_nth_child(proc_node_t *dir, uint32_t n);
uint32_t     procfs_child_count(proc_node_t *dir);
ssize_t      procfs_read(proc_node_t *n, void *buf, uint64_t off, size_t len);
void         procfs_populate(procfs_t *pfs);
proc_node_t *procfs_open_tid_file(uint32_t tid, const char *fname);

extern procfs_t *g_procfs;

#endif
