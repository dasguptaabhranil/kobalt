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
#include "sysfs_vfs.h"
#include <sysfs.h>
#include <sysfs_kobalt.h>
#include <kmalloc.h>
#include <string.h>
#include <kernel.h>

typedef struct {
    sysfs_obj_t  *obj;
    sysfs_attr_t *attr;
} sysh_t;

static int sysfs_walk(const char *rel, sysfs_obj_t **obj_out,
                      sysfs_attr_t **attr_out)
{
    sysfs_obj_t *cur = sysfs_root();
    sysfs_obj_ref(cur);
    *attr_out = NULL;

    if (rel[0] == '\0') { *obj_out = cur; return 0; }

    const char *p = rel;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;

        const char *end = p;
        while (*end && *end != '/') end++;

        char seg[SYSFS_NAME_MAX];
        size_t len = (size_t)(end - p);
        if (len >= SYSFS_NAME_MAX) { sysfs_obj_unref(cur); return -1; }
        memcpy(seg, p, len);
        seg[len] = '\0';

        sysfs_obj_t  *next = NULL;
        sysfs_attr_t *attr = NULL;

        if (sysfs_dir_lookup(cur, seg, &next, &attr) < 0) {
            sysfs_obj_unref(cur);
            return -1;
        }
        sysfs_obj_unref(cur);
        cur = next;

        if (attr) {
            if (*end != '\0') { sysfs_obj_unref(cur); return -1; }
            *obj_out  = cur;
            *attr_out = attr;
            return 0;
        }
        p = end;
    }

    *obj_out = cur;
    return 0;
}

static int sysfs_vop_open(const char *rel, int flags, uint32_t mode, void **out)
{
    (void)flags; (void)mode;
    sysfs_obj_t  *obj  = NULL;
    sysfs_attr_t *attr = NULL;
    if (sysfs_walk(rel, &obj, &attr) < 0) return -2;

    sysh_t *h = kmalloc(sizeof *h);
    if (!h) { sysfs_obj_unref(obj); return -12; }
    h->obj  = obj;
    h->attr = attr;
    *out = h;
    return 0;
}

static void sysfs_vop_close(void *priv)
{
    sysh_t *h = priv;
    sysfs_obj_unref(h->obj);
    kfree(h);
}

static int sysfs_vop_read(void *priv, void *buf,
                           uint64_t off, size_t len, size_t *got)
{
    sysh_t *h = priv;
    if (!h->attr) return -9;
    ssize_t r = sysfs_attr_read(h->obj, h->attr, buf, (size_t)off, len);
    if (r < 0) return -5;
    *got = (size_t)r;
    return 0;
}

static int sysfs_vop_write(void *priv, const void *buf,
                            uint64_t off, size_t len, size_t *put)
{
    (void)off;
    sysh_t *h = priv;
    if (!h->attr) return -9;
    ssize_t r = sysfs_attr_write(h->obj, h->attr, buf, len);
    if (r < 0) return (int)r;
    *put = len;
    return 0;
}

static void stat_fill(vfs_stat_t *st, sysfs_obj_t *obj, sysfs_attr_t *attr)
{
    memset(st, 0, sizeof *st);
    st->nlink = 1;
    st->mode  = attr ? ((1u << 12) | (attr->mode & 0777u))
                     : ((4u << 12) | 0555u);
    (void)obj;
}

static int sysfs_vop_stat(const char *rel, vfs_stat_t *st)
{
    sysfs_obj_t  *obj  = NULL;
    sysfs_attr_t *attr = NULL;
    if (sysfs_walk(rel, &obj, &attr) < 0) return -2;
    stat_fill(st, obj, attr);
    sysfs_obj_unref(obj);
    return 0;
}

static int sysfs_vop_fstat(void *priv, vfs_stat_t *st)
{
    sysh_t *h = priv;
    stat_fill(st, h->obj, h->attr);
    return 0;
}

static int sysfs_vop_readdir(const char *rel, uint64_t idx, vfs_dirent_t *de)
{
    sysfs_obj_t  *obj  = NULL;
    sysfs_attr_t *attr = NULL;
    if (sysfs_walk(rel, &obj, &attr) < 0) return -2;
    if (attr) { sysfs_obj_unref(obj); return -20; }

    char name[SYSFS_NAME_MAX + 1];
    int  type;
    int  r = sysfs_dir_readdir(obj, (int)idx, name, &type);
    sysfs_obj_unref(obj);
    if (r < 0) return -2;

    memset(de, 0, sizeof *de);
    de->type = (type == 2) ? VFS_DT_DIR : VFS_DT_REG;
    size_t nl = strlen(name);
    if (nl >= sizeof de->name) nl = sizeof de->name - 1;
    memcpy(de->name, name, nl);
    de->name[nl] = '\0';
    return 0;
}

static const vfs_file_ops_t s_sysfs_ops = {
    .open     = sysfs_vop_open,
    .close    = sysfs_vop_close,
    .read     = sysfs_vop_read,
    .write    = sysfs_vop_write,
    .truncate = NULL,
    .stat     = sysfs_vop_stat,
    .lstat    = sysfs_vop_stat,
    .fstat    = sysfs_vop_fstat,
    .unlink   = NULL,
    .mkdir    = NULL,
    .rmdir    = NULL,
    .readdir  = sysfs_vop_readdir,
};

static int sysfs_mount_noop(void)  { return 0; }
static int sysfs_umount_noop(void) { return 0; }

void sysfs_vfs_register(void)
{
    sysfs_init();
    sysfs_kobalt_init();

    int id = vfs_register_driver("sysfs", "/sys",
                                 sysfs_mount_noop, sysfs_umount_noop);
    if (id < 0) { klog_fail("sysfs", "register failed"); return; }
    vfs_set_driver_ops(id, &s_sysfs_ops);
    klog_ok("sysfs", "/sys registered in VFS");
}
