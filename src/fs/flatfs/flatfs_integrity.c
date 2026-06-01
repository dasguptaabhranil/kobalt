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
#include "flatfs_integrity.h"
#include "flatfs_tykid.h"
#include "flatfs_crc.h"
#include "flatfs_inode.h"

flatfs_err_t flatfs_integrity_check_block(uint64_t blk)
{
    if (!blk_valid(blk)) return FLATFS_ERR_BOUNDS;
    void *p = blk_ptr(blk);

    if (!flatfs_block_crc_ok(p, FLATFS_BLOCK_SIZE)) {
        flatfs_tykid_audit_crc_err(blk);
        return FLATFS_ERR_CRC;
    }

    if (!flatfs_tykid_mac_ok(p, FLATFS_BLOCK_SIZE)) {
        flatfs_tykid_audit_crc_err(blk);
        return FLATFS_ERR_CRC;
    }
    return FLATFS_OK;
}

flatfs_err_t flatfs_integrity_check_inode(uint64_t ino)
{
    flatfs_inode_t in;
    flatfs_err_t e = flatfs_inode_read(ino, &in);
    if (e) return e;
    if (in.inode_magic != FLATFS_INODE_MAGIC) return FLATFS_ERR_CORRUPT;

    uint32_t saved = in.crc32;
    in.crc32 = 0;
    uint32_t got = flatfs_crc32(&in, sizeof(in));
    if (got != saved) return FLATFS_ERR_CRC;

    return FLATFS_OK;
}

flatfs_err_t flatfs_integrity_check(flatfs_integrity_result_t *out)
{
    FMEMSET(out, 0, sizeof(*out));

    flatfs_super_t *sb = g_fs.super;
    uint64_t total = sb->total_blocks;

    for (uint64_t ino = 1; ino < sb->total_inodes; ino++) {
        flatfs_inode_t *in = inode_ptr(ino);
        if (in->inode_magic == 0) continue;
        out->blocks_checked++;
        uint32_t sv = in->crc32;
        in->crc32 = 0;
        uint32_t got = flatfs_crc32(in, sizeof(*in));
        in->crc32 = sv;
        if (got != sv || in->inode_magic != FLATFS_INODE_MAGIC) {
            out->inode_errors++;
            out->crc_errors++;
        }
    }

    for (uint64_t blk = sb->data_start; blk < total; blk++) {
        out->blocks_checked++;
        if (flatfs_integrity_check_block(blk) != FLATFS_OK)
            out->crc_errors++;
    }

    out->passed = (out->crc_errors == 0 && out->inode_errors == 0
                   && out->btree_errors == 0 && out->journal_errors == 0);
    return FLATFS_OK;
}
