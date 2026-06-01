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
#include "flatfs_inode.h"
#include "flatfs_crc.h"

static uint64_t g_ino_hint = 1;

flatfs_err_t flatfs_inode_read(uint64_t ino, flatfs_inode_t *out)
{
    if (!ino || ino >= g_fs.super->total_inodes) return FLATFS_ERR_BOUNDS;
    FMEMCPY(out, inode_ptr(ino), FLATFS_INODE_SIZE);

    if (out->inode_magic == 0) return FLATFS_ERR_NOTFOUND;
    if (out->inode_magic != FLATFS_INODE_MAGIC) return FLATFS_ERR_CORRUPT;

    uint32_t sv = out->crc32;
    out->crc32 = 0;
    uint32_t got = flatfs_crc32(out, sizeof(*out));
    out->crc32 = sv;
    return (got == sv) ? FLATFS_OK : FLATFS_ERR_CRC;
}

flatfs_err_t flatfs_inode_write(const flatfs_inode_t *in)
{
    if (!in->ino || in->ino >= g_fs.super->total_inodes)
        return FLATFS_ERR_BOUNDS;

    flatfs_inode_t tmp;
    FMEMCPY(&tmp, in, sizeof(tmp));
    tmp.crc32 = 0;
    tmp.crc32 = flatfs_crc32(&tmp, sizeof(tmp));

    FMEMCPY(inode_ptr(in->ino), &tmp, FLATFS_INODE_SIZE);
    return FLATFS_OK;
}

flatfs_err_t flatfs_inode_alloc(uint64_t *out_ino)
{
    flatfs_super_t *sb = g_fs.super;
    if (!sb->free_inodes) return FLATFS_ERR_NOSPACE;

    uint64_t total = sb->total_inodes;
    uint64_t start = g_ino_hint;

    for (uint64_t i = 0; i < total; i++) {
        uint64_t ino = (start + i) % total;
        if (!ino) continue;
        flatfs_inode_t *slot = inode_ptr(ino);
        if (slot->inode_magic == 0) {
            g_ino_hint = ino + 1;
            sb->free_inodes--;
            *out_ino = ino;
            return FLATFS_OK;
        }
    }
    return FLATFS_ERR_NOSPACE;
}

flatfs_err_t flatfs_inode_free(uint64_t ino)
{
    if (!ino || ino >= g_fs.super->total_inodes) return FLATFS_ERR_BOUNDS;
    flatfs_inode_t *slot = inode_ptr(ino);
    if (!slot->inode_magic) return FLATFS_ERR_NOTFOUND;

    FMEMSET(slot, 0, FLATFS_INODE_SIZE);
    g_fs.super->free_inodes++;
    if (ino < g_ino_hint) g_ino_hint = ino;
    return FLATFS_OK;
}

flatfs_err_t flatfs_inode_init(flatfs_inode_t *in, uint64_t ino,
                                uint16_t mode, uint16_t uid, uint16_t gid)
{
    FMEMSET(in, 0, sizeof(*in));
    in->ino         = ino;
    in->mode        = mode;
    in->uid         = uid;
    in->gid         = gid;
    in->nlink       = 1;
    in->inode_magic = FLATFS_INODE_MAGIC;

    return FLATFS_OK;
}

flatfs_err_t flatfs_inode_getattr(uint64_t ino, flatfs_stat_t *out)
{
    flatfs_inode_t in;
    flatfs_err_t e = flatfs_inode_read(ino, &in);
    if (e) return e;

    out->ino      = in.ino;
    out->mode     = in.mode;
    out->uid      = in.uid;
    out->gid      = in.gid;
    out->nlink    = in.nlink;
    out->size     = in.size;
    out->blocks   = in.blocks;
    out->atime_ns = in.atime_ns;
    out->mtime_ns = in.mtime_ns;
    out->ctime_ns = in.ctime_ns;
    out->crtime_ns= in.crtime_ns;
    out->flags    = in.flags;
    out->blksize  = FLATFS_BLOCK_SIZE;
    return FLATFS_OK;
}

flatfs_err_t flatfs_inode_setattr(uint64_t ino, const flatfs_stat_t *attr,
                                   uint32_t mask)
{
    flatfs_inode_t in;
    flatfs_err_t e = flatfs_inode_read(ino, &in);
    if (e) return e;

    if (mask & FLATFS_ATTR_MODE)  in.mode     = attr->mode;
    if (mask & FLATFS_ATTR_UID)   in.uid      = attr->uid;
    if (mask & FLATFS_ATTR_GID)   in.gid      = attr->gid;
    if (mask & FLATFS_ATTR_ATIME) in.atime_ns = attr->atime_ns;
    if (mask & FLATFS_ATTR_MTIME) in.mtime_ns = attr->mtime_ns;

    return flatfs_inode_write(&in);
}
