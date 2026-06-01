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
#include "flatfs_file.h"
#include "flatfs_tykid.h"
#include "flatfs_alloc.h"
#include "flatfs_inode.h"
#include "flatfs_inline.h"
#include "flatfs_crc.h"

#define PTRS_PER_BLK  (FLATFS_BLOCK_SIZE / sizeof(uint64_t))
#define DIRECT_MAX    10u
#define INDIRECT_IDX  10u
#define DBLIND_IDX    11u

static flatfs_err_t map_block(flatfs_inode_t *in, uint64_t lblk,
                               int alloc, uint64_t *out_pblk)
{
    if (lblk < DIRECT_MAX) {
        if (!in->data.blk_ptrs[lblk]) {
            if (!alloc) { *out_pblk = 0; return FLATFS_OK; }
            flatfs_err_t e = flatfs_alloc_block(&in->data.blk_ptrs[lblk]);
            if (e) return e;
            in->blocks += FLATFS_BLOCK_SIZE / 512;
        }
        *out_pblk = in->data.blk_ptrs[lblk];
        return FLATFS_OK;
    }

    lblk -= DIRECT_MAX;

    if (lblk < PTRS_PER_BLK) {

        if (!in->data.blk_ptrs[INDIRECT_IDX]) {
            if (!alloc) { *out_pblk = 0; return FLATFS_OK; }
            flatfs_err_t e = flatfs_alloc_block(&in->data.blk_ptrs[INDIRECT_IDX]);
            if (e) return e;
            in->blocks += FLATFS_BLOCK_SIZE / 512;
        }
        uint64_t *ind = (uint64_t *)blk_ptr(in->data.blk_ptrs[INDIRECT_IDX]);
        if (!ind[lblk]) {
            if (!alloc) { *out_pblk = 0; return FLATFS_OK; }
            flatfs_err_t e = flatfs_alloc_block(&ind[lblk]);
            if (e) return e;
            in->blocks += FLATFS_BLOCK_SIZE / 512;
        }
        *out_pblk = ind[lblk];
        return FLATFS_OK;
    }

    lblk -= PTRS_PER_BLK;

    if (lblk < PTRS_PER_BLK * PTRS_PER_BLK) {

        if (!in->data.blk_ptrs[DBLIND_IDX]) {
            if (!alloc) { *out_pblk = 0; return FLATFS_OK; }
            flatfs_err_t e = flatfs_alloc_block(&in->data.blk_ptrs[DBLIND_IDX]);
            if (e) return e;
            in->blocks += FLATFS_BLOCK_SIZE / 512;
        }
        uint64_t *dbl  = (uint64_t *)blk_ptr(in->data.blk_ptrs[DBLIND_IDX]);
        uint64_t  idx1 = lblk / PTRS_PER_BLK;
        uint64_t  idx2 = lblk % PTRS_PER_BLK;
        if (!dbl[idx1]) {
            if (!alloc) { *out_pblk = 0; return FLATFS_OK; }
            flatfs_err_t e = flatfs_alloc_block(&dbl[idx1]);
            if (e) return e;
            in->blocks += FLATFS_BLOCK_SIZE / 512;
        }
        uint64_t *ind = (uint64_t *)blk_ptr(dbl[idx1]);
        if (!ind[idx2]) {
            if (!alloc) { *out_pblk = 0; return FLATFS_OK; }
            flatfs_err_t e = flatfs_alloc_block(&ind[idx2]);
            if (e) return e;
            in->blocks += FLATFS_BLOCK_SIZE / 512;
        }
        *out_pblk = ind[idx2];
        return FLATFS_OK;
    }

    return FLATFS_ERR_TOOBIG;
}

flatfs_err_t flatfs_file_read_blk(const flatfs_inode_t *in, uint64_t lblk,
                                   void *buf)
{
    uint64_t pblk;
    flatfs_err_t e = map_block((flatfs_inode_t *)in, lblk, 0, &pblk);
    if (e) return e;
    if (!pblk) {
        FMEMSET(buf, 0, FLATFS_BLOCK_SIZE);
        return FLATFS_OK;
    }
    FMEMCPY(buf, blk_ptr(pblk), FLATFS_BLOCK_SIZE);

    if (in->flags & FLATFS_FL_HASCRC) {
        if (!flatfs_block_crc_ok(buf, FLATFS_BLOCK_SIZE)) {
            flatfs_tykid_audit_crc_err(pblk);
            return FLATFS_ERR_CRC;
        }

        if (!flatfs_tykid_mac_ok(buf, FLATFS_BLOCK_SIZE)) {
            flatfs_tykid_audit_crc_err(pblk);
            return FLATFS_ERR_CRC;
        }
    }
    return FLATFS_OK;
}

flatfs_err_t flatfs_file_write_blk(flatfs_inode_t *in, uint64_t lblk,
                                    const void *data)
{
    uint64_t pblk;
    flatfs_err_t e = map_block(in, lblk, 1, &pblk);
    if (e) return e;

    uint8_t tmp[FLATFS_BLOCK_SIZE];
    FMEMCPY(tmp, data, FLATFS_BLOCK_SIZE);

    if (in->flags & FLATFS_FL_HASCRC) {

        flatfs_tykid_mac_set(tmp, FLATFS_BLOCK_SIZE);
        flatfs_block_crc_set(tmp, FLATFS_BLOCK_SIZE);
    }

    FMEMCPY(blk_ptr(pblk), tmp, FLATFS_BLOCK_SIZE);
    return FLATFS_OK;
}

flatfs_err_t flatfs_file_pread(const flatfs_inode_t *in, uint64_t off,
                                void *buf, size_t len, size_t *nr)
{
    if (in->flags & FLATFS_FL_INLINE)
        return flatfs_inline_read(in, off, buf, len, nr);

    if (off >= in->size) { *nr = 0; return FLATFS_OK; }
    size_t avail = (size_t)(in->size - off);
    if (len > avail) len = avail;

    size_t done = 0;
    while (done < len) {
        uint64_t lblk = (off + done) >> FLATFS_BLOCK_SHIFT;
        uint32_t boff = (uint32_t)((off + done) & (FLATFS_BLOCK_SIZE - 1));
        size_t   chunk = FLATFS_BLOCK_SIZE - boff;
        if (chunk > len - done) chunk = len - done;

        uint8_t blkbuf[FLATFS_BLOCK_SIZE];
        flatfs_err_t e = flatfs_file_read_blk(in, lblk, blkbuf);
        if (e) return e;

        FMEMCPY((uint8_t *)buf + done, blkbuf + boff, chunk);
        done += chunk;
        flatfs_mon_read();
    }
    *nr = done;
    return FLATFS_OK;
}

flatfs_err_t flatfs_file_pwrite(flatfs_inode_t *in, uint64_t off,
                                 const void *buf, size_t len, size_t *nw)
{
    if (in->flags & FLATFS_FL_INLINE) {
        if (!flatfs_inline_fits((size_t)(off + len))) {
            flatfs_err_t e = flatfs_inline_to_block(in);
            if (e) return e;
        } else {
            return flatfs_inline_write(in, off, buf, len, nw);
        }
    }

    size_t done = 0;
    while (done < len) {
        uint64_t lblk = (off + done) >> FLATFS_BLOCK_SHIFT;
        uint32_t boff = (uint32_t)((off + done) & (FLATFS_BLOCK_SIZE - 1));
        size_t   chunk = FLATFS_BLOCK_SIZE - boff;
        if (chunk > len - done) chunk = len - done;

        uint8_t blkbuf[FLATFS_BLOCK_SIZE];

        if (boff || chunk < FLATFS_BLOCK_SIZE) {
            flatfs_err_t e = flatfs_file_read_blk(in, lblk, blkbuf);
            if (e) return e;
        } else {
            FMEMSET(blkbuf, 0, FLATFS_BLOCK_SIZE);
        }

        FMEMCPY(blkbuf + boff, (const uint8_t *)buf + done, chunk);
        flatfs_err_t e = flatfs_file_write_blk(in, lblk, blkbuf);
        if (e) return e;

        done += chunk;
        flatfs_mon_write();
    }

    if (off + done > in->size)
        in->size = off + done;

    *nw = done;
    return flatfs_inode_write(in);
}

flatfs_err_t flatfs_file_truncate(flatfs_inode_t *in, uint64_t new_sz)
{
    if (new_sz > in->size) {

        in->size = new_sz;
        return flatfs_inode_write(in);
    }

    if (new_sz == in->size) return FLATFS_OK;

    uint64_t old_blks = (in->size + FLATFS_BLOCK_SIZE - 1) >> FLATFS_BLOCK_SHIFT;
    uint64_t new_blks = (new_sz  + FLATFS_BLOCK_SIZE - 1) >> FLATFS_BLOCK_SHIFT;

    for (uint64_t lb = new_blks; lb < old_blks; lb++) {
        uint64_t pb;
        if (map_block(in, lb, 0, &pb) == FLATFS_OK && pb)
            flatfs_free_block(pb);
    }

    in->size = new_sz;
    if (!new_sz && (in->flags & FLATFS_FL_INLINE)) {
        FMEMSET(in->data.inline_data, 0, FLATFS_INLINE_MAX);
    }
    return flatfs_inode_write(in);
}
