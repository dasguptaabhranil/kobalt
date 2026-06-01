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
#include "flatfs_journal_tx.h"
#include "flatfs_tykid.h"
#include "flatfs_crc.h"

flatfs_err_t flatfs_journal_begin(uint64_t *tx_id)
{
    if (g_j.tx_active) return FLATFS_ERR_BADSTATE;
    g_j.tx_active = 1;
    g_j.tx_count  = 0;
    g_j.tx_id     = g_j.tx_seq++;
    *tx_id = g_j.tx_id;
    return FLATFS_OK;
}

flatfs_err_t flatfs_journal_log(uint64_t tx_id, uint64_t blk,
                                 const void *data)
{
    if (!g_j.tx_active || tx_id != g_j.tx_id) return FLATFS_ERR_BADSTATE;
    if (g_j.tx_count >= FLATFS_JOURNAL_MAXTX) return FLATFS_ERR_OVERFLOW;

    uint32_t slot = g_j.tx_count++;
    g_j.tx_targets[slot] = blk;
    FMEMCPY(g_j.tx_buf[slot], data, FLATFS_BLOCK_SIZE);
    return FLATFS_OK;
}

flatfs_err_t flatfs_journal_commit(uint64_t tx_id)
{
    if (!g_j.tx_active || tx_id != g_j.tx_id) return FLATFS_ERR_BADSTATE;

    flatfs_err_t seal_err = flatfs_tykid_seal_check();
    if (seal_err != FLATFS_OK) {
        flatfs_journal_abort(tx_id);
        flatfs_tykid_audit_journal(0);
        return seal_err;
    }

    flatfs_jsuper_t *js = (flatfs_jsuper_t *)blk_ptr(g_fs.super->journal_start);
    uint64_t log_base   = js->log_start;
    uint64_t log_cnt    = js->log_count;
    uint64_t pos        = js->tail;

    uint64_t desc_abs = log_base + (pos % log_cnt);
    uint8_t *desc_blk = (uint8_t *)blk_ptr(desc_abs);
    FMEMSET(desc_blk, 0, FLATFS_BLOCK_SIZE);

    flatfs_jrec_t *rec = (flatfs_jrec_t *)desc_blk;
    rec->magic    = FLATFS_JOURNAL_MAGIC;
    rec->type     = FLATFS_JREC_DESCRIPTOR;
    rec->sequence = g_j.tx_id;
    rec->count    = g_j.tx_count;

    flatfs_jtag_t *tags = (flatfs_jtag_t *)(rec + 1);
    for (uint32_t i = 0; i < g_j.tx_count; i++) {
        tags[i].target_blk = g_j.tx_targets[i];
        tags[i].flags      = (i == g_j.tx_count - 1) ? FLATFS_JTAG_LAST : 0;
        tags[i].crc32      = flatfs_crc32(g_j.tx_buf[i], FLATFS_BLOCK_SIZE);
    }
    rec->crc32 = 0;
    rec->crc32 = flatfs_crc32(desc_blk, FLATFS_BLOCK_SIZE);
    pos++;

    for (uint32_t i = 0; i < g_j.tx_count; i++) {
        uint64_t dabs = log_base + (pos % log_cnt);
        FMEMCPY(blk_ptr(dabs), g_j.tx_buf[i], FLATFS_BLOCK_SIZE);
        pos++;
    }

    uint64_t commit_abs = log_base + (pos % log_cnt);
    uint8_t *cb = (uint8_t *)blk_ptr(commit_abs);
    FMEMSET(cb, 0, FLATFS_BLOCK_SIZE);
    flatfs_jrec_t *cr = (flatfs_jrec_t *)cb;
    cr->magic    = FLATFS_JOURNAL_MAGIC;
    cr->type     = FLATFS_JREC_COMMIT;
    cr->sequence = g_j.tx_id;
    cr->crc32    = 0;
    cr->crc32    = flatfs_crc32(cb, FLATFS_BLOCK_SIZE);
    pos++;

    for (uint32_t i = 0; i < g_j.tx_count; i++)
        FMEMCPY(blk_ptr(g_j.tx_targets[i]), g_j.tx_buf[i], FLATFS_BLOCK_SIZE);

    js->tail              = pos;
    js->last_commit_seq   = g_j.tx_id;
    js->sequence          = g_j.tx_seq;
    js->crc32 = 0;
    js->crc32 = flatfs_crc32(js, sizeof(*js));
    FMEMCPY(blk_ptr(g_fs.super->journal_start + 1), js, sizeof(*js));

    g_j.tx_active = 0;
    g_j.tx_count  = 0;
    g_j.stat_commits++;
    flatfs_tykid_audit_journal(1);
    flatfs_mon_journal();
    return FLATFS_OK;
}

flatfs_err_t flatfs_journal_abort(uint64_t tx_id)
{
    if (!g_j.tx_active || tx_id != g_j.tx_id) return FLATFS_ERR_BADSTATE;
    g_j.tx_active = 0;
    g_j.tx_count  = 0;
    g_j.stat_aborts++;
    flatfs_tykid_audit_journal(0);
    return FLATFS_OK;
}
