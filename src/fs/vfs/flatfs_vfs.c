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
#include "flatfs_vfs.h"
#include "flatfs_inode.h"
#include "flatfs_dir.h"
#include "flatfs_file.h"
#include "vfs.h"

static uint8_t mode_to_dt(uint16_t mode)
{
    switch (mode & FLATFS_S_IFMT) {
    case FLATFS_S_IFREG: return FLATFS_DT_REG;
    case FLATFS_S_IFDIR: return FLATFS_DT_DIR;
    case FLATFS_S_IFLNK: return FLATFS_DT_LNK;
    default:             return FLATFS_DT_UNKNOWN;
    }
}

static uint8_t dt_to_vfs(uint8_t dt)
{
    switch (dt) {
    case FLATFS_DT_REG: return VFS_DT_REG;
    case FLATFS_DT_DIR: return VFS_DT_DIR;
    case FLATFS_DT_LNK: return VFS_DT_LNK;
    default:            return 0;
    }
}

typedef struct {
    uint64_t ino;
    int      used;
} flat_fd_t;

static flat_fd_t g_fpool[VFS_MAX_FDS];

static flat_fd_t *fd_alloc(void)
{
    for (int i = 0; i < VFS_MAX_FDS; i++)
        if (!g_fpool[i].used) { g_fpool[i].used = 1; return &g_fpool[i]; }
    return 0;
}

static int xe(flatfs_err_t e)
{
    switch (e) {
    case FLATFS_OK:            return 0;
    case FLATFS_ERR_NOTFOUND:  return -2;
    case FLATFS_ERR_EXIST:     return -17;
    case FLATFS_ERR_NOTEMPTY:  return -39;
    case FLATFS_ERR_ISDIR:     return -21;
    case FLATFS_ERR_INVAL:     return -22;
    case FLATFS_ERR_PERM:      return -1;
    default:                   return -1;
    }
}

static flatfs_err_t plookup(const char *rel, uint64_t *out)
{
    uint64_t cur = FLATFS_ROOT_INO;
    if (!rel || !*rel) { *out = cur; return FLATFS_OK; }

    char comp[VFS_NAME_MAX + 1];
    const char *p = rel;
    for (;;) {
        while (*p == '/') p++;
        if (!*p) break;
        int n = 0;
        while (*p && *p != '/' && n < VFS_NAME_MAX) comp[n++] = *p++;
        comp[n] = 0;
        flatfs_err_t e = flatfs_dir_lookup(cur, comp, &cur);
        if (e) return e;
    }
    *out = cur;
    return FLATFS_OK;
}

static int psplit(const char *rel, uint64_t *dir, const char **name)
{
    const char *slash = 0;
    for (const char *p = rel; *p; p++) if (*p == '/') slash = p;
    if (!slash) { *dir = FLATFS_ROOT_INO; *name = rel; return 0; }

    char buf[VFS_PATH_MAX];
    size_t n = (size_t)(slash - rel);
    if (n >= VFS_PATH_MAX) return -22;
    for (size_t i = 0; i < n; i++) buf[i] = rel[i];
    buf[n] = 0;
    *name = slash + 1;
    return xe(plookup(buf, dir));
}

static void cvt_stat(const flatfs_stat_t *fs, vfs_stat_t *vs)
{
    vs->ino   = fs->ino;
    vs->mode  = fs->mode;
    vs->nlink = fs->nlink;
    vs->size  = fs->size;
    vs->mtime = (uint32_t)(fs->mtime_ns / 1000000000ULL);
    vs->ctime = (uint32_t)(fs->ctime_ns / 1000000000ULL);
}

static flatfs_err_t do_create(uint64_t dir, const char *name,
                               uint16_t mode, uint64_t *out)
{
    uint64_t tid = flatfs_tykid_gen(FLATFS_TYKID_WRITE);
    flatfs_err_t e = flatfs_tykid_verify(tid, FLATFS_TYKID_WRITE);
    if (e) return e;

    uint64_t ino;
    e = flatfs_inode_alloc(&ino);
    if (e) return e;

    flatfs_inode_t in;
    flatfs_inode_init(&in, ino, FLATFS_S_IFREG | (mode & 0x1FFu), 0, 0);
    in.flags |= FLATFS_FL_INLINE;
    in.tykid_last = tid;
    e = flatfs_inode_write(&in);
    if (e) { flatfs_inode_free(ino); return e; }

    e = flatfs_dir_insert(dir, name, ino, mode_to_dt(FLATFS_S_IFREG));
    if (e) { flatfs_inode_free(ino); return e; }

    *out = ino;
    return FLATFS_OK;
}

static int b_open(const char *rel, int flags, uint32_t mode, void **priv)
{
    uint64_t ino;
    flatfs_err_t e = plookup(rel, &ino);
    if (e == FLATFS_ERR_NOTFOUND && (flags & VFS_O_CREAT)) {
        uint64_t dir;
        const char *name;
        int rc = psplit(rel, &dir, &name);
        if (rc) return rc;
        e = do_create(dir, name, (uint16_t)mode, &ino);
    }
    if (e) return xe(e);

    if (flags & VFS_O_TRUNC) {
        flatfs_inode_t in;
        if (flatfs_inode_read(ino, &in) == FLATFS_OK)
            flatfs_file_truncate(&in, 0);
    }

    flat_fd_t *f = fd_alloc();
    if (!f) return -12;
    f->ino = ino;
    *priv  = f;
    return 0;
}

static void b_close(void *priv)
{
    ((flat_fd_t *)priv)->used = 0;
}

static int b_read(void *priv, void *buf, uint64_t off, size_t len, size_t *got)
{
    flatfs_inode_t in;
    flatfs_err_t e = flatfs_inode_read(((flat_fd_t *)priv)->ino, &in);
    if (e) return xe(e);
    return xe(flatfs_file_pread(&in, off, buf, len, got));
}

static int b_write(void *priv, const void *buf, uint64_t off, size_t len,
                   size_t *put)
{
    flatfs_inode_t in;
    flatfs_err_t e = flatfs_inode_read(((flat_fd_t *)priv)->ino, &in);
    if (e) return xe(e);
    e = flatfs_file_pwrite(&in, off, buf, len, put);
    if (e) return xe(e);
    return xe(flatfs_inode_write(&in));
}

static int b_truncate(void *priv, uint64_t sz)
{
    flatfs_inode_t in;
    flatfs_err_t e = flatfs_inode_read(((flat_fd_t *)priv)->ino, &in);
    if (e) return xe(e);
    return xe(flatfs_file_truncate(&in, sz));
}

static int b_stat(const char *rel, vfs_stat_t *st)
{
    uint64_t ino;
    flatfs_err_t e = plookup(rel, &ino);
    if (e) return xe(e);
    flatfs_stat_t fs;
    e = flatfs_inode_getattr(ino, &fs);
    if (e) return xe(e);
    cvt_stat(&fs, st);
    return 0;
}

static int b_lstat(const char *rel, vfs_stat_t *st)
{
    uint64_t dir;
    const char *name;
    int rc = psplit(rel, &dir, &name);
    if (rc) return rc;
    uint64_t ino;
    flatfs_err_t e = flatfs_dir_lookup(dir, name, &ino);
    if (e) return xe(e);
    flatfs_stat_t fs;
    e = flatfs_inode_getattr(ino, &fs);
    if (e) return xe(e);
    cvt_stat(&fs, st);
    return 0;
}

static int b_fstat(void *priv, vfs_stat_t *st)
{
    flatfs_stat_t fs;
    flatfs_err_t e = flatfs_inode_getattr(((flat_fd_t *)priv)->ino, &fs);
    if (e) return xe(e);
    cvt_stat(&fs, st);
    return 0;
}

static int b_unlink(const char *rel)
{
    uint64_t dir;
    const char *name;
    int rc = psplit(rel, &dir, &name);
    if (rc) return rc;
    uint64_t ino;
    flatfs_err_t e = flatfs_dir_lookup(dir, name, &ino);
    if (e) return xe(e);
    e = flatfs_dir_remove(dir, name);
    if (e) return xe(e);
    flatfs_inode_t in;
    if (flatfs_inode_read(ino, &in) == FLATFS_OK) {
        if (--in.nlink == 0) flatfs_inode_free(ino);
        else flatfs_inode_write(&in);
    }
    return 0;
}

static int b_mkdir(const char *rel, uint32_t mode)
{
    uint64_t dir;
    const char *name;
    int rc = psplit(rel, &dir, &name);
    if (rc) return rc;
    uint64_t ignored;
    return xe(flatfs_dir_create(dir, name, (uint16_t)mode, 0, 0, &ignored));
}

static int b_rmdir(const char *rel)
{
    uint64_t dir;
    const char *name;
    int rc = psplit(rel, &dir, &name);
    if (rc) return rc;
    uint64_t ino;
    flatfs_err_t e = flatfs_dir_lookup(dir, name, &ino);
    if (e) return xe(e);
    int empty;
    e = flatfs_dir_isempty(ino, &empty);
    if (e) return xe(e);
    if (!empty) return xe(FLATFS_ERR_NOTEMPTY);
    e = flatfs_dir_remove(dir, name);
    if (e) return xe(e);
    return xe(flatfs_inode_free(ino));
}

static int b_readdir(const char *rel, uint64_t idx, vfs_dirent_t *de)
{
    uint64_t ino;
    int rc = xe(plookup(rel, &ino));
    if (rc) return rc;

    uint32_t pos = 0;
    flatfs_dirent_info_t info;
    for (uint64_t i = 0; i <= idx; i++) {
        flatfs_err_t e = flatfs_dir_readdir(ino, &pos, &info);
        if (e == FLATFS_ERR_NOTFOUND) return -2;
        if (e) return xe(e);
    }

    de->ino  = info.ino;
    de->type = dt_to_vfs(info.type);
    int n = 0;
    while (info.name[n] && n < VFS_NAME_MAX) { de->name[n] = info.name[n]; n++; }
    de->name[n] = 0;
    return 0;
}

static const vfs_file_ops_t g_bridge_ops = {
    .open     = b_open,
    .close    = b_close,
    .read     = b_read,
    .write    = b_write,
    .truncate = b_truncate,
    .stat     = b_stat,
    .lstat    = b_lstat,
    .fstat    = b_fstat,
    .unlink   = b_unlink,
    .mkdir    = b_mkdir,
    .rmdir    = b_rmdir,
    .readdir  = b_readdir,
};

static int g_drv_id = -1;

flatfs_err_t flatfs_vfs_register(void)
{
    int id = vfs_register_driver("flatfs", "/", 0, 0);
    if (id < 0) return FLATFS_ERR_INVAL;
    vfs_set_driver_ops(id, &g_bridge_ops);
    g_drv_id = id;
    return FLATFS_OK;
}

flatfs_err_t flatfs_vfs_unregister(void)
{
    g_drv_id = -1;
    flatfs_unmount();
    return FLATFS_OK;
}
