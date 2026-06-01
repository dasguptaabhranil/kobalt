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

#include "tmpfs.h"
#include <kmalloc.h>
#include <string.h>

#define nm_eq(a, b)  (!strncmp((a), (b), TMPFS_NAME_MAX + 1))

tmpfs_inode_t *tmpfs_dir_lookup(tmpfs_sb_t *sb, tmpfs_inode_t *dir,
                                const char *name)
{
    if (dir->type != TMPFS_T_DIR) return NULL;

    spin_lock(&dir->lock);
    uint64_t found = 0;
    for (tmpfs_dirent_t *de = dir->de_head; de; de = de->next) {
        if (nm_eq(de->name, name)) { found = de->ino; break; }
    }
    spin_unlock(&dir->lock);

    return found ? tmpfs_inode_get(sb, found) : NULL;
}

int tmpfs_dir_link(tmpfs_inode_t *dir, const char *name, uint64_t ino)
{
    if (dir->type != TMPFS_T_DIR) return TMPFS_ENOTDIR;

    size_t nlen = strnlen(name, TMPFS_NAME_MAX + 1);
    if (!nlen || nlen > TMPFS_NAME_MAX) return TMPFS_EINVAL;

    tmpfs_dirent_t *de = kmalloc(sizeof *de);
    if (!de) return TMPFS_ENOMEM;

    de->ino = ino;
    memcpy(de->name, name, nlen + 1);

    spin_lock(&dir->lock);

    for (tmpfs_dirent_t *d = dir->de_head; d; d = d->next) {
        if (nm_eq(d->name, name)) {
            spin_unlock(&dir->lock);
            kfree(de);
            return TMPFS_EEXIST;
        }
    }

    de->next      = dir->de_head;
    dir->de_head  = de;
    dir->de_count++;
    dir->mtime    = tmpfs_now();
    spin_unlock(&dir->lock);
    return TMPFS_OK;
}

int tmpfs_dir_unlink(tmpfs_inode_t *dir, const char *name)
{
    if (dir->type != TMPFS_T_DIR) return TMPFS_ENOTDIR;

    spin_lock(&dir->lock);
    tmpfs_dirent_t **pp = &dir->de_head;
    while (*pp) {
        if (nm_eq((*pp)->name, name)) {
            tmpfs_dirent_t *del = *pp;
            *pp = del->next;
            dir->de_count--;
            dir->mtime = tmpfs_now();
            spin_unlock(&dir->lock);
            kfree(del);
            return TMPFS_OK;
        }
        pp = &(*pp)->next;
    }
    spin_unlock(&dir->lock);
    return TMPFS_ENOENT;
}

int tmpfs_dir_iterate(tmpfs_inode_t *dir, uint64_t idx,
                      uint64_t *ino_out, char *name_out)
{
    if (dir->type != TMPFS_T_DIR) return TMPFS_ENOTDIR;

    spin_lock(&dir->lock);
    tmpfs_dirent_t *de = dir->de_head;
    uint64_t i = 0;
    while (de) {
        if (i == idx) {
            *ino_out = de->ino;
            size_t n = strnlen(de->name, TMPFS_NAME_MAX);
            memcpy(name_out, de->name, n + 1);
            spin_unlock(&dir->lock);
            return TMPFS_OK;
        }
        de = de->next;
        i++;
    }
    spin_unlock(&dir->lock);
    return TMPFS_ENOENT;
}
