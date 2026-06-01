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
#include "tmpfs_vfs.h"
#include <vfs/vfs.h>
#include <kmalloc.h>
#include <kernel.h>
#include <string.h>

static tmpfs_sb_t *g_sb = NULL;

typedef struct {
    tmpfs_sb_t    *sb;
    tmpfs_inode_t *ip;
} tf_hdl_t;

static const char *tf_strrchr(const char *s, int c)
{
    const char *last = NULL;
    while (*s) { if (*s == (char)c) last = s; s++; }
    return last;
}

static tmpfs_inode_t *path_resolve(tmpfs_sb_t *sb, const char *rel)
{
    tmpfs_inode_t *cur = sb->root;
    char comp[TMPFS_NAME_MAX + 1];

    if (*rel == '/') rel++;
    if (!*rel) return cur;

    while (*rel) {
        const char *end = rel;
        while (*end && *end != '/') end++;
        size_t n = (size_t)(end - rel);
        if (!n) { rel = end + 1; continue; }
        if (n > TMPFS_NAME_MAX) return NULL;
        memcpy(comp, rel, n);
        comp[n] = '\0';
        cur = tmpfs_dir_lookup(sb, cur, comp);
        if (!cur) return NULL;
        rel = *end ? end + 1 : end;
    }
    return cur;
}

static tmpfs_inode_t *path_parent(tmpfs_sb_t *sb, const char *rel,
                                   const char **leaf)
{
    const char *slash = tf_strrchr(rel, '/');
    if (!slash) { *leaf = rel; return sb->root; }
    if (slash == rel) { *leaf = rel + 1; return sb->root; }

    char pbuf[VFS_PATH_MAX];
    size_t plen = (size_t)(slash - rel);
    if (plen >= sizeof pbuf) return NULL;
    memcpy(pbuf, rel, plen);
    pbuf[plen] = '\0';
    *leaf = slash + 1;
    return path_resolve(sb, pbuf);
}

static void fill_stat(tmpfs_inode_t *ip, vfs_stat_t *st)
{
    st->ino   = ip->ino;
    st->mode  = ((uint32_t)ip->type << 12) | ip->perm;
    st->nlink = ip->nlink;
    st->size  = ip->size;
    st->mtime = ip->mtime;
    st->ctime = ip->ctime;
}

static uint8_t itype_vfs(uint16_t t)
{
    if (t == TMPFS_T_DIR) return VFS_DT_DIR;
    if (t == TMPFS_T_LNK) return VFS_DT_LNK;
    return VFS_DT_REG;
}

static int tf_open(const char *rel, int flags, uint32_t mode, void **priv_out)
{
    tmpfs_sb_t *sb = g_sb;
    tmpfs_inode_t *ip = path_resolve(sb, rel);

    if (!ip) {
        if (!(flags & VFS_O_CREAT)) return TMPFS_ENOENT;
        const char *leaf;
        tmpfs_inode_t *dir = path_parent(sb, rel, &leaf);
        if (!dir || !*leaf) return TMPFS_ENOENT;
        ip = tmpfs_inode_alloc(sb, TMPFS_T_REG, (uint16_t)(mode & 0xFFFu));
        if (!ip) return TMPFS_ENOMEM;
        int rc = tmpfs_dir_link(dir, leaf, ip->ino);
        if (rc) { tmpfs_inode_free(sb, ip); return rc; }
    }

    tf_hdl_t *h = kmalloc(sizeof *h);
    if (!h) return TMPFS_ENOMEM;
    h->sb = sb; h->ip = ip;
    *priv_out = h;
    return TMPFS_OK;
}

static void tf_close(void *priv) { kfree(priv); }

static int tf_read(void *priv, void *buf, uint64_t off, size_t len, size_t *got)
{ return tmpfs_file_read(((tf_hdl_t *)priv)->ip, buf, off, len, got); }

static int tf_write(void *priv, const void *buf, uint64_t off,
                    size_t len, size_t *put)
{ return tmpfs_file_write(((tf_hdl_t *)priv)->ip, buf, off, len, put); }

static int tf_truncate(void *priv, uint64_t sz)
{ return tmpfs_file_truncate(((tf_hdl_t *)priv)->ip, sz); }

static int tf_stat(const char *rel, vfs_stat_t *st)
{
    tmpfs_inode_t *ip = path_resolve(g_sb, rel);
    if (!ip) return TMPFS_ENOENT;
    fill_stat(ip, st);
    return TMPFS_OK;
}

static int tf_lstat(const char *rel, vfs_stat_t *st) { return tf_stat(rel, st); }

static int tf_fstat(void *priv, vfs_stat_t *st)
{ fill_stat(((tf_hdl_t *)priv)->ip, st); return TMPFS_OK; }

static int tf_unlink(const char *rel)
{
    const char *leaf;
    tmpfs_inode_t *dir = path_parent(g_sb, rel, &leaf);
    if (!dir) return TMPFS_ENOENT;
    tmpfs_inode_t *tgt = tmpfs_dir_lookup(g_sb, dir, leaf);
    if (!tgt) return TMPFS_ENOENT;
    if (tgt->type == TMPFS_T_DIR) return TMPFS_EISDIR;
    int rc = tmpfs_dir_unlink(dir, leaf);
    if (rc) return rc;
    if (--tgt->nlink == 0) tmpfs_inode_free(g_sb, tgt);
    return TMPFS_OK;
}

static int tf_mkdir(const char *rel, uint32_t mode)
{
    const char *leaf;
    tmpfs_inode_t *dir = path_parent(g_sb, rel, &leaf);
    if (!dir || !*leaf) return TMPFS_ENOENT;
    tmpfs_inode_t *ip = tmpfs_inode_alloc(g_sb, TMPFS_T_DIR,
                                           (uint16_t)(mode & 0xFFFu));
    if (!ip) return TMPFS_ENOMEM;
    ip->nlink = 2;
    int rc = tmpfs_dir_link(dir, leaf, ip->ino);
    if (rc) { tmpfs_inode_free(g_sb, ip); return rc; }
    dir->nlink++;
    return TMPFS_OK;
}

static int tf_rmdir(const char *rel)
{
    const char *leaf;
    tmpfs_inode_t *dir = path_parent(g_sb, rel, &leaf);
    if (!dir) return TMPFS_ENOENT;
    tmpfs_inode_t *tgt = tmpfs_dir_lookup(g_sb, dir, leaf);
    if (!tgt) return TMPFS_ENOENT;
    if (tgt->type != TMPFS_T_DIR) return TMPFS_ENOTDIR;
    if (tgt->de_count) return TMPFS_ENOTEMPTY;
    int rc = tmpfs_dir_unlink(dir, leaf);
    if (rc) return rc;
    if (dir->nlink > 0) dir->nlink--;
    tgt->nlink = 0;
    tmpfs_inode_free(g_sb, tgt);
    return TMPFS_OK;
}

static int tf_readdir(const char *rel, uint64_t idx, vfs_dirent_t *de)
{
    tmpfs_inode_t *dir = path_resolve(g_sb, rel);
    if (!dir) return TMPFS_ENOENT;
    if (dir->type != TMPFS_T_DIR) return TMPFS_ENOTDIR;
    uint64_t ino;
    char name[TMPFS_NAME_MAX + 1];
    int rc = tmpfs_dir_iterate(dir, idx, &ino, name);
    if (rc) return rc;
    de->ino  = ino;
    tmpfs_inode_t *child = tmpfs_inode_get(g_sb, ino);
    de->type = child ? itype_vfs(child->type) : VFS_DT_REG;
    memcpy(de->name, name, strnlen(name, TMPFS_NAME_MAX) + 1);
    return TMPFS_OK;
}

static const vfs_file_ops_t tmpfs_file_ops = {
    .open     = tf_open,
    .close    = tf_close,
    .read     = tf_read,
    .write    = tf_write,
    .truncate = tf_truncate,
    .stat     = tf_stat,
    .lstat    = tf_lstat,
    .fstat    = tf_fstat,
    .unlink   = tf_unlink,
    .mkdir    = tf_mkdir,
    .rmdir    = tf_rmdir,
    .readdir  = tf_readdir,
};

static int tf_mount(void)
{
    if (g_sb) { klog_warn("tmpfs", "already mounted"); return TMPFS_OK; }
    g_sb = tmpfs_sb_alloc();
    return g_sb ? TMPFS_OK : TMPFS_ENOMEM;
}

static int tf_umount(void)
{
    if (!g_sb) return TMPFS_OK;
    tmpfs_sb_destroy(g_sb);
    g_sb = NULL;
    return TMPFS_OK;
}

void tmpfs_vfs_register(void)
{
    int id = vfs_register_driver("tmpfs", "/tmp", tf_mount, tf_umount);
    vfs_set_driver_ops(id, &tmpfs_file_ops);
    klog_ok("tmpfs", "registered at /tmp");
}
