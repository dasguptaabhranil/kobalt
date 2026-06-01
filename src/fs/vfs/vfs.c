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

#include "vfs.h"

#define KMEMSET(d,c,n) __builtin_memset((d),(c),(n))

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define E_NOENT  (-2)
#define E_BADF   (-9)
#define E_NOMEM  (-12)
#define E_EXIST  (-17)
#define E_XDEV   (-18)
#define E_NOTDIR (-20)
#define E_ISDIR  (-21)
#define E_INVAL  (-22)
#define E_NOSPC  (-28)
#define E_NOSYS  (-38)

static volatile int g_fd_lk;
#define FD_LOCK()   do { while (__atomic_test_and_set(&g_fd_lk, __ATOMIC_SEQ_CST)) \
                        __asm__ volatile("pause" ::: "memory"); } while (0)
#define FD_UNLOCK() __atomic_clear(&g_fd_lk, __ATOMIC_RELEASE)

static inline size_t vstrlen(const char *s)
    { size_t n = 0; while (s[n]) n++; return n; }

static inline int vstrcmp_n(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (!a[i]) return 0;
    }
    return 0;
}

static inline void vstrncpy(char *d, const char *s, size_t n)
{
    size_t i;
    for (i = 0; i < n-1 && s[i]; i++) d[i] = s[i];
    d[i] = '\0';
}

typedef struct {
    int            valid;
    char           name[32];
    char           mnt[VFS_PATH_MAX];
    size_t         mnt_len;
    int          (*umount_fn)(void);
    vfs_file_ops_t ops;
} drv_t;

static drv_t g_drv[VFS_MAX_DRIVERS];
static int   g_ndrv;

struct vfs_file {
    int     used;
    int     drv;
    void   *priv;
    int64_t pos;
    int     flags;
};

static struct vfs_file g_fd[VFS_MAX_FDS];

static int route(const char *path, const char **rel)
{
    int    best     = -1;
    size_t best_len = 0;

    for (int i = 0; i < g_ndrv; i++) {
        if (!g_drv[i].valid) continue;
        size_t ml = g_drv[i].mnt_len;
        if (vstrcmp_n(path, g_drv[i].mnt, ml) != 0) continue;
        if (path[ml] != '\0' && path[ml] != '/' && ml > 1) continue;
        if (ml >= best_len) { best = i; best_len = ml; }
    }
    if (best < 0) return -1;

    const char *r = path + best_len;
    if (*r == '/') r++;
    *rel = r;
    return best;
}

static int alloc_fd(void)
{
    FD_LOCK();
    for (int i = 0; i < VFS_MAX_FDS; i++) {
        if (!g_fd[i].used) {
            g_fd[i].used = 1;
            g_fd[i].priv = 0;
            FD_UNLOCK();
            return i;
        }
    }
    FD_UNLOCK();
    return E_NOMEM;
}

void vfs_init(void) {}

int vfs_register_driver(const char *name, const char *mnt,
                         int (*mount_fn)(void), int (*umount_fn)(void))
{
    if (g_ndrv >= VFS_MAX_DRIVERS) return E_NOSPC;

    int id = g_ndrv++;
    drv_t *d = &g_drv[id];
    KMEMSET(d, 0, sizeof *d);

    vstrncpy(d->name, name, sizeof d->name);
    vstrncpy(d->mnt,  mnt,  sizeof d->mnt);
    d->mnt_len   = vstrlen(d->mnt);
    d->umount_fn = umount_fn;

    if (mount_fn) {
        int rc = mount_fn();
        if (rc < 0) { g_ndrv--; return rc; }
    }

    d->valid = 1;
    return id;
}

void vfs_set_driver_ops(int drv_id, const vfs_file_ops_t *ops)
{
    if (drv_id < 0 || drv_id >= g_ndrv || !g_drv[drv_id].valid) return;
    g_drv[drv_id].ops = *ops;
}

void vfs_invalidate_driver(int drv_id)
{
    if (drv_id < 0 || drv_id >= g_ndrv) return;
    static const vfs_file_ops_t z = {0};
    g_drv[drv_id].ops   = z;
    g_drv[drv_id].valid = 0;
}

int vfs_open(const char *path, int flags, uint32_t mode)
{
    const char *rel;
    int d = route(path, &rel);
    if (d < 0 || !g_drv[d].ops.open) return E_NOENT;

    int fd = alloc_fd();
    if (fd < 0) return fd;

    void *priv = 0;
    int rc = g_drv[d].ops.open(rel, flags, mode, &priv);
    if (rc < 0) {
        g_fd[fd].used = 0;
        return rc;
    }

    g_fd[fd].drv   = d;
    g_fd[fd].priv  = priv;
    g_fd[fd].pos   = 0;
    g_fd[fd].flags = flags;
    return fd;
}

void vfs_close(int fd)
{
    if (fd < 0 || fd >= VFS_MAX_FDS) return;

    FD_LOCK();
    if (!g_fd[fd].used) { FD_UNLOCK(); return; }
    int   d    = g_fd[fd].drv;
    void *priv = g_fd[fd].priv;
    g_fd[fd].used = 0;
    g_fd[fd].priv = 0;
    FD_UNLOCK();

    if (g_drv[d].ops.close)
        g_drv[d].ops.close(priv);
}

int vfs_read(int fd, void *buf, size_t len)
{
    if (fd < 0 || fd >= VFS_MAX_FDS || !g_fd[fd].used) return E_BADF;
    int d = g_fd[fd].drv;
    if (!g_drv[d].ops.read) return E_BADF;
    size_t got = 0;
    int rc = g_drv[d].ops.read(g_fd[fd].priv, buf,
                                (uint64_t)g_fd[fd].pos, len, &got);
    if (rc < 0) return rc;
    g_fd[fd].pos += (int64_t)got;
    return (int)got;
}

int vfs_write(int fd, const void *buf, size_t len)
{
    if (fd < 0 || fd >= VFS_MAX_FDS || !g_fd[fd].used) return E_BADF;
    int d = g_fd[fd].drv;
    if (!g_drv[d].ops.write) return E_BADF;

    int64_t off = g_fd[fd].pos;
    if (g_fd[fd].flags & VFS_O_APPEND) {
        vfs_stat_t st;
        if (g_drv[d].ops.fstat &&
            g_drv[d].ops.fstat(g_fd[fd].priv, &st) == 0)
            off = (int64_t)st.size;
    }

    size_t put = 0;
    int rc = g_drv[d].ops.write(g_fd[fd].priv, buf, (uint64_t)off, len, &put);
    if (rc < 0) return rc;
    g_fd[fd].pos = off + (int64_t)put;
    return (int)put;
}

int vfs_seek(int fd, int64_t off, int whence)
{
    if (fd < 0 || fd >= VFS_MAX_FDS || !g_fd[fd].used) return E_BADF;

    int64_t newpos;
    if (whence == SEEK_SET) {
        newpos = off;
    } else if (whence == SEEK_CUR) {
        newpos = g_fd[fd].pos + off;
    } else if (whence == SEEK_END) {
        int d = g_fd[fd].drv;
        if (!g_drv[d].ops.fstat) return E_INVAL;
        vfs_stat_t st;
        int rc = g_drv[d].ops.fstat(g_fd[fd].priv, &st);
        if (rc < 0) return rc;
        newpos = (int64_t)st.size + off;
    } else {
        return E_INVAL;
    }

    if (newpos < 0) return E_INVAL;
    g_fd[fd].pos = newpos;
    return 0;
}

int64_t vfs_tell(int fd)
{
    if (fd < 0 || fd >= VFS_MAX_FDS || !g_fd[fd].used) return E_BADF;
    return g_fd[fd].pos;
}

int vfs_truncate(int fd, uint64_t sz)
{
    if (fd < 0 || fd >= VFS_MAX_FDS || !g_fd[fd].used) return E_BADF;
    int d = g_fd[fd].drv;
    if (!g_drv[d].ops.truncate) return E_NOSYS;
    return g_drv[d].ops.truncate(g_fd[fd].priv, sz);
}

int vfs_stat(const char *path, vfs_stat_t *st)
{
    const char *rel;
    int d = route(path, &rel);
    if (d < 0 || !g_drv[d].ops.stat) return E_NOENT;
    return g_drv[d].ops.stat(rel, st);
}

int vfs_lstat(const char *path, vfs_stat_t *st)
{
    const char *rel;
    int d = route(path, &rel);
    if (d < 0) return E_NOENT;
    if (g_drv[d].ops.lstat) return g_drv[d].ops.lstat(rel, st);
    if (g_drv[d].ops.stat)  return g_drv[d].ops.stat(rel, st);
    return E_NOSYS;
}

int vfs_fstat(int fd, vfs_stat_t *st)
{
    if (fd < 0 || fd >= VFS_MAX_FDS || !g_fd[fd].used) return E_BADF;
    int d = g_fd[fd].drv;
    if (!g_drv[d].ops.fstat) return E_NOSYS;
    return g_drv[d].ops.fstat(g_fd[fd].priv, st);
}

int vfs_unlink(const char *path)
{
    const char *rel;
    int d = route(path, &rel);
    if (d < 0 || !g_drv[d].ops.unlink) return E_NOENT;
    return g_drv[d].ops.unlink(rel);
}

int vfs_mkdir(const char *path, uint32_t mode)
{
    const char *rel;
    int d = route(path, &rel);
    if (d < 0 || !g_drv[d].ops.mkdir) return E_NOSYS;
    return g_drv[d].ops.mkdir(rel, mode);
}

int vfs_rmdir(const char *path)
{
    const char *rel;
    int d = route(path, &rel);
    if (d < 0 || !g_drv[d].ops.rmdir) return E_NOSYS;
    return g_drv[d].ops.rmdir(rel);
}

int vfs_readdir(const char *path, uint64_t idx, vfs_dirent_t *de)
{
    const char *rel;
    int d = route(path, &rel);
    if (d < 0 || !g_drv[d].ops.readdir) return E_NOSYS;
    return g_drv[d].ops.readdir(rel, idx, de);
}

int vfs_rename(const char *oldpath, const char *newpath)
{
    const char *old_rel, *new_rel;
    int od = route(oldpath, &old_rel);
    int nd = route(newpath, &new_rel);
    if (od < 0) return E_NOENT;
    if (nd < 0) return E_NOENT;
    if (od != nd) return E_XDEV;
    if (!g_drv[od].ops.rename) return E_NOSYS;
    return g_drv[od].ops.rename(old_rel, new_rel);
}

int vfs_symlink(const char *target, const char *linkpath)
{
    const char *rel;
    int d = route(linkpath, &rel);
    if (d < 0 || !g_drv[d].ops.symlink) return E_NOSYS;
    return g_drv[d].ops.symlink(target, rel);
}

int vfs_readlink(const char *path, char *buf, size_t bufsz)
{
    const char *rel;
    int d = route(path, &rel);
    if (d < 0 || !g_drv[d].ops.readlink) return E_NOSYS;
    return g_drv[d].ops.readlink(rel, buf, bufsz);
}

int vfs_path_truncate(const char *path, uint64_t sz)
{
    const char *rel;
    int d = route(path, &rel);
    if (d < 0) return E_NOENT;
    if (g_drv[d].ops.path_truncate) return g_drv[d].ops.path_truncate(rel, sz);
    if (!g_drv[d].ops.open || !g_drv[d].ops.truncate) return E_NOSYS;

    void *priv = 0;
    int rc = g_drv[d].ops.open(rel, VFS_O_WRONLY, 0, &priv);
    if (rc < 0) return rc;
    rc = g_drv[d].ops.truncate(priv, sz);
    if (g_drv[d].ops.close) g_drv[d].ops.close(priv);
    return rc;
}

#ifndef NDEBUG
extern int kprintf(const char *fmt, ...);
void vfs_debug_dump(void)
{
    kprintf("[VFS] drivers=%d\n", g_ndrv);
    for (int i = 0; i < g_ndrv; i++) {
        if (!g_drv[i].valid) continue;
        kprintf("  drv[%d] '%s' @ '%s'\n", i, g_drv[i].name, g_drv[i].mnt);
    }
    int open = 0;
    for (int i = 0; i < VFS_MAX_FDS; i++) if (g_fd[i].used) open++;
    kprintf("  fds: %d/%d\n", open, VFS_MAX_FDS);
}
#endif
