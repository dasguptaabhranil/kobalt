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

#ifndef _INC_SYSFS_H
#define _INC_SYSFS_H

#include <stddef.h>
#include <spinlock.h>

#ifndef __ssize_t_defined
#define __ssize_t_defined
typedef long ssize_t;
#endif

#define SYSFS_NAME_MAX  64
#define SYSFS_PAGE_SIZE 4096

typedef struct sysfs_obj  sysfs_obj_t;
typedef struct sysfs_attr sysfs_attr_t;

struct sysfs_attr {
    const char *name;
    unsigned    mode;
    int (*show)(sysfs_obj_t *, char *, size_t);
    int (*store)(sysfs_obj_t *, const char *, size_t);
};

struct sysfs_obj {
    char            name[SYSFS_NAME_MAX];
    sysfs_obj_t    *parent;
    sysfs_obj_t   **children;
    int             nchildren;
    int             cap;
    sysfs_attr_t  **attrs;
    int             nattrs;
    int             attr_cap;
    void           *priv;
    spinlock_t      lock;
    volatile int    refcnt;
};

#define SYSFS_ATTR_RO(_name, _show) \
    static sysfs_attr_t attr_##_name = { \
        .name  = #_name, \
        .mode  = 0444, \
        .show  = (_show), \
        .store = NULL, \
    }

#define SYSFS_ATTR_RW(_name, _show, _store) \
    static sysfs_attr_t attr_##_name = { \
        .name  = #_name, \
        .mode  = 0644, \
        .show  = (_show), \
        .store = (_store), \
    }

void         sysfs_init(void);
sysfs_obj_t *sysfs_root(void);
sysfs_obj_t *sysfs_obj_create(const char *name, sysfs_obj_t *parent);
void         sysfs_obj_destroy(sysfs_obj_t *obj);
int          sysfs_add_attr(sysfs_obj_t *obj, sysfs_attr_t *attr);
sysfs_obj_t *sysfs_lookup(sysfs_obj_t *base, const char *name);
sysfs_obj_t *sysfs_mkdir_p(const char *path, sysfs_obj_t *base);
void         sysfs_obj_ref(sysfs_obj_t *obj);
void         sysfs_obj_unref(sysfs_obj_t *obj);

int     sysfs_dir_lookup(sysfs_obj_t *dir, const char *name,
                         sysfs_obj_t **obj_out, sysfs_attr_t **attr_out);
int     sysfs_dir_readdir(sysfs_obj_t *obj, int idx,
                          char *name_out, int *type_out);
ssize_t sysfs_attr_read(sysfs_obj_t *obj, sysfs_attr_t *attr,
                        void *buf, size_t off, size_t len);
ssize_t sysfs_attr_write(sysfs_obj_t *obj, sysfs_attr_t *attr,
                         const void *buf, size_t len);

#endif
