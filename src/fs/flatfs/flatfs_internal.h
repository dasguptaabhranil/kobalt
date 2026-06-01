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

#ifndef FLATFS_INTERNAL_H
#define FLATFS_INTERNAL_H

#include "flatfs.h"

#define FMEMSET(d,c,n)  __builtin_memset((d),(c),(n))
#define FMEMCPY(d,s,n)  __builtin_memcpy((d),(s),(n))
#define FMEMCMP(a,b,n)  __builtin_memcmp((a),(b),(n))

static inline size_t fstrlen(const char *s, size_t max)
{
    size_t i = 0;
    while (i < max && s[i]) i++;
    return i;
}

static inline int fstrncmp(const char *a, const char *b, size_t n)
{
    while (n--) {
        unsigned char ca = (unsigned char)*a, cb = (unsigned char)*b;
        if (ca != cb) return (int)ca - (int)cb;
        if (!ca) return 0;
        a++; b++;
    }
    return 0;
}

typedef struct flatfs_fs {
    uint8_t             *buf;
    uint64_t             capacity;
    flatfs_super_t      *super;
    int                  rw;
    int                  mounted;
    int                  readonly;
    uint64_t             tykid_seq;
    tykid_gate_ctx_t    *tykid;
} flatfs_fs_t;

extern flatfs_fs_t g_fs;

#include "flatfs_tykid.h"

static inline void *blk_ptr(uint64_t blk)
{
    return g_fs.buf + (blk << FLATFS_BLOCK_SHIFT);
}

static inline int blk_valid(uint64_t blk)
{
    return blk > 0 && blk < g_fs.super->total_blocks;
}

static inline flatfs_inode_t *inode_ptr(uint64_t ino)
{
    uint64_t base = g_fs.super->inode_table_start;
    uint64_t blk  = base + ino / FLATFS_INODES_PER_BLK;
    uint32_t off  = (ino % FLATFS_INODES_PER_BLK) * FLATFS_INODE_SIZE;
    return (flatfs_inode_t *)((uint8_t *)blk_ptr(blk) + off);
}

static inline uint32_t next_pow2(uint32_t n)
{
    if (n == 0) return 1;
    n--;
    n |= n >> 1; n |= n >> 2; n |= n >> 4;
    n |= n >> 8; n |= n >> 16;
    return n + 1;
}

flatfs_err_t  flatfs_inode_read(uint64_t ino, flatfs_inode_t *out);
flatfs_err_t  flatfs_inode_write(const flatfs_inode_t *in);
flatfs_err_t  flatfs_inode_alloc(uint64_t *out_ino);
flatfs_err_t  flatfs_inode_free(uint64_t ino);

flatfs_err_t  flatfs_alloc_block(uint64_t *out);
flatfs_err_t  flatfs_alloc_blocks(uint32_t count, uint64_t *out);
flatfs_err_t  flatfs_free_block(uint64_t blk);
flatfs_err_t  flatfs_free_blocks(uint64_t blk, uint32_t count);

flatfs_err_t  flatfs_journal_begin(uint64_t *tx_id);
flatfs_err_t  flatfs_journal_log(uint64_t tx_id, uint64_t blk, const void *data);
flatfs_err_t  flatfs_journal_commit(uint64_t tx_id);
flatfs_err_t  flatfs_journal_abort(uint64_t tx_id);
flatfs_err_t  flatfs_journal_replay(void);

flatfs_err_t  flatfs_dir_lookup(uint64_t dir_ino, const char *name, uint64_t *out_ino);
flatfs_err_t  flatfs_dir_insert(uint64_t dir_ino, const char *name,
                                 uint64_t ino, uint8_t type);
flatfs_err_t  flatfs_dir_remove(uint64_t dir_ino, const char *name);
flatfs_err_t  flatfs_dir_isempty(uint64_t dir_ino, int *empty);

flatfs_err_t  flatfs_btree_lookup(uint64_t hash, uint64_t ino, uint64_t *out);
flatfs_err_t  flatfs_btree_insert(uint64_t hash, uint64_t ino, uint64_t val);
flatfs_err_t  flatfs_btree_delete(uint64_t hash, uint64_t ino);

flatfs_err_t  flatfs_file_read_blk(const flatfs_inode_t *in, uint64_t lblk,
                                    void *buf);
flatfs_err_t  flatfs_file_write_blk(flatfs_inode_t *in, uint64_t lblk,
                                     const void *data);
flatfs_err_t  flatfs_file_truncate(flatfs_inode_t *in, uint64_t new_sz);

flatfs_err_t  flatfs_inline_read(const flatfs_inode_t *in, uint64_t off,
                                  void *buf, size_t len, size_t *nr);
flatfs_err_t  flatfs_inline_write(flatfs_inode_t *in, uint64_t off,
                                   const void *buf, size_t len, size_t *nw);
int           flatfs_inline_fits(size_t sz);

uint64_t  flatfs_tykid_gen(uint16_t op);
flatfs_err_t flatfs_tykid_verify(uint64_t id, uint16_t expected_op);

void  flatfs_mon_read(void);
void  flatfs_mon_write(void);
void  flatfs_mon_alloc(void);
void  flatfs_mon_journal(void);
void  flatfs_mon_htree_lookup(int hit);
void  flatfs_mon_btree_lookup(int hit);

#endif
