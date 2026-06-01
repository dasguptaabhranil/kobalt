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

#ifndef TMPFS_H
#define TMPFS_H

#include <stdint.h>
#include <stddef.h>
#include <spinlock.h>

#define TMPFS_PAGE_SIZE   4096u
#define TMPFS_L2_SZ       64u
#define TMPFS_L1_SZ       64u
#define TMPFS_MAX_SZ      ((uint64_t)TMPFS_L1_SZ * TMPFS_L2_SZ * TMPFS_PAGE_SIZE)

#define TMPFS_NAME_MAX    255u
#define TMPFS_HTAB_SZ     512u

#define TMPFS_T_REG   1u
#define TMPFS_T_DIR   2u
#define TMPFS_T_LNK   4u

#define TMPFS_OK          0
#define TMPFS_ENOMEM    (-12)
#define TMPFS_ENOENT     (-2)
#define TMPFS_EEXIST    (-17)
#define TMPFS_ENOTDIR   (-20)
#define TMPFS_EISDIR    (-21)
#define TMPFS_EFBIG     (-27)
#define TMPFS_EINVAL    (-22)
#define TMPFS_ENOTEMPTY (-39)
#define TMPFS_ENOSYS    (-38)

typedef struct tmpfs_sb     tmpfs_sb_t;
typedef struct tmpfs_inode  tmpfs_inode_t;
typedef struct tmpfs_dirent tmpfs_dirent_t;

struct tmpfs_dirent {
    uint64_t         ino;
    tmpfs_dirent_t  *next;
    char             name[TMPFS_NAME_MAX + 1];
};

struct tmpfs_inode {
    uint64_t          ino;
    uint16_t          type;
    uint16_t          perm;
    uint32_t          nlink;
    uint64_t          size;
    uint32_t          atime, mtime, ctime;
    spinlock_t        lock;

    uint8_t         **pg_l1[TMPFS_L1_SZ];

    tmpfs_dirent_t   *de_head;
    uint32_t          de_count;

    tmpfs_inode_t    *ht_next;
};

struct tmpfs_sb {
    spinlock_t      lock;
    uint64_t        ino_next;
    uint32_t        nr_inodes;
    uint32_t        nr_pages;
    tmpfs_inode_t  *htab[TMPFS_HTAB_SZ];
    tmpfs_inode_t  *root;
};

uint32_t       tmpfs_now(void);
tmpfs_sb_t    *tmpfs_sb_alloc(void);
void           tmpfs_sb_destroy(tmpfs_sb_t *sb);

tmpfs_inode_t *tmpfs_inode_alloc(tmpfs_sb_t *sb, uint16_t type, uint16_t perm);
void           tmpfs_inode_free(tmpfs_sb_t *sb, tmpfs_inode_t *ip);
tmpfs_inode_t *tmpfs_inode_get(tmpfs_sb_t *sb, uint64_t ino);

int   tmpfs_file_read    (tmpfs_inode_t *ip, void *buf, uint64_t off, size_t len, size_t *got);
int   tmpfs_file_write   (tmpfs_inode_t *ip, const void *buf, uint64_t off, size_t len, size_t *put);
int   tmpfs_file_truncate(tmpfs_inode_t *ip, uint64_t newsz);
void  tmpfs_file_freepages(tmpfs_inode_t *ip);

tmpfs_inode_t *tmpfs_dir_lookup (tmpfs_sb_t *sb, tmpfs_inode_t *dir, const char *name);
int            tmpfs_dir_link   (tmpfs_inode_t *dir, const char *name, uint64_t ino);
int            tmpfs_dir_unlink (tmpfs_inode_t *dir, const char *name);
int            tmpfs_dir_iterate(tmpfs_inode_t *dir, uint64_t idx,
                                 uint64_t *ino_out, char *name_out);

#endif
