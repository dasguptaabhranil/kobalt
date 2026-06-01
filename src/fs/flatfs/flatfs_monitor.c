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
#include "flatfs_monitor.h"

static flatfs_mon_stats_t g_mon;

void flatfs_mon_init(void)
{
    FMEMSET(&g_mon, 0, sizeof(g_mon));
}

void flatfs_mon_snapshot_to_super(void)
{
    flatfs_super_t *sb = g_fs.super;
    sb->mon_read_ops        = g_mon.read_ops;
    sb->mon_write_ops       = g_mon.write_ops;
    sb->mon_alloc_ops       = g_mon.alloc_ops;
    sb->mon_journal_commits = g_mon.journal_commits;
}

void flatfs_mon_read(void)          { g_mon.read_ops++; }
void flatfs_mon_write(void)         { g_mon.write_ops++; }
void flatfs_mon_alloc(void)         { g_mon.alloc_ops++; }
void flatfs_mon_journal(void)       { g_mon.journal_commits++; }
void flatfs_mon_crc_error(void)     { g_mon.crc_errors++; }
void flatfs_mon_inline(int write)
{
    if (write) g_mon.inline_writes++;
    else       g_mon.inline_reads++;
}

void flatfs_mon_htree_lookup(int hit)
{
    if (hit) g_mon.htree_hits++;
    else     g_mon.htree_misses++;
}

void flatfs_mon_btree_lookup(int hit)
{
    if (hit) g_mon.btree_hits++;
    else     g_mon.btree_misses++;
}

void flatfs_mon_icache(int hit)
{
    if (hit) g_mon.inode_cache_hits++;
    else     g_mon.inode_cache_misses++;
}

void flatfs_mon_get(flatfs_mon_stats_t *out)
{
    FMEMCPY(out, &g_mon, sizeof(*out));
}

int flatfs_mon_health_score(void)
{
    int score = 100;

    if (g_mon.crc_errors > 0) {
        uint64_t errs = g_mon.crc_errors;
        score -= (int)(errs > 10 ? 50 : errs * 5);
    }

    uint64_t ht_total = g_mon.htree_hits + g_mon.htree_misses;
    if (ht_total > 100) {
        int hit_pct = (int)(g_mon.htree_hits * 100 / ht_total);
        if (hit_pct < 80) score -= (80 - hit_pct) / 4;
    }

    uint64_t bt_total = g_mon.btree_hits + g_mon.btree_misses;
    if (bt_total > 100) {
        int hit_pct = (int)(g_mon.btree_hits * 100 / bt_total);
        if (hit_pct < 60) score -= (60 - hit_pct) / 3;
    }

    flatfs_super_t *sb = g_fs.super;
    if (sb->total_blocks > 0) {
        uint64_t free_pct = sb->free_blocks * 100 / sb->total_blocks;
        if (free_pct < 10)
            score -= (int)(10 - free_pct) * 2;
    }

    if (score < 0)  score = 0;
    if (score > 100) score = 100;
    return score;
}
