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

#ifndef FLATFS_JOURNAL_H
#define FLATFS_JOURNAL_H

#include "flatfs.h"

typedef struct __attribute__((packed)) flatfs_jsuper {
    uint32_t  magic;
    uint32_t  version;
    uint64_t  sequence;
    uint64_t  head;
    uint64_t  tail;
    uint64_t  log_start;
    uint64_t  log_count;
    uint64_t  last_commit_seq;
    uint32_t  flags;
    uint32_t  block_size;
    uint8_t   _pad[4028];
    uint32_t  crc32;
} flatfs_jsuper_t;

typedef char _jsuper_chk[(sizeof(flatfs_jsuper_t) == FLATFS_BLOCK_SIZE) ? 1 : -1];

typedef struct __attribute__((packed)) flatfs_jrec {
    uint32_t  magic;
    uint32_t  type;
    uint64_t  sequence;
    uint32_t  count;
    uint32_t  crc32;
} flatfs_jrec_t;

#define FLATFS_JREC_DESCRIPTOR  1u
#define FLATFS_JREC_COMMIT      2u
#define FLATFS_JREC_REVOKE      3u

typedef struct __attribute__((packed)) flatfs_jtag {
    uint64_t  target_blk;
    uint32_t  flags;
    uint32_t  crc32;
} flatfs_jtag_t;

#define FLATFS_JTAG_LAST   (1u << 0)
#define FLATFS_JTAG_ESCAPE (1u << 1)

flatfs_err_t flatfs_journal_init(void);
flatfs_err_t flatfs_journal_replay(void);
flatfs_err_t flatfs_journal_begin(uint64_t *tx_id);
flatfs_err_t flatfs_journal_log(uint64_t tx_id, uint64_t blk,
                                 const void *data);
flatfs_err_t flatfs_journal_commit(uint64_t tx_id);
flatfs_err_t flatfs_journal_abort(uint64_t tx_id);
flatfs_err_t flatfs_journal_revoke(uint64_t blk);
flatfs_err_t flatfs_journal_checkpoint(void);
void         flatfs_journal_stats(uint64_t *commits, uint64_t *replays,
                                   uint64_t *aborts);

#endif
