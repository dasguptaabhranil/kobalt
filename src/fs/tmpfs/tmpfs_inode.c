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

static inline uint32_t ht_bucket(uint64_t ino)
{

    return (uint32_t)((ino * 0x9E3779B97F4A7C15ULL) >> 55) & (TMPFS_HTAB_SZ - 1);
}

tmpfs_inode_t *tmpfs_inode_alloc(tmpfs_sb_t *sb, uint16_t type, uint16_t perm)
{
    tmpfs_inode_t *ip = kmalloc(sizeof *ip);
    if (!ip) return NULL;
    memset(ip, 0, sizeof *ip);

    ip->type  = type;
    ip->perm  = perm;
    ip->nlink = 1;
    uint32_t now = tmpfs_now();
    ip->atime = ip->mtime = ip->ctime = now;

    spin_lock(&sb->lock);
    ip->ino          = sb->ino_next++;
    uint32_t h       = ht_bucket(ip->ino);
    ip->ht_next      = sb->htab[h];
    sb->htab[h]      = ip;
    sb->nr_inodes++;
    spin_unlock(&sb->lock);

    return ip;
}

void tmpfs_inode_free(tmpfs_sb_t *sb, tmpfs_inode_t *ip)
{
    uint32_t h = ht_bucket(ip->ino);

    spin_lock(&sb->lock);
    tmpfs_inode_t **pp = &sb->htab[h];
    while (*pp && *pp != ip)
        pp = &(*pp)->ht_next;
    if (*pp) {
        *pp = ip->ht_next;
        sb->nr_inodes--;
    }
    spin_unlock(&sb->lock);

    tmpfs_file_freepages(ip);

    tmpfs_dirent_t *de = ip->de_head;
    while (de) {
        tmpfs_dirent_t *dn = de->next;
        kfree(de);
        de = dn;
    }

    kfree(ip);
}

tmpfs_inode_t *tmpfs_inode_get(tmpfs_sb_t *sb, uint64_t ino)
{
    uint32_t h = ht_bucket(ino);

    spin_lock(&sb->lock);
    tmpfs_inode_t *ip = sb->htab[h];
    while (ip && ip->ino != ino)
        ip = ip->ht_next;
    spin_unlock(&sb->lock);

    return ip;
}
