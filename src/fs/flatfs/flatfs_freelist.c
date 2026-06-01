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
#include "flatfs_freelist.h"
#include "flatfs_tykid.h"
#include "flatfs_crc.h"
#include "flatfs_inode.h"

flatfs_err_t flatfs_freelist_load(uint32_t gi, flatfs_flgroup_t *out)
{
    uint64_t blk = g_fs.super->freelist_start + gi;
    if (!blk_valid(blk)) return FLATFS_ERR_BOUNDS;

    FMEMCPY(out, blk_ptr(blk), sizeof(*out));
    if (out->magic != FLATFS_FREELIST_MAGIC) return FLATFS_ERR_CORRUPT;

    uint32_t sv = out->crc32;
    out->crc32 = 0;
    uint32_t got = flatfs_crc32(out, sizeof(*out));
    out->crc32 = sv;
    return (got == sv) ? FLATFS_OK : FLATFS_ERR_CRC;
}

flatfs_err_t flatfs_freelist_flush(uint32_t gi, const flatfs_flgroup_t *in)
{
    uint64_t blk = g_fs.super->freelist_start + gi;
    if (!blk_valid(blk)) return FLATFS_ERR_BOUNDS;

    flatfs_flgroup_t tmp;
    FMEMCPY(&tmp, in, sizeof(tmp));
    tmp.crc32 = 0;
    tmp.crc32 = flatfs_crc32(&tmp, sizeof(tmp));

    flatfs_tykid_mac_set(&tmp, sizeof(tmp));
    tmp.crc32 = 0;
    tmp.crc32 = flatfs_crc32(&tmp, sizeof(tmp));
    FMEMCPY(blk_ptr(blk), &tmp, sizeof(tmp));
    return FLATFS_OK;
}

flatfs_err_t flatfs_freelist_rebuild(void)
{
    flatfs_super_t *sb = g_fs.super;
    uint64_t ngroups = sb->freelist_blocks;

    for (uint64_t gi = 0; gi < ngroups; gi++) {
        flatfs_flgroup_t *g =
            (flatfs_flgroup_t *)blk_ptr(sb->freelist_start + gi);
        FMEMSET(g->bitmap, 0xFF, sizeof(g->bitmap));
        g->free_count = 0;
    }
    sb->free_blocks = 0;

    for (uint64_t ino = 1; ino < sb->total_inodes; ino++) {
        flatfs_inode_t in;
        if (flatfs_inode_read(ino, &in) != FLATFS_OK) continue;

        if (in.flags & FLATFS_FL_INLINE) continue;

        for (int i = 0; i < 12; i++) {
            uint64_t blk = in.data.blk_ptrs[i];
            if (!blk) continue;
            if (blk < sb->data_start || blk >= sb->total_blocks) continue;

            uint64_t lo   = blk - sb->data_start;
            uint32_t gi   = (uint32_t)(lo / FLATFS_GROUP_BLOCKS);
            uint32_t bit  = (uint32_t)(lo % FLATFS_GROUP_BLOCKS);
            flatfs_flgroup_t *g =
                (flatfs_flgroup_t *)blk_ptr(sb->freelist_start + gi);
            if (g->bitmap[bit >> 3] & (1u << (bit & 7))) {
                g->bitmap[bit >> 3] &= ~(uint8_t)(1u << (bit & 7));
                g->free_count++;
                sb->free_blocks++;
            }
        }
    }

    for (uint64_t gi = 0; gi < ngroups; gi++) {
        flatfs_flgroup_t *g =
            (flatfs_flgroup_t *)blk_ptr(sb->freelist_start + gi);
        g->crc32 = 0;
        g->crc32 = flatfs_crc32(g, sizeof(*g));
    }
    return FLATFS_OK;
}
