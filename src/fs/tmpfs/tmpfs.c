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
#include <kernel.h>
#include <string.h>

extern uint32_t sys_now(void);

uint32_t tmpfs_now(void)
{
    return sys_now();
}

tmpfs_sb_t *tmpfs_sb_alloc(void)
{
    tmpfs_sb_t *sb = kmalloc(sizeof *sb);
    if (!sb) {
        klog_fail("tmpfs", "sb kmalloc failed");
        return NULL;
    }
    memset(sb, 0, sizeof *sb);
    sb->ino_next = 1;

    tmpfs_inode_t *root = tmpfs_inode_alloc(sb, TMPFS_T_DIR, 0755);
    if (!root) {
        kfree(sb);
        return NULL;
    }
    root->nlink = 2;
    sb->root = root;

    klog_ok("tmpfs", "mounted  root=ino:1");
    return sb;
}

void tmpfs_sb_destroy(tmpfs_sb_t *sb)
{
    for (unsigned i = 0; i < TMPFS_HTAB_SZ; i++) {
        tmpfs_inode_t *ip = sb->htab[i];
        while (ip) {
            tmpfs_inode_t *nx = ip->ht_next;
            tmpfs_file_freepages(ip);
            tmpfs_dirent_t *de = ip->de_head;
            while (de) {
                tmpfs_dirent_t *dn = de->next;
                kfree(de);
                de = dn;
            }
            kfree(ip);
            ip = nx;
        }
    }
    kfree(sb);
    klog_ok("tmpfs", "unmounted");
}
