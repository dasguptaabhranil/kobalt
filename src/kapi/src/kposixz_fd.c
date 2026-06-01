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

#include "kposixz_internal.h"
#include <vfs.h>
#include "../../fs/devfs/devfs.h"
#include "../../fs/devfs/devfs_tykid.h"

extern void uart_putc(char c);
extern s32  uart_getc(void);
extern s32  uart_getc_blocking(void);

#define D_NAME_OFF  19u

static s64 kfs_vfs_read(kposixz_file_t *f, void *buf, u64 len)
{
    kpz_kfs_priv_t *priv = (kpz_kfs_priv_t *)f->priv;
    if (priv->is_dir) return KPZ_ERR(KPZE_ISDIR);
    if (f->pos >= (kpz_off_t)priv->size) return 0;

    usz avail = priv->size - (usz)f->pos;
    usz n = len < avail ? (usz)len : avail;
    vfs_seek(priv->vfs_fd, f->pos, 0);
    int got = vfs_read(priv->vfs_fd, buf, n);
    if (got < 0) return KPZ_ERR(KPZE_IO);
    f->pos += (kpz_off_t)got;
    return (s64)got;
}

static s64 kfs_vfs_write(kposixz_file_t *f, const void *buf, u64 len)
{
    (void)f; (void)buf; (void)len;
    return KPZ_ERR(KPZE_ROFS);
}

static s64 kfs_vfs_seek(kposixz_file_t *f, s64 off, u32 whence)
{
    kpz_kfs_priv_t *priv = (kpz_kfs_priv_t *)f->priv;
    kpz_off_t new_pos;
    usz bound = priv->is_dir ? 0 : priv->size;

    switch (whence) {
    case KPZ_SEEK_SET: new_pos = off;                       break;
    case KPZ_SEEK_CUR: new_pos = f->pos + off;             break;
    case KPZ_SEEK_END: new_pos = (kpz_off_t)bound + off;  break;
    default: return KPZ_ERR(KPZE_INVAL);
    }

    if (new_pos < 0) return KPZ_ERR(KPZE_INVAL);
    f->pos = new_pos;
    return (s64)new_pos;
}

static s64 kfs_vfs_stat(kposixz_file_t *f, kpz_stat_t *st)
{
    kpz_kfs_priv_t *priv = (kpz_kfs_priv_t *)f->priv;
    kpz_memzero(st, sizeof(*st));

    if (priv->is_dir) {
        st->st_mode    = KPZ_S_IFDIR | 0555;
        st->st_nlink   = 2;
        st->st_blksize = 4096;
    } else {
        st->st_mode    = KPZ_S_IFREG | 0444;
        st->st_nlink   = 1;
        st->st_size    = (kpz_off_t)priv->size;
        st->st_blksize = 4096;
        st->st_blocks  = (st->st_size + 511) / 512;
        st->st_ino     = (kpz_ino_t)priv->vfs_fd;
    }
    return 0;
}

static s64 kfs_vfs_getdents(kposixz_file_t *f, void *buf, u64 len)
{
    kpz_kfs_priv_t *priv = (kpz_kfs_priv_t *)f->priv;
    if (!priv->is_dir) return KPZ_ERR(KPZE_NOTDIR);

    u8 *p = (u8 *)buf;
    u64 filled = 0;

    for (;;) {
        vfs_dirent_t de;
        if (vfs_readdir(priv->dirpath, (u64)f->pos, &de) < 0) break;

        usz namelen = kpz_strlen(de.name);
        u16 reclen = (u16)((D_NAME_OFF + namelen + 1u + 7u) & ~7u);

        if (filled + reclen > len) break;

        kpz_dirent64_t *d = (kpz_dirent64_t *)(p + filled);
        d->d_ino    = de.ino ? de.ino : (u64)(f->pos + 1);
        d->d_off    = (s64)(f->pos + 1);
        d->d_reclen = reclen;
        d->d_type   = (de.type == VFS_DT_DIR) ? 4 :
                      (de.type == VFS_DT_LNK) ? 10 : 8;
        kpz_strncpy(d->d_name, de.name, namelen + 1);

        filled += reclen;
        f->pos++;
    }

    return (s64)filled;
}

static u32 kfs_vfs_poll(kposixz_file_t *f, u32 events)
{
    (void)f;
    return events & (KPZ_POLLIN | KPZ_POLLOUT);
}

static void kfs_vfs_close(kposixz_file_t *f)
{
    kpz_kfs_priv_t *priv = (kpz_kfs_priv_t *)f->priv;
    if (priv && !priv->is_dir && priv->vfs_fd >= 0)
        vfs_close(priv->vfs_fd);
}

static const kpz_vfs_ops_t kpz_kfs_ops = {
    .read     = kfs_vfs_read,
    .write    = kfs_vfs_write,
    .seek     = kfs_vfs_seek,
    .stat     = kfs_vfs_stat,
    .close    = kfs_vfs_close,
    .ioctl    = (void *)0,
    .getdents = (void *)0,
    .poll     = kfs_vfs_poll,
};

static const kpz_vfs_ops_t kpz_kfs_dir_ops = {
    .read     = kfs_vfs_read,
    .write    = kfs_vfs_write,
    .seek     = kfs_vfs_seek,
    .stat     = kfs_vfs_stat,
    .close    = kfs_vfs_close,
    .ioctl    = (void *)0,
    .getdents = kfs_vfs_getdents,
    .poll     = kfs_vfs_poll,
};

kposixz_file_t *kpz_kfs_open(const char *path, u32 flags)
{
    vfs_stat_t vs;
    int is_dir = 0;

    int vfd = vfs_open(path, (int)flags, 0444);
    if (vfd < 0) {
        if (vfs_stat(path, &vs) == 0 && (vs.mode & 0170000) == 0040000)
            is_dir = 1;
        else
            return (void *)0;
    }

    kposixz_file_t *f = (kposixz_file_t *)kmalloc(
                            sizeof(kposixz_file_t) + sizeof(kpz_kfs_priv_t));
    if (!f) {
        if (vfd >= 0) vfs_close(vfd);
        return (void *)0;
    }

    kpz_memzero(f, sizeof(*f) + sizeof(kpz_kfs_priv_t));
    kpz_kfs_priv_t *priv = (kpz_kfs_priv_t *)(f + 1);

    if (is_dir || (vfd >= 0 && vfs_fstat(vfd, &vs) == 0 &&
                   (vs.mode & 0170000) == 0040000)) {
        if (vfd >= 0) { vfs_close(vfd); vfd = -1; }
        priv->is_dir  = 1;
        priv->vfs_fd  = -1;
        priv->size    = 0;
        kpz_strncpy(priv->dirpath, path, sizeof(priv->dirpath));
        f->ops  = &kpz_kfs_dir_ops;
        f->mode = KPZ_S_IFDIR | 0555;
    } else {
        priv->vfs_fd = vfd;
        priv->is_dir = 0;
        if (vfs_fstat(vfd, &vs) == 0) priv->size = (usz)vs.size;
        f->ops  = &kpz_kfs_ops;
        f->mode = KPZ_S_IFREG | 0444;
    }

    f->priv     = priv;
    f->pos      = 0;
    f->flags    = flags;
    f->refcount = 1;

    return f;
}

static s64 kpz_devfs_node_read(kposixz_file_t *f, void *buf, u64 len)
{
    devfs_file_t *df = (devfs_file_t *)f->priv;
    df->pos = (uint64_t)f->pos;
    ssize_t n = devfs_cdev_read(df, buf, (size_t)len);
    if (n < 0) return KPZ_ERR(KPZE_IO);
    f->pos += (kpz_off_t)n;
    return (s64)n;
}

static s64 kpz_devfs_node_write(kposixz_file_t *f, const void *buf, u64 len)
{
    devfs_file_t *df = (devfs_file_t *)f->priv;
    df->pos = (uint64_t)f->pos;
    ssize_t n = devfs_cdev_write(df, buf, (size_t)len);
    if (n < 0) return KPZ_ERR(KPZE_IO);
    f->pos += (kpz_off_t)n;
    return (s64)n;
}

static s64 kpz_devfs_node_seek(kposixz_file_t *f, s64 off, u32 whence)
{
    kpz_off_t new_pos;
    switch (whence) {
    case KPZ_SEEK_SET: new_pos = off;          break;
    case KPZ_SEEK_CUR: new_pos = f->pos + off; break;
    default: return KPZ_ERR(KPZE_INVAL);
    }
    if (new_pos < 0) return KPZ_ERR(KPZE_INVAL);
    f->pos = new_pos;
    return (s64)new_pos;
}

static s64 kpz_devfs_node_stat(kposixz_file_t *f, kpz_stat_t *st)
{
    devfs_file_t *df = (devfs_file_t *)f->priv;
    if (!df || !df->node) return KPZ_ERR(KPZE_BADF);
    kpz_memzero(st, sizeof(*st));
    st->st_mode  = KPZ_S_IFCHR | (kpz_mode_t)(df->node->mode & 0777u);
    st->st_ino   = (kpz_ino_t)(uptr)df->node;
    st->st_nlink = 1;
    return 0;
}

static s64 kpz_devfs_node_ioctl(kposixz_file_t *f, u64 req, u64 arg)
{
    devfs_file_t *df = (devfs_file_t *)f->priv;
    int rc = devfs_cdev_ioctl(df, (unsigned long)req, (void *)(uptr)arg);
    return (s64)rc;
}

static u32 kpz_devfs_node_poll(kposixz_file_t *f, u32 events)
{
    (void)f;
    return events & (KPZ_POLLIN | KPZ_POLLOUT);
}

static void kpz_devfs_node_close(kposixz_file_t *f)
{
    devfs_cdev_close((devfs_file_t *)f->priv);
    f->priv = (void *)0;
}

static const kpz_vfs_ops_t kpz_devfs_node_ops = {
    .read     = kpz_devfs_node_read,
    .write    = kpz_devfs_node_write,
    .seek     = kpz_devfs_node_seek,
    .stat     = kpz_devfs_node_stat,
    .ioctl    = kpz_devfs_node_ioctl,
    .close    = kpz_devfs_node_close,
    .getdents = (void *)0,
    .poll     = kpz_devfs_node_poll,
};

kposixz_file_t *kpz_devfs_open(const char *path, u32 flags)
{
    if (!path) return (void *)0;
    devfs_node_t *node = devfs_lookup_path(path);
    if (!node) return (void *)0;
    if (node->type == DEVFS_TYPE_DIR) return (void *)0;
    devfs_file_t *df = devfs_cdev_open(node, (int)flags);
    if (!df) return (void *)0;

    kposixz_file_t *f = (kposixz_file_t *)kmalloc(sizeof(kposixz_file_t));
    if (!f) { devfs_cdev_close(df); return (void *)0; }

    kpz_memzero(f, sizeof(*f));
    f->ops      = &kpz_devfs_node_ops;
    f->priv     = (void *)df;
    f->pos      = 0;
    f->flags    = flags;
    f->mode     = KPZ_S_IFCHR | (kpz_mode_t)(node->mode & 0777u);
    f->refcount = 1;
    return f;
}

static s64 devfs_stdin_read(kposixz_file_t *f, void *buf, u64 len)
{
    (void)f;
    u8 *p = (u8 *)buf;
    for (u64 i = 0; i < len; i++) {
        s32 c = uart_getc_blocking();
        if (c < 0) return (s64)i;
        p[i] = (u8)c;
        if (c == '\n') return (s64)(i + 1);
    }
    return (s64)len;
}

static s64 devfs_no_write(kposixz_file_t *f, const void *buf, u64 len)
{
    (void)f; (void)buf; (void)len;
    return KPZ_ERR(KPZE_BADF);
}

static s64 devfs_stdin_stat(kposixz_file_t *f, kpz_stat_t *st)
{
    (void)f;
    kpz_memzero(st, sizeof(*st));
    st->st_mode = KPZ_S_IFCHR | 0444;
    st->st_ino  = 1;
    return 0;
}

static s64 devfs_out_write(kposixz_file_t *f, const void *buf, u64 len)
{
    (void)f;
    const u8 *p = (const u8 *)buf;
    for (u64 i = 0; i < len; i++) uart_putc((char)p[i]);
    return (s64)len;
}

static s64 devfs_no_read(kposixz_file_t *f, void *buf, u64 len)
{
    (void)f; (void)buf; (void)len;
    return KPZ_ERR(KPZE_BADF);
}

static s64 devfs_out_stat(kposixz_file_t *f, kpz_stat_t *st)
{
    (void)f;
    kpz_memzero(st, sizeof(*st));
    st->st_mode = KPZ_S_IFCHR | 0222;
    st->st_ino  = 2;
    return 0;
}

static s64 devfs_ioctl(kposixz_file_t *f, u64 req, u64 arg)
{
    (void)f;
    if (req == KPZ_TIOCGWINSZ) {
        kpz_winsize_t *ws = (kpz_winsize_t *)arg;
        if (!ws) return KPZ_ERR(KPZE_FAULT);
        ws->ws_row = 24; ws->ws_col = 80;
        ws->ws_xpixel = 0; ws->ws_ypixel = 0;
        return 0;
    }
    return 0;
}

static u32 devfs_stdin_poll(kposixz_file_t *f, u32 events)
{
    (void)f;
    return events & KPZ_POLLIN;
}

static u32 devfs_out_poll(kposixz_file_t *f, u32 events)
{
    (void)f;
    return events & KPZ_POLLOUT;
}

static void devfs_noop_close(kposixz_file_t *f) { (void)f; }

static const kpz_vfs_ops_t kpz_devfs_stdin_ops = {
    .read  = devfs_stdin_read,
    .write = devfs_no_write,
    .seek  = (void *)0,
    .stat  = devfs_stdin_stat,
    .ioctl = devfs_ioctl,
    .close = devfs_noop_close,
    .poll  = devfs_stdin_poll,
};

static const kpz_vfs_ops_t kpz_devfs_stdout_ops = {
    .read  = devfs_no_read,
    .write = devfs_out_write,
    .seek  = (void *)0,
    .stat  = devfs_out_stat,
    .ioctl = devfs_ioctl,
    .close = devfs_noop_close,
    .poll  = devfs_out_poll,
};

static kposixz_file_t kpz_stdin_file = {
    .ops = &kpz_devfs_stdin_ops, .priv = (void *)0, .pos = 0,
    .flags = KPZ_O_RDONLY, .mode = KPZ_S_IFCHR | 0444, .refcount = 1,
};
static kposixz_file_t kpz_stdout_file = {
    .ops = &kpz_devfs_stdout_ops, .priv = (void *)0, .pos = 0,
    .flags = KPZ_O_WRONLY, .mode = KPZ_S_IFCHR | 0222, .refcount = 1,
};
static kposixz_file_t kpz_stderr_file = {
    .ops = &kpz_devfs_stdout_ops, .priv = (void *)0, .pos = 0,
    .flags = KPZ_O_WRONLY, .mode = KPZ_S_IFCHR | 0222, .refcount = 1,
};

kposixz_file_t *kpz_devfs_open_stdin(void)  { kpz_atomic_inc(&kpz_stdin_file.refcount);  return &kpz_stdin_file;  }
kposixz_file_t *kpz_devfs_open_stdout(void) { kpz_atomic_inc(&kpz_stdout_file.refcount); return &kpz_stdout_file; }
kposixz_file_t *kpz_devfs_open_stderr(void) { kpz_atomic_inc(&kpz_stderr_file.refcount); return &kpz_stderr_file; }

s32 kpz_fd_alloc(kposixz_proc_t *proc, kposixz_file_t *file)
{
    kpz_spin_lock(&proc->fds.lock);
    for (s32 i = 0; i < KPOSIXZ_MAX_FD; i++) {
        if (!proc->fds.files[i]) {
            proc->fds.files[i]   = file;
            proc->fds.cloexec[i] = 0;
            kpz_spin_unlock(&proc->fds.lock);
            return i;
        }
    }
    kpz_spin_unlock(&proc->fds.lock);
    return -KPZE_MFILE;
}

kposixz_file_t *kpz_fd_get(kposixz_proc_t *proc, kpz_fd_t fd)
{
    if (fd < 0 || fd >= KPOSIXZ_MAX_FD) return (void *)0;
    kpz_spin_lock(&proc->fds.lock);
    kposixz_file_t *f = proc->fds.files[fd];
    if (f) kpz_atomic_inc(&f->refcount);
    kpz_spin_unlock(&proc->fds.lock);
    return f;
}

void kpz_fd_put(kposixz_file_t *file)
{
    if (!file) return;
    if (kpz_atomic_dec(&file->refcount) == 0) {
        if (file->ops && file->ops->close)
            file->ops->close(file);
        if (file != &kpz_stdin_file  &&
            file != &kpz_stdout_file &&
            file != &kpz_stderr_file)
            kfree(file);
    }
}

s32 kpz_fd_close(kposixz_proc_t *proc, kpz_fd_t fd)
{
    if (fd < 0 || fd >= KPOSIXZ_MAX_FD) return -KPZE_BADF;
    kpz_spin_lock(&proc->fds.lock);
    kposixz_file_t *f = proc->fds.files[fd];
    if (!f) { kpz_spin_unlock(&proc->fds.lock); return -KPZE_BADF; }
    proc->fds.files[fd]   = (void *)0;
    proc->fds.cloexec[fd] = 0;
    kpz_spin_unlock(&proc->fds.lock);
    kpz_fd_put(f);
    return 0;
}

s32 kpz_fd_dup(kposixz_proc_t *proc, kpz_fd_t oldfd)
{
    kposixz_file_t *f = kpz_fd_get(proc, oldfd);
    if (!f) return -KPZE_BADF;
    s32 newfd = kpz_fd_alloc(proc, f);
    if (newfd < 0) kpz_fd_put(f);
    return newfd;
}

s32 kpz_fd_dup2(kposixz_proc_t *proc, kpz_fd_t oldfd, kpz_fd_t newfd)
{
    if (newfd < 0 || newfd >= KPOSIXZ_MAX_FD) return -KPZE_BADF;
    if (oldfd == newfd) return newfd;

    kposixz_file_t *f = kpz_fd_get(proc, oldfd);
    if (!f) return -KPZE_BADF;

    kpz_spin_lock(&proc->fds.lock);
    kposixz_file_t *old       = proc->fds.files[newfd];
    proc->fds.files[newfd]   = f;
    proc->fds.cloexec[newfd] = 0;
    kpz_spin_unlock(&proc->fds.lock);

    if (old) kpz_fd_put(old);
    return newfd;
}
