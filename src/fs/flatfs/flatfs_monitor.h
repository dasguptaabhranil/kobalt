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

#ifndef FLATFS_MONITOR_H
#define FLATFS_MONITOR_H

#include "flatfs.h"

typedef struct flatfs_mon_stats {
    uint64_t  read_ops;
    uint64_t  write_ops;
    uint64_t  alloc_ops;
    uint64_t  journal_commits;
    uint64_t  htree_hits;
    uint64_t  htree_misses;
    uint64_t  btree_hits;
    uint64_t  btree_misses;
    uint64_t  crc_errors;
    uint64_t  inode_cache_hits;
    uint64_t  inode_cache_misses;
    uint64_t  inline_reads;
    uint64_t  inline_writes;

    uint64_t  read_lat[4];
    uint64_t  write_lat[4];
} flatfs_mon_stats_t;

void flatfs_mon_init(void);
void flatfs_mon_snapshot_to_super(void);
void flatfs_mon_read(void);
void flatfs_mon_write(void);
void flatfs_mon_alloc(void);
void flatfs_mon_journal(void);
void flatfs_mon_htree_lookup(int hit);
void flatfs_mon_btree_lookup(int hit);
void flatfs_mon_crc_error(void);
void flatfs_mon_icache(int hit);
void flatfs_mon_inline(int write);

void flatfs_mon_get(flatfs_mon_stats_t *out);

int  flatfs_mon_health_score(void);

#endif
