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

#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

#define VFS_MAX_DRIVERS  16
#define VFS_MAX_FDS      256
#define VFS_PATH_MAX     512
#define VFS_NAME_MAX     255

#define VFS_O_RDONLY    0x000
#define VFS_O_WRONLY    0x001
#define VFS_O_RDWR      0x002
#define VFS_O_CREAT     0x040
#define VFS_O_TRUNC     0x200
#define VFS_O_APPEND    0x400
#define VFS_O_DIRECTORY 0x10000

#define VFS_DT_REG   1
#define VFS_DT_DIR   2
#define VFS_DT_LNK   3

typedef struct {
    uint64_t ino;
    uint32_t mode;
    uint32_t nlink;
    uint64_t size;
    uint32_t mtime;
    uint32_t ctime;
} vfs_stat_t;

typedef struct {
    uint64_t ino;
    uint8_t  type;
    char     name[VFS_NAME_MAX + 1];
} vfs_dirent_t;

typedef struct {
    int  (*open)        (const char *rel, int flags, uint32_t mode, void **priv);
    void (*close)       (void *priv);
    int  (*read)        (void *priv, void       *buf, uint64_t off, size_t len, size_t *got);
    int  (*write)       (void *priv, const void *buf, uint64_t off, size_t len, size_t *put);
    int  (*truncate)    (void *priv, uint64_t sz);
    int  (*stat)        (const char *rel, vfs_stat_t *st);
    int  (*lstat)       (const char *rel, vfs_stat_t *st);
    int  (*fstat)       (void *priv, vfs_stat_t *st);
    int  (*unlink)      (const char *rel);
    int  (*mkdir)       (const char *rel, uint32_t mode);
    int  (*rmdir)       (const char *rel);
    int  (*readdir)     (const char *rel, uint64_t idx, vfs_dirent_t *de);
    int  (*rename)      (const char *old_rel, const char *new_rel);
    int  (*symlink)     (const char *target,  const char *link_rel);
    int  (*readlink)    (const char *rel, char *buf, size_t bufsz);
    int  (*path_truncate)(const char *rel, uint64_t sz);
} vfs_file_ops_t;

void    vfs_init(void);

int     vfs_register_driver(const char *name, const char *mnt,
                             int (*mount_fn)(void), int (*umount_fn)(void));
void    vfs_set_driver_ops(int drv_id, const vfs_file_ops_t *ops);
void    vfs_invalidate_driver(int drv_id);

int     vfs_open        (const char *path, int flags, uint32_t mode);
void    vfs_close       (int fd);
int     vfs_read        (int fd, void       *buf, size_t len);
int     vfs_write       (int fd, const void *buf, size_t len);
int     vfs_seek        (int fd, int64_t off, int whence);
int64_t vfs_tell        (int fd);
int     vfs_truncate    (int fd, uint64_t sz);
int     vfs_stat        (const char *path, vfs_stat_t *st);
int     vfs_lstat       (const char *path, vfs_stat_t *st);
int     vfs_fstat       (int fd, vfs_stat_t *st);
int     vfs_unlink      (const char *path);
int     vfs_mkdir       (const char *path, uint32_t mode);
int     vfs_rmdir       (const char *path);
int     vfs_readdir     (const char *path, uint64_t idx, vfs_dirent_t *de);
int     vfs_rename      (const char *oldpath, const char *newpath);
int     vfs_symlink     (const char *target,  const char *linkpath);
int     vfs_readlink    (const char *path, char *buf, size_t bufsz);
int     vfs_path_truncate(const char *path, uint64_t sz);

#ifndef NDEBUG
void vfs_debug_dump(void);
#endif

#endif
