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

#ifndef FLATFS_JOURNAL_TX_H
#define FLATFS_JOURNAL_TX_H

#include "flatfs_journal.h"

typedef struct {
    uint64_t  tx_seq;
    uint64_t  tx_id;
    int       tx_active;
    uint8_t   tx_buf[FLATFS_JOURNAL_MAXTX][FLATFS_BLOCK_SIZE];
    uint64_t  tx_targets[FLATFS_JOURNAL_MAXTX];
    uint32_t  tx_count;
    uint64_t  revoke[256];
    uint32_t  nrevoke;
    uint64_t  stat_commits;
    uint64_t  stat_replays;
    uint64_t  stat_aborts;
} jstate_t;

extern jstate_t g_j;

#endif
