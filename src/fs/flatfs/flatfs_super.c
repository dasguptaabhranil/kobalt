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
#include "flatfs_tykid.h"
#include "flatfs_crc.h"

flatfs_fs_t g_fs;

flatfs_err_t flatfs_super_validate(const flatfs_super_t *sb)
{
    if (sb->magic != FLATFS_MAGIC)         return FLATFS_ERR_MAGIC;
    if (sb->version != FLATFS_VERSION)     return FLATFS_ERR_CORRUPT;
    if (sb->block_size != FLATFS_BLOCK_SIZE) return FLATFS_ERR_CORRUPT;
    if (sb->inode_size != FLATFS_INODE_SIZE) return FLATFS_ERR_CORRUPT;
    if (sb->total_blocks > FLATFS_MAX_BLOCKS) return FLATFS_ERR_BOUNDS;

    uint32_t saved = sb->crc32;
    ((flatfs_super_t *)sb)->crc32 = 0;
    uint32_t got = flatfs_crc32(sb, sizeof(*sb));
    ((flatfs_super_t *)sb)->crc32 = saved;

    return (got == saved) ? FLATFS_OK : FLATFS_ERR_CRC;
}

flatfs_err_t flatfs_super_read(void *buf, uint64_t cap, flatfs_super_t **out)
{
    if (!buf || cap < FLATFS_BLOCK_SIZE * 4) return FLATFS_ERR_INVAL;

    flatfs_super_t *sb = (flatfs_super_t *)buf;
    flatfs_err_t e = flatfs_super_validate(sb);
    if (e != FLATFS_OK) {
        flatfs_tykid_audit_super_corrupt();

        sb = (flatfs_super_t *)((uint8_t *)buf + FLATFS_BLOCK_SIZE);
        e = flatfs_super_validate(sb);
        if (e != FLATFS_OK) return e;

        FMEMCPY(buf, sb, sizeof(*sb));
        ((flatfs_super_t *)buf)->crc32 = 0;
        flatfs_super_update_crc((flatfs_super_t *)buf);
    }

    if (!flatfs_tykid_mac_ok(sb, sizeof(*sb))) {
        flatfs_tykid_audit_crc_err(0);
        return FLATFS_ERR_CRC;
    }

    *out = (flatfs_super_t *)buf;
    return FLATFS_OK;
}

void flatfs_super_update_crc(flatfs_super_t *sb)
{
    sb->crc32 = 0;
    sb->crc32 = flatfs_crc32(sb, sizeof(*sb));
}

flatfs_err_t flatfs_super_write(flatfs_super_t *sb)
{
    flatfs_super_update_crc(sb);

    flatfs_tykid_mac_set(sb, sizeof(*sb));

    flatfs_super_update_crc(sb);

    FMEMCPY((uint8_t *)g_fs.buf + FLATFS_BLOCK_SIZE, sb, sizeof(*sb));
    return FLATFS_OK;
}

void flatfs_super_mark_dirty(flatfs_super_t *sb)
{
    sb->state = (sb->state & ~FLATFS_STATE_CLEAN) | FLATFS_STATE_DIRTY;
    sb->mount_count++;
    flatfs_super_write(sb);
}

void flatfs_super_mark_clean(flatfs_super_t *sb)
{
    sb->state = (sb->state & ~FLATFS_STATE_DIRTY) | FLATFS_STATE_CLEAN;
    flatfs_super_write(sb);
}

flatfs_err_t flatfs_super_init(void *buf, uint64_t cap, const char *label,
                                uint64_t ninodes)
{
    if (!buf || cap < FLATFS_BLOCK_SIZE * 16) return FLATFS_ERR_INVAL;

    FMEMSET(buf, 0, FLATFS_BLOCK_SIZE * 2);
    flatfs_super_t *sb = (flatfs_super_t *)buf;

    sb->magic       = FLATFS_MAGIC;
    sb->version     = FLATFS_VERSION;
    sb->block_size  = FLATFS_BLOCK_SIZE;
    sb->inode_size  = FLATFS_INODE_SIZE;
    sb->capacity_bytes = cap;

    uint64_t total_blks = cap >> FLATFS_BLOCK_SHIFT;
    sb->total_blocks = total_blks;

    uint64_t off = 2;
    sb->journal_start  = off + 2;
    sb->journal_blocks = FLATFS_JOURNAL_BLOCKS;
    off = sb->journal_start + sb->journal_blocks;

    uint64_t inode_blks = (ninodes + FLATFS_INODES_PER_BLK - 1)
                          / FLATFS_INODES_PER_BLK;
    sb->inode_table_start  = off;
    sb->inode_table_blocks = inode_blks;
    sb->total_inodes       = ninodes;
    sb->free_inodes        = ninodes - 1;
    off += inode_blks;

    sb->btree_root_blk = off++;

    uint64_t data_blks = total_blks - off - 1;
    uint64_t ngroups   = (data_blks + FLATFS_GROUP_BLOCKS - 1)
                         / FLATFS_GROUP_BLOCKS;
    sb->freelist_start  = off;
    sb->freelist_blocks = ngroups;
    off += ngroups;

    sb->data_start   = off;
    sb->free_blocks  = total_blks - off;

    sb->root_ino     = 1;
    sb->features     = FLATFS_FEAT_JOURNAL | FLATFS_FEAT_HTREE |
                       FLATFS_FEAT_BTREE   | FLATFS_FEAT_INLINE |
                       FLATFS_FEAT_CRC32   | FLATFS_FEAT_64BIT  |
                       FLATFS_FEAT_MONITOR | FLATFS_FEAT_ARBITER;
    sb->state        = FLATFS_STATE_CLEAN;
    sb->max_mounts   = 30;

    if (label) {
        size_t n = fstrlen(label, 63);
        FMEMCPY(sb->label, label, n);
        sb->label[n] = 0;
    }

    flatfs_super_update_crc(sb);
    FMEMCPY((uint8_t *)buf + FLATFS_BLOCK_SIZE, sb, sizeof(*sb));
    return FLATFS_OK;
}
