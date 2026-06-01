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

#include "flatfs_internal.h"
#include "flatfs_super.h"
#include "flatfs_inode.h"
#include "flatfs_inode_cache.h"
#include "flatfs_alloc.h"
#include "flatfs_journal.h"
#include "flatfs_btree.h"
#include "flatfs_dir.h"
#include "flatfs_file.h"
#include "flatfs_inline.h"
#include "flatfs_monitor.h"
#include "flatfs_handoff.h"
#include "flatfs_crc.h"

flatfs_err_t flatfs_arbiter_register_fs(void);
flatfs_err_t flatfs_arbiter_unregister_fs(void);
flatfs_err_t flatfs_journal_recover(void);

static uint64_t g_tykid_seq;

uint64_t flatfs_tykid_gen(uint16_t op)
{
    return FLATFS_TYKID(op, ++g_tykid_seq);
}

flatfs_err_t flatfs_tykid_verify(uint64_t id, uint16_t expected_op)
{
    if (FLATFS_TYKID_OP(id) != expected_op) return FLATFS_ERR_TYKID;
    if (FLATFS_TYKID_SEQ(id) == 0)          return FLATFS_ERR_TYKID;
    return flatfs_tykid_seal_check();
}

flatfs_err_t flatfs_mkfs(void *buf, uint64_t capacity, const char *label)
{
    if (!buf || capacity < FLATFS_BLOCK_SIZE * 32) return FLATFS_ERR_INVAL;
    flatfs_crc32_init_table();
    uint64_t ninodes = (capacity >> FLATFS_BLOCK_SHIFT) / 8;
    if (ninodes < 64)    ninodes = 64;
    if (ninodes > 65536) ninodes = 65536;
    return flatfs_super_init(buf, capacity, label, ninodes);
}

flatfs_err_t flatfs_mount(void *buf, uint64_t capacity,
                           tykid_gate_ctx_t *tykid_ctx)
{
    if (!buf || capacity < FLATFS_BLOCK_SIZE * 32) return FLATFS_ERR_INVAL;

    flatfs_crc32_init_table();

    g_fs.buf      = (uint8_t *)buf;
    g_fs.capacity = capacity;
    g_fs.rw       = 1;
    g_fs.mounted  = 0;
    g_fs.readonly = 0;
    g_fs.tykid    = NULL;

    flatfs_err_t e = flatfs_tykid_bind(tykid_ctx);
    if (e != FLATFS_OK) return e;
    g_fs.tykid = tykid_ctx;

    flatfs_super_t *sb;
    e = flatfs_super_read(buf, capacity, &sb);
    if (e != FLATFS_OK) {
        flatfs_tykid_audit_super_corrupt();
        return e;
    }
    g_fs.super = sb;

    if (sb->state & FLATFS_STATE_DIRTY) {
        e = flatfs_journal_recover();
        if (e != FLATFS_OK) {
            flatfs_tykid_audit_mount(0);
            return e;
        }
    }

    flatfs_super_mark_dirty(sb);

    e = flatfs_alloc_init();                    if (e) return e;
    e = flatfs_journal_init();                  if (e) return e;
    e = flatfs_btree_init(sb->btree_root_blk);  if (e) return e;

    flatfs_icache_init();
    flatfs_mon_init();
    flatfs_handoff_init();

    g_tykid_seq = sb->tykid_seq;
    g_fs.mounted = 1;

    flatfs_tykid_audit_mount(1);
    flatfs_arbiter_register_fs();
    return FLATFS_OK;
}

flatfs_err_t flatfs_unmount(void)
{
    if (!g_fs.mounted) return FLATFS_ERR_NOINIT;

    flatfs_icache_flush();
    flatfs_journal_checkpoint();
    flatfs_mon_snapshot_to_super();
    flatfs_handoff_tick();

    g_fs.super->tykid_seq = g_tykid_seq;
    flatfs_tykid_audit_unmount();
    flatfs_super_mark_clean(g_fs.super);

    flatfs_tykid_unbind();
    flatfs_arbiter_unregister_fs();

    g_fs.mounted  = 0;
    g_fs.readonly = 0;
    g_fs.tykid    = NULL;
    return FLATFS_OK;
}

flatfs_err_t flatfs_sync(void)
{
    if (!g_fs.mounted) return FLATFS_ERR_NOINIT;
    flatfs_icache_flush();
    flatfs_journal_checkpoint();
    flatfs_mon_snapshot_to_super();
    flatfs_super_write(g_fs.super);
    return FLATFS_OK;
}

flatfs_err_t flatfs_statfs(flatfs_space_t *out)
{
    if (!g_fs.mounted) return FLATFS_ERR_NOINIT;
    flatfs_super_t *sb = g_fs.super;
    out->total_blocks   = sb->total_blocks;
    out->free_blocks    = sb->free_blocks;
    out->total_inodes   = sb->total_inodes;
    out->free_inodes    = sb->free_inodes;
    out->capacity_bytes = sb->capacity_bytes;
    out->free_bytes     = sb->free_blocks * FLATFS_BLOCK_SIZE;
    out->block_size     = FLATFS_BLOCK_SIZE;
    return FLATFS_OK;
}

static flatfs_err_t path_walk(const char *path, uint64_t *out_ino,
                               uint64_t *out_parent, char *out_last,
                               int symlink_depth)
{
    if (!path || path[0] != '/') return FLATFS_ERR_INVAL;
    if (symlink_depth > 8)       return FLATFS_ERR_LOOP;

    uint64_t cur = g_fs.super->root_ino;
    uint64_t par = cur;

    const char *p = path + 1;
    char comp[FLATFS_NAME_MAX];

    while (*p) {
        size_t n = 0;
        while (p[n] && p[n] != '/') n++;
        if (n == 0) { p++; continue; }
        if (n >= FLATFS_NAME_MAX) return FLATFS_ERR_NAMETOOLONG;
        FMEMCPY(comp, p, n);
        comp[n] = 0;
        p += n;
        if (*p == '/') p++;

        int is_last = (*p == 0);
        if (is_last && out_last)
            FMEMCPY(out_last, comp, n + 1);

        par = cur;
        flatfs_err_t e = flatfs_dir_lookup(cur, comp, &cur);
        if (e) return e;

        if (!is_last || !out_last) {
            flatfs_inode_t in;
            e = flatfs_inode_read(cur, &in);
            if (e) return e;

            if (FLATFS_S_ISLNK(in.mode)) {
                char target[FLATFS_SYMLINK_MAX];
                size_t tlen;
                if (in.flags & FLATFS_FL_SYMLINK_INLN) {
                    tlen = (size_t)in.size;
                    FMEMCPY(target, in.data.inline_data, tlen);
                    target[tlen] = 0;
                } else {
                    e = flatfs_file_pread(&in, 0, target,
                                          FLATFS_SYMLINK_MAX - 1, &tlen);
                    if (e) return e;
                    target[tlen] = 0;
                }
                char newpath[FLATFS_SYMLINK_MAX + 256];
                size_t rlen = fstrlen(p, 256);
                if (tlen + 1 + rlen + 1 > sizeof(newpath))
                    return FLATFS_ERR_NAMETOOLONG;
                FMEMCPY(newpath, target, tlen);
                if (rlen) {
                    newpath[tlen] = '/';
                    FMEMCPY(newpath + tlen + 1, p, rlen + 1);
                } else {
                    newpath[tlen] = 0;
                }
                return path_walk(newpath, out_ino, out_parent,
                                 out_last, symlink_depth + 1);
            }
        }
    }

    *out_ino = cur;
    if (out_parent) *out_parent = par;
    return FLATFS_OK;
}

flatfs_err_t flatfs_stat(const char *path, flatfs_stat_t *out)
{
    if (!g_fs.mounted) return FLATFS_ERR_NOINIT;
    uint64_t ino;
    flatfs_err_t e = path_walk(path, &ino, NULL, NULL, 0);
    if (e) return e;
    return flatfs_inode_getattr(ino, out);
}

flatfs_err_t flatfs_open(const char *path, int flags, flatfs_file_t *f)
{
    if (!g_fs.mounted) return FLATFS_ERR_NOINIT;

    uint64_t parent, ino;
    char last[FLATFS_NAME_MAX];
    flatfs_err_t e = path_walk(path, &ino, &parent, last, 0);

    if (e == FLATFS_ERR_NOTFOUND && (flags & FLATFS_O_CREAT)) {
        uint64_t new_ino;
        e = flatfs_inode_alloc(&new_ino);
        if (e) return e;

        flatfs_inode_t in;
        flatfs_inode_init(&in, new_ino, FLATFS_S_IFREG | 0644u, 0, 0);
        in.flags |= FLATFS_FL_INLINE | FLATFS_FL_HASCRC;
        uint64_t ts = flatfs_tykid_entropy();
        if (ts) in.crtime_ns = in.atime_ns = in.mtime_ns = in.ctime_ns = ts;
        e = flatfs_inode_write(&in);
        if (e) { flatfs_inode_free(new_ino); return e; }

        e = flatfs_dir_insert(parent, last, new_ino, FLATFS_DT_REG);
        if (e) { flatfs_inode_free(new_ino); return e; }
        ino = new_ino;
    } else if (e) {
        return e;
    } else if (flags & FLATFS_O_EXCL) {
        return FLATFS_ERR_EXIST;
    }

    f->magic  = FLATFS_FILE_MAGIC;
    f->ino    = ino;
    f->oflags = flags;
    f->pos    = 0;
    f->dirty  = 0;

    flatfs_inode_t in;
    e = flatfs_inode_read(ino, &in);
    if (e) return e;

    if (FLATFS_S_ISDIR(in.mode)) return FLATFS_ERR_ISDIR;
    f->size = in.size;

    if (flags & FLATFS_O_TRUNC) {
        e = flatfs_file_truncate(&in, 0);
        if (e) return e;
        f->size = 0;
    }
    return FLATFS_OK;
}

flatfs_err_t flatfs_close(flatfs_file_t *f)
{
    if (!f || f->magic != FLATFS_FILE_MAGIC) return FLATFS_ERR_BADSTATE;
    if (f->dirty) {
        flatfs_inode_t in;
        if (flatfs_inode_read(f->ino, &in) == FLATFS_OK) {
            in.size = f->size;
            flatfs_inode_write(&in);
        }
    }
    f->magic = 0;
    return FLATFS_OK;
}

flatfs_err_t flatfs_read(flatfs_file_t *f, void *buf, size_t len, size_t *nr)
{
    if (!f || f->magic != FLATFS_FILE_MAGIC) return FLATFS_ERR_BADSTATE;
    flatfs_inode_t in;
    flatfs_err_t e = flatfs_inode_read(f->ino, &in);
    if (e) return e;
    e = flatfs_file_pread(&in, f->pos, buf, len, nr);
    if (e == FLATFS_OK) f->pos += *nr;
    return e;
}

flatfs_err_t flatfs_write(flatfs_file_t *f, const void *buf, size_t len,
                           size_t *nw)
{
    if (!f || f->magic != FLATFS_FILE_MAGIC) return FLATFS_ERR_BADSTATE;
    if (!(f->oflags & (FLATFS_O_WRONLY | FLATFS_O_RDWR)))
        return FLATFS_ERR_PERM;
    if (g_fs.readonly) return FLATFS_ERR_RDONLY;

    flatfs_err_t e = flatfs_tykid_seal_check();
    if (e != FLATFS_OK) {
        g_fs.readonly = 1;
        flatfs_tykid_audit_readonly();
        return e;
    }

    flatfs_inode_t in;
    e = flatfs_inode_read(f->ino, &in);
    if (e) return e;
    uint64_t off = (f->oflags & FLATFS_O_APPEND) ? in.size : f->pos;
    e = flatfs_file_pwrite(&in, off, buf, len, nw);
    if (e == FLATFS_OK) {
        uint64_t ts = flatfs_tykid_entropy();
        if (ts) in.mtime_ns = ts;
        in.ctime_ns = ts ? ts : in.mtime_ns;
        flatfs_inode_write(&in);
        f->pos   = off + *nw;
        f->size  = in.size;
        f->dirty = 1;
    }
    flatfs_handoff_tick();
    return e;
}

flatfs_err_t flatfs_seek(flatfs_file_t *f, int64_t off, int whence,
                          uint64_t *pos)
{
    if (!f || f->magic != FLATFS_FILE_MAGIC) return FLATFS_ERR_BADSTATE;
    int64_t np;
    switch (whence) {
    case FLATFS_SEEK_SET: np = off; break;
    case FLATFS_SEEK_CUR: np = (int64_t)f->pos + off; break;
    case FLATFS_SEEK_END: np = (int64_t)f->size + off; break;
    default: return FLATFS_ERR_INVAL;
    }
    if (np < 0) return FLATFS_ERR_INVAL;
    f->pos = (uint64_t)np;
    if (pos) *pos = f->pos;
    return FLATFS_OK;
}

flatfs_err_t flatfs_fsync(flatfs_file_t *f)
{
    if (!f || f->magic != FLATFS_FILE_MAGIC) return FLATFS_ERR_BADSTATE;
    if (f->dirty) {
        flatfs_inode_t in;
        if (flatfs_inode_read(f->ino, &in) == FLATFS_OK) {
            in.size = f->size;
            flatfs_inode_write(&in);
        }
        f->dirty = 0;
    }
    return flatfs_sync();
}

flatfs_err_t flatfs_unlink(const char *path)
{
    if (!g_fs.mounted) return FLATFS_ERR_NOINIT;
    uint64_t parent, ino;
    char last[FLATFS_NAME_MAX];
    flatfs_err_t e = path_walk(path, &ino, &parent, last, 0);
    if (e) return e;
    flatfs_inode_t in;
    e = flatfs_inode_read(ino, &in);
    if (e) return e;
    if (FLATFS_S_ISDIR(in.mode)) return FLATFS_ERR_ISDIR;
    e = flatfs_dir_remove(parent, last);
    if (e) return e;
    if (--in.nlink == 0) {
        flatfs_file_truncate(&in, 0);
        return flatfs_inode_free(ino);
    }
    return flatfs_inode_write(&in);
}

flatfs_err_t flatfs_mkdir(const char *path, uint16_t mode)
{
    if (!g_fs.mounted) return FLATFS_ERR_NOINIT;
    uint64_t parent, ino;
    char last[FLATFS_NAME_MAX];
    flatfs_err_t e = path_walk(path, &ino, &parent, last, 0);
    if (e == FLATFS_OK) return FLATFS_ERR_EXIST;
    if (e != FLATFS_ERR_NOTFOUND) return e;

    uint64_t new_ino;
    return flatfs_dir_create(parent, last, mode, 0, 0, &new_ino);
}

flatfs_err_t flatfs_rmdir(const char *path)
{
    if (!g_fs.mounted) return FLATFS_ERR_NOINIT;
    uint64_t parent, ino;
    char last[FLATFS_NAME_MAX];
    flatfs_err_t e = path_walk(path, &ino, &parent, last, 0);
    if (e) return e;

    flatfs_inode_t in;
    e = flatfs_inode_read(ino, &in);
    if (e) return e;
    if (!FLATFS_S_ISDIR(in.mode)) return FLATFS_ERR_NOTDIR;

    int empty;
    e = flatfs_dir_isempty(ino, &empty);
    if (e) return e;
    if (!empty) return FLATFS_ERR_NOTEMPTY;

    e = flatfs_dir_remove(parent, last);
    if (e) return e;
    return flatfs_inode_free(ino);
}

flatfs_err_t flatfs_rename(const char *old, const char *newp)
{
    if (!g_fs.mounted) return FLATFS_ERR_NOINIT;
    uint64_t opar, oino, npar, nino;
    char olast[FLATFS_NAME_MAX], nlast[FLATFS_NAME_MAX];

    flatfs_err_t e = path_walk(old, &oino, &opar, olast, 0);
    if (e) return e;

    e = path_walk(newp, &nino, &npar, nlast, 0);
    if (e && e != FLATFS_ERR_NOTFOUND) return e;

    flatfs_inode_t in;
    e = flatfs_inode_read(oino, &in);
    if (e) return e;

    e = flatfs_dir_remove(opar, olast);
    if (e) return e;

    uint8_t dt = FLATFS_S_ISDIR(in.mode) ? FLATFS_DT_DIR :
                 FLATFS_S_ISLNK(in.mode) ? FLATFS_DT_LNK : FLATFS_DT_REG;

    return flatfs_dir_insert(npar, nlast, oino, dt);
}

flatfs_err_t flatfs_symlink(const char *target, const char *link)
{
    if (!g_fs.mounted) return FLATFS_ERR_NOINIT;
    uint64_t parent, ino;
    char last[FLATFS_NAME_MAX];
    flatfs_err_t e = path_walk(link, &ino, &parent, last, 0);
    if (e == FLATFS_OK) return FLATFS_ERR_EXIST;
    if (e != FLATFS_ERR_NOTFOUND) return e;

    uint64_t new_ino;
    e = flatfs_inode_alloc(&new_ino);
    if (e) return e;

    flatfs_inode_t in;
    flatfs_inode_init(&in, new_ino, FLATFS_S_IFLNK | 0777u, 0, 0);
    size_t tlen = fstrlen(target, FLATFS_SYMLINK_MAX);
    if (flatfs_inline_fits(tlen + 1)) {
        FMEMCPY(in.data.inline_data, target, tlen + 1);
        in.size  = tlen;
        in.flags |= FLATFS_FL_INLINE | FLATFS_FL_SYMLINK_INLN;
    } else {
        in.flags &= ~FLATFS_FL_INLINE;
    }
    e = flatfs_inode_write(&in);
    if (e) { flatfs_inode_free(new_ino); return e; }

    if (!(in.flags & FLATFS_FL_SYMLINK_INLN)) {
        size_t nw;
        e = flatfs_file_pwrite(&in, 0, target, tlen + 1, &nw);
        if (e) { flatfs_inode_free(new_ino); return e; }
    }
    return flatfs_dir_insert(parent, last, new_ino, FLATFS_DT_LNK);
}

flatfs_err_t flatfs_readlink(const char *path, char *buf, size_t sz,
                              size_t *len)
{
    if (!g_fs.mounted) return FLATFS_ERR_NOINIT;
    uint64_t ino;
    flatfs_err_t e = path_walk(path, &ino, NULL, NULL, 0);
    if (e) return e;
    flatfs_inode_t in;
    e = flatfs_inode_read(ino, &in);
    if (e) return e;
    if (!FLATFS_S_ISLNK(in.mode)) return FLATFS_ERR_INVAL;
    if (in.flags & FLATFS_FL_SYMLINK_INLN) {
        size_t n = (size_t)in.size;
        if (n >= sz) n = sz - 1;
        FMEMCPY(buf, in.data.inline_data, n);
        buf[n] = 0;
        *len = n;
        return FLATFS_OK;
    }
    return flatfs_file_pread(&in, 0, buf, sz - 1, len);
}

flatfs_err_t flatfs_link(const char *old, const char *newp)
{
    if (!g_fs.mounted) return FLATFS_ERR_NOINIT;
    uint64_t oino, npar, nino;
    char last[FLATFS_NAME_MAX];
    flatfs_err_t e = path_walk(old, &oino, NULL, NULL, 0);
    if (e) return e;
    e = path_walk(newp, &nino, &npar, last, 0);
    if (e == FLATFS_OK) return FLATFS_ERR_EXIST;
    if (e != FLATFS_ERR_NOTFOUND) return e;

    flatfs_inode_t in;
    e = flatfs_inode_read(oino, &in);
    if (e) return e;
    if (FLATFS_S_ISDIR(in.mode)) return FLATFS_ERR_ISDIR;
    in.nlink++;
    e = flatfs_inode_write(&in);
    if (e) return e;

    uint8_t dt = FLATFS_S_ISLNK(in.mode) ? FLATFS_DT_LNK : FLATFS_DT_REG;
    return flatfs_dir_insert(npar, last, oino, dt);
}

flatfs_err_t flatfs_opendir(const char *path, flatfs_dir_t *d)
{
    if (!g_fs.mounted) return FLATFS_ERR_NOINIT;
    uint64_t ino;
    flatfs_err_t e = path_walk(path, &ino, NULL, NULL, 0);
    if (e) return e;
    flatfs_inode_t in;
    e = flatfs_inode_read(ino, &in);
    if (e) return e;
    if (!FLATFS_S_ISDIR(in.mode)) return FLATFS_ERR_NOTDIR;
    d->magic = FLATFS_DIR_MAGIC;
    d->ino   = ino;
    d->pos   = 0;
    d->count = (uint32_t)in.size;
    return FLATFS_OK;
}

flatfs_err_t flatfs_readdir(flatfs_dir_t *d, flatfs_dirent_info_t *out)
{
    if (!d || d->magic != FLATFS_DIR_MAGIC) return FLATFS_ERR_BADSTATE;
    return flatfs_dir_readdir(d->ino, &d->pos, out);
}

flatfs_err_t flatfs_closedir(flatfs_dir_t *d)
{
    if (!d || d->magic != FLATFS_DIR_MAGIC) return FLATFS_ERR_BADSTATE;
    d->magic = 0;
    return FLATFS_OK;
}

const char *flatfs_strerror(flatfs_err_t e)
{
    switch (e) {
    case FLATFS_OK:               return "success";
    case FLATFS_ERR_INVAL:        return "invalid argument";
    case FLATFS_ERR_NOINIT:       return "filesystem not mounted";
    case FLATFS_ERR_NOTFOUND:     return "no such file or directory";
    case FLATFS_ERR_EXIST:        return "file exists";
    case FLATFS_ERR_NOSPACE:      return "no space left on device";
    case FLATFS_ERR_RDONLY:       return "read-only filesystem";
    case FLATFS_ERR_CRC:          return "CRC32 mismatch";
    case FLATFS_ERR_CORRUPT:      return "corrupt on-disk structure";
    case FLATFS_ERR_OVERFLOW:     return "capacity exceeded";
    case FLATFS_ERR_BOUNDS:       return "address out of bounds";
    case FLATFS_ERR_BADSTATE:     return "bad object state";
    case FLATFS_ERR_ISDIR:        return "is a directory";
    case FLATFS_ERR_NOTDIR:       return "not a directory";
    case FLATFS_ERR_NOTEMPTY:     return "directory not empty";
    case FLATFS_ERR_LOOP:         return "symlink loop";
    case FLATFS_ERR_NAMETOOLONG:  return "name too long";
    case FLATFS_ERR_PERM:         return "permission denied";
    case FLATFS_ERR_IO:           return "I/O error";
    case FLATFS_ERR_JOURNAL:      return "journal error";
    case FLATFS_ERR_BTREE:        return "btree error";
    case FLATFS_ERR_ALLOC:        return "allocator error";
    case FLATFS_ERR_TYKID:        return "TYKID verification failed";
    case FLATFS_ERR_TOOBIG:       return "file too large";
    case FLATFS_ERR_NOSYS:        return "not implemented";
    case FLATFS_ERR_MAGIC:        return "bad magic number";
    case FLATFS_ERR_HANDOFF:      return "arbiter handoff failed";
    default:                      return "unknown error";
    }
}

uint32_t flatfs_crc32(const void *data, size_t len)
{
    return flatfs_crc32_internal(data, len);
}
