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

int sysfs_dir_readdir(sysfs_obj_t *obj, int idx, char *name_out, int *type_out)
{
    unsigned long fl = spin_lock_irqsave(&obj->lock);

    int total = obj->nattrs + obj->nchildren;
    if (idx < 0 || idx >= total) {
        spin_unlock_irqrestore(&obj->lock, fl);
        return -1;
    }

    const char *n;
    if (idx < obj->nattrs) {
        n = obj->attrs[idx]->name;
        *type_out = 1;
    } else {
        n = obj->children[idx - obj->nattrs]->name;
        *type_out = 2;
    }

    size_t len = strlen(n);
    memcpy(name_out, n, len);
    name_out[len] = '\0';

    spin_unlock_irqrestore(&obj->lock, fl);
    return 0;
}

int sysfs_dir_lookup(sysfs_obj_t *dir, const char *name,
                     sysfs_obj_t **obj_out, sysfs_attr_t **attr_out)
{
    unsigned long fl = spin_lock_irqsave(&dir->lock);

    for (int i = 0; i < dir->nattrs; i++) {
        if (strcmp(dir->attrs[i]->name, name) == 0) {
            *obj_out  = dir;
            *attr_out = dir->attrs[i];
            sysfs_obj_ref(dir);
            spin_unlock_irqrestore(&dir->lock, fl);
            return 0;
        }
    }

    for (int i = 0; i < dir->nchildren; i++) {
        if (strcmp(dir->children[i]->name, name) == 0) {
            *obj_out  = dir->children[i];
            *attr_out = NULL;
            sysfs_obj_ref(dir->children[i]);
            spin_unlock_irqrestore(&dir->lock, fl);
            return 0;
        }
    }

    spin_unlock_irqrestore(&dir->lock, fl);
    return -1;
}
