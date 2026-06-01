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

#ifndef FLATFS_H
#define FLATFS_H

#ifdef __KERNEL__
#  include "../../inc/kernel.h"
#else
#  include <stdint.h>
#  include <stddef.h>
#  include <stdbool.h>
#endif

#define FLATFS_BLOCK_SIZE      4096u
#define FLATFS_BLOCK_SHIFT     12u
#define FLATFS_MAGIC           0x464C4154u
#define FLATFS_VERSION         1u
#define FLATFS_NAME_MAX        256u
#define FLATFS_SYMLINK_MAX     4096u

#define FLATFS_MAX_BLOCKS      (1ULL << 41)
#define FLATFS_MAX_BYTES       (FLATFS_MAX_BLOCKS * FLATFS_BLOCK_SIZE)

#define FLATFS_INODE_SIZE      256u
#define FLATFS_ROOT_INO        1ULL
#define FLATFS_INODES_PER_BLK  (FLATFS_BLOCK_SIZE / FLATFS_INODE_SIZE)

#define FLATFS_INLINE_MAX      96u

#define FLATFS_BTREE_ORDER     120u
#define FLATFS_BTREE_MAGIC     0x46425452u
#define FLATFS_BTREE_MIN_KEYS  (FLATFS_BTREE_ORDER / 2)

#define FLATFS_JOURNAL_MAGIC   0x464A524eu
#define FLATFS_JOURNAL_BLOCKS  1024u
#define FLATFS_JOURNAL_MAXTX   256u

#define FLATFS_HTREE_MAGIC     0x46485452u
#define FLATFS_HTREE_FANOUT    256u
#define FLATFS_DIRENT_SIZE     128u
#define FLATFS_DIRENTS_PER_BLK (FLATFS_BLOCK_SIZE / FLATFS_DIRENT_SIZE)

#define FLATFS_FREELIST_MAGIC  0x46465253u
#define FLATFS_ALLOC_MAXORDER  21u
#define FLATFS_GROUP_BLOCKS    2048u

#define FLATFS_CRC32_POLY      0xEDB88320u

#define FLATFS_TYKID_WRITE     0x0001ULL
#define FLATFS_TYKID_ALLOC     0x0002ULL
#define FLATFS_TYKID_FREE      0x0003ULL
#define FLATFS_TYKID_JOURNAL   0x0004ULL
#define FLATFS_TYKID_DIR       0x0005ULL
#define FLATFS_TYKID_BTREE     0x0006ULL
#define FLATFS_TYKID_SHIFT     48u
#define FLATFS_TYKID(op, seq)  (((uint64_t)(op) << FLATFS_TYKID_SHIFT) | ((seq) & 0xFFFFFFFFFFFFULL))
#define FLATFS_TYKID_OP(id)    ((uint16_t)((id) >> FLATFS_TYKID_SHIFT))
#define FLATFS_TYKID_SEQ(id)   ((id) & 0xFFFFFFFFFFFFULL)

#define FLATFS_FL_INLINE       (1u <<  0)
#define FLATFS_FL_EXTENTS      (1u <<  1)
#define FLATFS_FL_HTREE        (1u <<  2)
#define FLATFS_FL_IMMUTABLE    (1u <<  3)
#define FLATFS_FL_APPEND       (1u <<  4)
#define FLATFS_FL_NOATIME      (1u <<  5)
#define FLATFS_FL_SYNC         (1u <<  6)
#define FLATFS_FL_DIRTY        (1u <<  7)
#define FLATFS_FL_ENCRYPT      (1u <<  8)
#define FLATFS_FL_HASCRC       (1u <<  9)
#define FLATFS_FL_SYMLINK_INLN (1u << 10)

#define FLATFS_FEAT_JOURNAL    (1u << 0)
#define FLATFS_FEAT_HTREE      (1u << 1)
#define FLATFS_FEAT_BTREE      (1u << 2)
#define FLATFS_FEAT_INLINE     (1u << 3)
#define FLATFS_FEAT_CRC32      (1u << 4)
#define FLATFS_FEAT_64BIT      (1u << 5)
#define FLATFS_FEAT_MONITOR    (1u << 6)
#define FLATFS_FEAT_ARBITER    (1u << 7)

#define FLATFS_STATE_CLEAN     0x0001u
#define FLATFS_STATE_DIRTY     0x0002u
#define FLATFS_STATE_ERROR     0x0004u
#define FLATFS_STATE_REPLAY    0x0008u
#define FLATFS_STATE_ARBITER   0x0010u

#define FLATFS_S_IFMT   0xF000u
#define FLATFS_S_IFREG  0x8000u
#define FLATFS_S_IFDIR  0x4000u
#define FLATFS_S_IFLNK  0xA000u
#define FLATFS_S_IFBLK  0x6000u
#define FLATFS_S_IFCHR  0x2000u
#define FLATFS_S_IFIFO  0x1000u
#define FLATFS_S_IFSOCK 0xC000u

#define FLATFS_S_ISREG(m)  (((m) & FLATFS_S_IFMT) == FLATFS_S_IFREG)
#define FLATFS_S_ISDIR(m)  (((m) & FLATFS_S_IFMT) == FLATFS_S_IFDIR)
#define FLATFS_S_ISLNK(m)  (((m) & FLATFS_S_IFMT) == FLATFS_S_IFLNK)

#define FLATFS_O_RDONLY    0x0000u
#define FLATFS_O_WRONLY    0x0001u
#define FLATFS_O_RDWR      0x0002u
#define FLATFS_O_CREAT     0x0040u
#define FLATFS_O_EXCL      0x0080u
#define FLATFS_O_TRUNC     0x0200u
#define FLATFS_O_APPEND    0x0400u
#define FLATFS_O_SYNC      0x1000u
#define FLATFS_O_DIRECTORY 0x4000u

#define FLATFS_SEEK_SET  0
#define FLATFS_SEEK_CUR  1
#define FLATFS_SEEK_END  2

#define FLATFS_DT_UNKNOWN  0
#define FLATFS_DT_FIFO     1
#define FLATFS_DT_CHR      2
#define FLATFS_DT_DIR      4
#define FLATFS_DT_BLK      6
#define FLATFS_DT_REG      8
#define FLATFS_DT_LNK     10
#define FLATFS_DT_SOCK    12

typedef enum flatfs_err {
    FLATFS_OK             =   0,
    FLATFS_ERR_INVAL      =  -1,
    FLATFS_ERR_NOINIT     =  -2,
    FLATFS_ERR_NOTFOUND   =  -3,
    FLATFS_ERR_EXIST      =  -4,
    FLATFS_ERR_NOSPACE    =  -5,
    FLATFS_ERR_RDONLY     =  -6,
    FLATFS_ERR_CRC        =  -7,
    FLATFS_ERR_CORRUPT    =  -8,
    FLATFS_ERR_OVERFLOW   =  -9,
    FLATFS_ERR_BOUNDS     = -10,
    FLATFS_ERR_BADSTATE   = -11,
    FLATFS_ERR_ISDIR      = -12,
    FLATFS_ERR_NOTDIR     = -13,
    FLATFS_ERR_NOTEMPTY   = -14,
    FLATFS_ERR_LOOP       = -15,
    FLATFS_ERR_NAMETOOLONG= -16,
    FLATFS_ERR_PERM       = -17,
    FLATFS_ERR_IO         = -18,
    FLATFS_ERR_JOURNAL    = -19,
    FLATFS_ERR_BTREE      = -20,
    FLATFS_ERR_ALLOC      = -21,
    FLATFS_ERR_TYKID      = -22,
    FLATFS_ERR_TOOBIG     = -23,
    FLATFS_ERR_NOSYS      = -24,
    FLATFS_ERR_MAGIC      = -25,
    FLATFS_ERR_HANDOFF    = -26,
} flatfs_err_t;

typedef struct __attribute__((packed)) flatfs_super {
    uint32_t  magic;
    uint32_t  version;
    uint64_t  uuid[2];
    uint64_t  total_blocks;
    uint64_t  free_blocks;
    uint64_t  total_inodes;
    uint64_t  free_inodes;
    uint64_t  root_ino;
    uint64_t  journal_start;
    uint64_t  journal_blocks;
    uint64_t  inode_table_start;
    uint64_t  inode_table_blocks;
    uint64_t  btree_root_blk;
    uint64_t  freelist_start;
    uint64_t  freelist_blocks;
    uint64_t  data_start;
    uint64_t  mkfs_time;
    uint64_t  mount_time;
    uint64_t  unmount_time;
    uint64_t  capacity_bytes;
    uint32_t  block_size;
    uint32_t  inode_size;
    uint32_t  features;
    uint32_t  state;
    uint32_t  error_count;
    uint32_t  mount_count;
    uint32_t  max_mounts;
    uint64_t  tykid_seq;
    uint8_t   label[64];
    uint64_t  mon_read_ops;
    uint64_t  mon_write_ops;
    uint64_t  mon_alloc_ops;
    uint64_t  mon_journal_commits;
    uint8_t   _pad[3800];
    uint32_t  crc32;
} flatfs_super_t;

typedef struct __attribute__((packed)) flatfs_inode {
    uint64_t  ino;
    uint16_t  mode;
    uint16_t  uid;
    uint16_t  gid;
    uint16_t  nlink;
    uint64_t  size;
    uint64_t  blocks;
    uint64_t  atime_ns;
    uint64_t  mtime_ns;
    uint64_t  ctime_ns;
    uint64_t  crtime_ns;
    uint32_t  flags;
    uint32_t  generation;
    union {
        uint8_t  inline_data[FLATFS_INLINE_MAX];
        uint64_t blk_ptrs[12];
        struct {
            uint64_t  extent_root;
            uint32_t  extent_depth;
            uint32_t  extent_count;
            uint8_t   _epad[80];
        } extents;
    } data;
    uint64_t  parent_ino;
    uint64_t  dir_htree_blk;
    uint32_t  htree_depth;
    uint32_t  _res0;
    uint64_t  tykid_last;
    uint8_t   _pad[48];
    uint32_t  crc32;
    uint32_t  inode_magic;
} flatfs_inode_t;

#define FLATFS_INODE_MAGIC  0x464C494eu

typedef struct __attribute__((packed)) flatfs_dirent {
    uint64_t  ino;
    uint32_t  hash;
    uint8_t   name_len;
    uint8_t   file_type;
    uint16_t  rec_len;
    char      name[108];
    uint32_t  crc32;
} flatfs_dirent_t;

typedef struct flatfs_file {
    uint32_t  magic;
#define FLATFS_FILE_MAGIC  0x464C4646u
    uint64_t  ino;
    int       oflags;
    uint64_t  pos;
    uint64_t  size;
    int       dirty;
} flatfs_file_t;

typedef struct flatfs_dir {
    uint32_t  magic;
#define FLATFS_DIR_MAGIC  0x464C4446u
    uint64_t  ino;
    uint32_t  pos;
    uint32_t  count;
} flatfs_dir_t;

typedef struct flatfs_dirent_info {
    uint64_t  ino;
    uint8_t   type;
    uint8_t   name_len;
    char      name[FLATFS_NAME_MAX];
} flatfs_dirent_info_t;

typedef struct flatfs_stat {
    uint64_t  ino;
    uint16_t  mode;
    uint16_t  uid, gid, nlink;
    uint64_t  size, blocks;
    uint64_t  atime_ns, mtime_ns, ctime_ns, crtime_ns;
    uint32_t  flags;
    uint32_t  blksize;
} flatfs_stat_t;

typedef struct flatfs_space {
    uint64_t  total_blocks, free_blocks;
    uint64_t  total_inodes, free_inodes;
    uint64_t  capacity_bytes, free_bytes;
    uint32_t  block_size;
} flatfs_space_t;

typedef char _flatfs_chk_super [(sizeof(flatfs_super_t)  == FLATFS_BLOCK_SIZE)  ? 1 : -1];
typedef char _flatfs_chk_inode [(sizeof(flatfs_inode_t)  == FLATFS_INODE_SIZE)  ? 1 : -1];
typedef char _flatfs_chk_dirent[(sizeof(flatfs_dirent_t) == FLATFS_DIRENT_SIZE) ? 1 : -1];

struct flatfs_btree_node;
struct flatfs_journal_hdr;
struct flatfs_freelist_group;
struct flatfs_htree_root;
struct flatfs_monitor_stats;

#ifndef __TYKID_H__
struct tykid_gate_ctx;
typedef struct tykid_gate_ctx tykid_gate_ctx_t;
#endif

flatfs_err_t  flatfs_mkfs(void *buf, uint64_t capacity, const char *label);
flatfs_err_t  flatfs_mount(void *buf, uint64_t capacity,
                            tykid_gate_ctx_t *tykid_ctx);
flatfs_err_t  flatfs_unmount(void);
flatfs_err_t  flatfs_sync(void);
flatfs_err_t  flatfs_statfs(flatfs_space_t *out);

flatfs_err_t  flatfs_open(const char *path, int flags, flatfs_file_t *f);
flatfs_err_t  flatfs_close(flatfs_file_t *f);
flatfs_err_t  flatfs_read(flatfs_file_t *f, void *buf, size_t len, size_t *nr);
flatfs_err_t  flatfs_write(flatfs_file_t *f, const void *buf, size_t len, size_t *nw);
flatfs_err_t  flatfs_seek(flatfs_file_t *f, int64_t off, int whence, uint64_t *pos);
flatfs_err_t  flatfs_fsync(flatfs_file_t *f);

flatfs_err_t  flatfs_stat(const char *path, flatfs_stat_t *out);
flatfs_err_t  flatfs_unlink(const char *path);
flatfs_err_t  flatfs_mkdir(const char *path, uint16_t mode);
flatfs_err_t  flatfs_rmdir(const char *path);
flatfs_err_t  flatfs_rename(const char *old, const char *newp);
flatfs_err_t  flatfs_symlink(const char *target, const char *link);
flatfs_err_t  flatfs_readlink(const char *path, char *buf, size_t sz, size_t *len);
flatfs_err_t  flatfs_link(const char *old, const char *newp);

flatfs_err_t  flatfs_opendir(const char *path, flatfs_dir_t *d);
flatfs_err_t  flatfs_readdir(flatfs_dir_t *d, flatfs_dirent_info_t *out);
flatfs_err_t  flatfs_closedir(flatfs_dir_t *d);

uint32_t      flatfs_crc32(const void *data, size_t len);
const char   *flatfs_strerror(flatfs_err_t e);

#endif
