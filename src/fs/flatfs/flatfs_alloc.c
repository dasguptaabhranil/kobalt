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
#include "flatfs_alloc.h"
#include "flatfs_crc.h"

static struct {
    uint64_t  ngroups;
    uint64_t  alloc_ops;
    uint64_t  free_ops;
    uint32_t  group_hint;
} g_alloc;

static flatfs_flgroup_t *group_ptr(uint32_t gi)
{
    return (flatfs_flgroup_t *)blk_ptr(g_fs.super->freelist_start + gi);
}

static void group_flush(uint32_t gi)
{
    flatfs_flgroup_t *g = group_ptr(gi);
    g->crc32 = 0;
    g->crc32 = flatfs_crc32(g, sizeof(*g));
}

static int bitmap_test(const uint8_t *bm, uint32_t bit)
{
    return (bm[bit >> 3] >> (bit & 7)) & 1;
}

static void bitmap_set(uint8_t *bm, uint32_t bit)
{
    bm[bit >> 3] |= (uint8_t)(1u << (bit & 7));
}

static void bitmap_clr(uint8_t *bm, uint32_t bit)
{
    bm[bit >> 3] &= (uint8_t)~(1u << (bit & 7));
}

flatfs_err_t flatfs_alloc_init(void)
{
    flatfs_super_t *sb = g_fs.super;
    g_alloc.ngroups    = sb->freelist_blocks;
    g_alloc.group_hint = 0;

    for (uint32_t gi = 0; gi < g_alloc.ngroups; gi++) {
        flatfs_flgroup_t *g = group_ptr(gi);
        if (g->magic != FLATFS_FREELIST_MAGIC) {
            FMEMSET(g, 0, sizeof(*g));
            g->magic     = FLATFS_FREELIST_MAGIC;
            g->group_idx = gi;
            g->base_blk  = sb->data_start + (uint64_t)gi * FLATFS_GROUP_BLOCKS;
            g->free_count= FLATFS_GROUP_BLOCKS;

            FMEMSET(g->bitmap, 0, sizeof(g->bitmap));
            group_flush(gi);
        }
    }
    return FLATFS_OK;
}

static int group_find_free(flatfs_flgroup_t *g)
{
    for (uint32_t i = 0; i < FLATFS_GROUP_BLOCKS; i++)
        if (!bitmap_test(g->bitmap, i)) return (int)i;
    return -1;
}

flatfs_err_t flatfs_alloc_block(uint64_t *out)
{
    flatfs_super_t *sb = g_fs.super;
    if (!sb->free_blocks) return FLATFS_ERR_NOSPACE;

    for (uint32_t gi = 0; gi < g_alloc.ngroups; gi++) {
        uint32_t idx = (g_alloc.group_hint + gi) % (uint32_t)g_alloc.ngroups;
        flatfs_flgroup_t *g = group_ptr(idx);
        if (!g->free_count) continue;

        int local = group_find_free(g);
        if (local < 0) continue;

        bitmap_set(g->bitmap, (uint32_t)local);
        g->free_count--;
        sb->free_blocks--;
        group_flush(idx);
        g_alloc.group_hint = idx;
        g_alloc.alloc_ops++;
        *out = g->base_blk + (uint32_t)local;
        FMEMSET(blk_ptr(*out), 0, FLATFS_BLOCK_SIZE);
        flatfs_mon_alloc();
        return FLATFS_OK;
    }
    return FLATFS_ERR_NOSPACE;
}

flatfs_err_t flatfs_alloc_blocks(uint32_t count, uint64_t *out)
{

    for (uint32_t i = 0; i < count; i++) {
        flatfs_err_t e = flatfs_alloc_block(&out[i]);
        if (e) {

            for (uint32_t j = 0; j < i; j++) flatfs_free_block(out[j]);
            return e;
        }
    }
    return FLATFS_OK;
}

flatfs_err_t flatfs_free_block(uint64_t blk)
{
    flatfs_super_t *sb = g_fs.super;
    if (blk < sb->data_start || blk >= sb->total_blocks)
        return FLATFS_ERR_BOUNDS;

    uint64_t local_off = blk - sb->data_start;
    uint32_t gi  = (uint32_t)(local_off / FLATFS_GROUP_BLOCKS);
    uint32_t bit = (uint32_t)(local_off % FLATFS_GROUP_BLOCKS);

    if (gi >= g_alloc.ngroups) return FLATFS_ERR_BOUNDS;

    flatfs_flgroup_t *g = group_ptr(gi);
    if (!bitmap_test(g->bitmap, bit)) return FLATFS_ERR_INVAL;

    bitmap_clr(g->bitmap, bit);
    g->free_count++;
    sb->free_blocks++;
    group_flush(gi);
    g_alloc.free_ops++;

    if (gi < g_alloc.group_hint) g_alloc.group_hint = gi;
    return FLATFS_OK;
}

flatfs_err_t flatfs_free_blocks(uint64_t blk, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        flatfs_err_t e = flatfs_free_block(blk + i);
        if (e) return e;
    }
    return FLATFS_OK;
}

void flatfs_alloc_stats(uint64_t *total, uint64_t *free_blks,
                         uint64_t *alloc_ops, uint64_t *free_ops)
{
    *total     = g_fs.super->total_blocks;
    *free_blks = g_fs.super->free_blocks;
    *alloc_ops = g_alloc.alloc_ops;
    *free_ops  = g_alloc.free_ops;
}
