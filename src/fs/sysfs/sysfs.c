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

#include "sysfs.h"

static sysfs_obj_t sysfs_root_obj;

static int grow_children(sysfs_obj_t *obj)
{
    int nc = obj->cap ? obj->cap * 2 : SYSFS_CHILDREN_INIT;
    sysfs_obj_t **nb = kmalloc(sizeof(sysfs_obj_t *) * nc);
    if (!nb) return -1;
    if (obj->children) {
        memcpy(nb, obj->children, sizeof(sysfs_obj_t *) * obj->nchildren);
        kfree(obj->children);
    }
    obj->children = nb;
    obj->cap = nc;
    return 0;
}

static int grow_attrs(sysfs_obj_t *obj)
{
    int nc = obj->attr_cap ? obj->attr_cap * 2 : SYSFS_ATTRS_INIT;
    sysfs_attr_t **nb = kmalloc(sizeof(sysfs_attr_t *) * nc);
    if (!nb) return -1;
    if (obj->attrs) {
        memcpy(nb, obj->attrs, sizeof(sysfs_attr_t *) * obj->nattrs);
        kfree(obj->attrs);
    }
    obj->attrs = nb;
    obj->attr_cap = nc;
    return 0;
}

void sysfs_init(void)
{
    memset(&sysfs_root_obj, 0, sizeof(sysfs_root_obj));
    memcpy(sysfs_root_obj.name, "/", 2);
    sysfs_root_obj.refcnt = 1;
}

sysfs_obj_t *sysfs_root(void)
{
    return &sysfs_root_obj;
}

void sysfs_obj_ref(sysfs_obj_t *obj)
{
    __atomic_fetch_add(&obj->refcnt, 1, __ATOMIC_SEQ_CST);
}

void sysfs_obj_unref(sysfs_obj_t *obj)
{
    if (obj == &sysfs_root_obj) return;
    if (__atomic_fetch_sub(&obj->refcnt, 1, __ATOMIC_SEQ_CST) == 1)
        sysfs_obj_destroy(obj);
}

sysfs_obj_t *sysfs_obj_create(const char *name, sysfs_obj_t *parent)
{
    sysfs_obj_t *obj = kmalloc(sizeof(*obj));
    if (!obj) return NULL;
    memset(obj, 0, sizeof(*obj));

    size_t nl = strlen(name);
    if (nl >= SYSFS_NAME_MAX) nl = SYSFS_NAME_MAX - 1;
    memcpy(obj->name, name, nl);
    obj->refcnt = 1;

    if (parent) {
        unsigned long fl = spin_lock_irqsave(&parent->lock);
        if (parent->nchildren >= parent->cap && grow_children(parent) < 0) {
            spin_unlock_irqrestore(&parent->lock, fl);
            kfree(obj);
            return NULL;
        }
        parent->children[parent->nchildren++] = obj;
        obj->parent = parent;
        sysfs_obj_ref(parent);
        spin_unlock_irqrestore(&parent->lock, fl);
    }
    return obj;
}

void sysfs_obj_destroy(sysfs_obj_t *obj)
{
    if (!obj || obj == &sysfs_root_obj) return;

    unsigned long fl = spin_lock_irqsave(&obj->lock);
    for (int i = 0; i < obj->nchildren; i++)
        sysfs_obj_unref(obj->children[i]);
    if (obj->children) kfree(obj->children);
    if (obj->attrs)    kfree(obj->attrs);
    spin_unlock_irqrestore(&obj->lock, fl);

    if (obj->parent) {
        sysfs_obj_t *p = obj->parent;
        unsigned long pf = spin_lock_irqsave(&p->lock);
        for (int i = 0; i < p->nchildren; i++) {
            if (p->children[i] == obj) {
                p->children[i] = p->children[--p->nchildren];
                break;
            }
        }
        spin_unlock_irqrestore(&p->lock, pf);
        sysfs_obj_unref(p);
    }

    kfree(obj);
}

int sysfs_add_attr(sysfs_obj_t *obj, sysfs_attr_t *attr)
{
    unsigned long fl = spin_lock_irqsave(&obj->lock);
    if (obj->nattrs >= obj->attr_cap && grow_attrs(obj) < 0) {
        spin_unlock_irqrestore(&obj->lock, fl);
        return -1;
    }
    obj->attrs[obj->nattrs++] = attr;
    spin_unlock_irqrestore(&obj->lock, fl);
    return 0;
}

sysfs_obj_t *sysfs_lookup(sysfs_obj_t *base, const char *name)
{
    unsigned long fl = spin_lock_irqsave(&base->lock);
    for (int i = 0; i < base->nchildren; i++) {
        if (strcmp(base->children[i]->name, name) == 0) {
            sysfs_obj_t *f = base->children[i];
            sysfs_obj_ref(f);
            spin_unlock_irqrestore(&base->lock, fl);
            return f;
        }
    }
    spin_unlock_irqrestore(&base->lock, fl);
    return NULL;
}

sysfs_obj_t *sysfs_mkdir_p(const char *path, sysfs_obj_t *base)
{
    char seg[SYSFS_NAME_MAX];
    sysfs_obj_t *cur = base ? base : &sysfs_root_obj;
    const char *p = path;

    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;

        const char *end = p;
        while (*end && *end != '/') end++;

        size_t len = (size_t)(end - p);
        if (len >= SYSFS_NAME_MAX) return NULL;
        memcpy(seg, p, len);
        seg[len] = '\0';

        sysfs_obj_t *next = sysfs_lookup(cur, seg);
        if (!next) {
            next = sysfs_obj_create(seg, cur);
            if (!next) return NULL;
        } else {
            sysfs_obj_unref(next);
        }
        cur = next;
        p = end;
    }
    return cur;
}
