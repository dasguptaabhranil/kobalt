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
#include "flatfs_dir.h"
#include "flatfs_hash.h"
#include "flatfs_inode.h"
#include "flatfs_alloc.h"
#include "flatfs_btree.h"

static uint8_t mode_to_dt(uint16_t mode)
{
    switch (mode & FLATFS_S_IFMT) {
    case FLATFS_S_IFREG:  return FLATFS_DT_REG;
    case FLATFS_S_IFDIR:  return FLATFS_DT_DIR;
    case FLATFS_S_IFLNK:  return FLATFS_DT_LNK;
    case FLATFS_S_IFBLK:  return FLATFS_DT_BLK;
    case FLATFS_S_IFCHR:  return FLATFS_DT_CHR;
    case FLATFS_S_IFIFO:  return FLATFS_DT_FIFO;
    case FLATFS_S_IFSOCK: return FLATFS_DT_SOCK;
    default:              return FLATFS_DT_UNKNOWN;
    }
}

flatfs_err_t flatfs_dir_create(uint64_t parent_ino, const char *name,
                                uint16_t mode, uint16_t uid, uint16_t gid,
                                uint64_t *out_ino)
{
    uint64_t new_ino;
    flatfs_err_t e = flatfs_inode_alloc(&new_ino);
    if (e) return e;

    flatfs_inode_t in;
    flatfs_inode_init(&in, new_ino,
                      FLATFS_S_IFDIR | (mode & 0x1FFu), uid, gid);
    in.nlink = 2;

    uint64_t rtblk;
    e = flatfs_alloc_block(&rtblk);
    if (e) { flatfs_inode_free(new_ino); return e; }

    e = flatfs_htree_init(new_ino, rtblk);
    if (e) { flatfs_free_block(rtblk); flatfs_inode_free(new_ino); return e; }

    flatfs_htree_insert(new_ino, ".", new_ino, FLATFS_DT_DIR);
    flatfs_htree_insert(new_ino, "..", parent_ino, FLATFS_DT_DIR);

    e = flatfs_inode_write(&in);
    if (e) { flatfs_free_block(rtblk); flatfs_inode_free(new_ino); return e; }

    e = flatfs_dir_insert(parent_ino, name, new_ino, mode_to_dt(FLATFS_S_IFDIR | mode));
    if (e) { flatfs_free_block(rtblk); flatfs_inode_free(new_ino); return e; }

    flatfs_inode_t par;
    if (flatfs_inode_read(parent_ino, &par) == FLATFS_OK) {
        par.nlink++;
        flatfs_inode_write(&par);
    }

    uint32_t h = flatfs_hash_name(name, fstrlen(name, FLATFS_NAME_MAX));
    flatfs_btree_insert((uint64_t)h, new_ino, new_ino);

    *out_ino = new_ino;
    return FLATFS_OK;
}

flatfs_err_t flatfs_dir_lookup(uint64_t dir_ino, const char *name,
                                uint64_t *out_ino)
{
    return flatfs_htree_lookup(dir_ino, name, out_ino);
}

flatfs_err_t flatfs_dir_insert(uint64_t dir_ino, const char *name,
                                uint64_t ino, uint8_t type)
{
    return flatfs_htree_insert(dir_ino, name, ino, type);
}

flatfs_err_t flatfs_dir_remove(uint64_t dir_ino, const char *name)
{
    return flatfs_htree_remove(dir_ino, name);
}

flatfs_err_t flatfs_dir_isempty(uint64_t dir_ino, int *empty)
{
    return flatfs_htree_isempty(dir_ino, empty);
}

flatfs_err_t flatfs_dir_readdir(uint64_t dir_ino, uint32_t *pos,
                                 flatfs_dirent_info_t *out)
{
    return flatfs_htree_iterate(dir_ino, pos, out);
}
