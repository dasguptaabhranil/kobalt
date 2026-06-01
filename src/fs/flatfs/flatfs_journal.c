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
#include "flatfs_journal.h"
#include "flatfs_crc.h"

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

static jstate_t g_j;

static flatfs_jsuper_t *jsuper(void)
{
    return (flatfs_jsuper_t *)blk_ptr(g_fs.super->journal_start);
}

flatfs_err_t flatfs_journal_init(void)
{
    FMEMSET(&g_j, 0, sizeof(g_j));
    flatfs_jsuper_t *js = jsuper();

    if (js->magic != FLATFS_JOURNAL_MAGIC) {

        FMEMSET(js, 0, sizeof(*js));
        js->magic      = FLATFS_JOURNAL_MAGIC;
        js->version    = 1;
        js->sequence   = 1;
        js->log_start  = g_fs.super->journal_start + 2;
        js->log_count  = g_fs.super->journal_blocks - 2;
        js->head       = 0;
        js->tail       = 0;
        js->block_size = FLATFS_BLOCK_SIZE;
        js->crc32 = 0;
        js->crc32 = flatfs_crc32(js, sizeof(*js));

        FMEMCPY(blk_ptr(g_fs.super->journal_start + 1), js, sizeof(*js));
    }
    g_j.tx_seq = js->sequence;
    return FLATFS_OK;
}

static int revoked(uint64_t blk)
{
    for (uint32_t i = 0; i < g_j.nrevoke; i++)
        if (g_j.revoke[i] == blk) return 1;
    return 0;
}

flatfs_err_t flatfs_journal_replay(void)
{
    flatfs_jsuper_t *js = jsuper();
    if (js->magic != FLATFS_JOURNAL_MAGIC) return FLATFS_ERR_JOURNAL;

    uint64_t log_base = js->log_start;
    uint64_t log_cnt  = js->log_count;
    uint64_t pos      = js->head;
    uint64_t limit    = js->tail;
    uint64_t replayed = 0;

    while (pos != limit) {
        uint64_t abs_blk = log_base + (pos % log_cnt);
        flatfs_jrec_t *rec = (flatfs_jrec_t *)blk_ptr(abs_blk);

        if (rec->magic != FLATFS_JOURNAL_MAGIC) break;

        if (rec->type == FLATFS_JREC_DESCRIPTOR) {
            uint32_t cnt = rec->count;
            uint64_t dpos = pos + 1;

            flatfs_jtag_t *tags = (flatfs_jtag_t *)(rec + 1);

            for (uint32_t i = 0; i < cnt; i++) {
                uint64_t data_blk = log_base + (dpos % log_cnt);
                if (!revoked(tags[i].target_blk)) {
                    void *src = blk_ptr(data_blk);

                    uint32_t dc = flatfs_crc32(src, FLATFS_BLOCK_SIZE);
                    if (dc == tags[i].crc32)
                        FMEMCPY(blk_ptr(tags[i].target_blk), src,
                                FLATFS_BLOCK_SIZE);
                }
                dpos++;
            }
            pos = dpos;
        } else if (rec->type == FLATFS_JREC_COMMIT) {
            replayed++;
            pos++;
        } else if (rec->type == FLATFS_JREC_REVOKE) {
            uint32_t cnt = rec->count;
            uint64_t *blks = (uint64_t *)(rec + 1);
            for (uint32_t i = 0; i < cnt && g_j.nrevoke < 256; i++)
                g_j.revoke[g_j.nrevoke++] = blks[i];
            pos++;
        } else {
            break;
        }
    }
    g_j.stat_replays += replayed;
    js->head = pos;
    js->crc32 = 0;
    js->crc32 = flatfs_crc32(js, sizeof(*js));
    return FLATFS_OK;
}

flatfs_err_t flatfs_journal_revoke(uint64_t blk)
{
    if (g_j.nrevoke >= 256) return FLATFS_ERR_OVERFLOW;
    g_j.revoke[g_j.nrevoke++] = blk;
    return FLATFS_OK;
}

flatfs_err_t flatfs_journal_checkpoint(void)
{
    flatfs_jsuper_t *js = jsuper();
    js->head = js->tail;
    js->crc32 = 0;
    js->crc32 = flatfs_crc32(js, sizeof(*js));
    FMEMCPY(blk_ptr(g_fs.super->journal_start + 1), js, sizeof(*js));
    return FLATFS_OK;
}

void flatfs_journal_stats(uint64_t *commits, uint64_t *replays, uint64_t *aborts)
{
    *commits = g_j.stat_commits;
    *replays = g_j.stat_replays;
    *aborts  = g_j.stat_aborts;
}
