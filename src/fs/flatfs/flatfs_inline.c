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
#include "flatfs_inline.h"
#include "flatfs_alloc.h"
#include "flatfs_inode.h"
#include "flatfs_crc.h"

int flatfs_inline_fits(size_t sz)
{
    return sz <= FLATFS_INLINE_MAX;
}

flatfs_err_t flatfs_inline_read(const flatfs_inode_t *in, uint64_t off,
                                 void *buf, size_t len, size_t *nr)
{
    if (!(in->flags & FLATFS_FL_INLINE)) return FLATFS_ERR_INVAL;
    if (off >= in->size) { *nr = 0; return FLATFS_OK; }

    size_t avail = (size_t)(in->size - off);
    size_t n = (len < avail) ? len : avail;
    FMEMCPY(buf, in->data.inline_data + off, n);
    *nr = n;
    return FLATFS_OK;
}

flatfs_err_t flatfs_inline_write(flatfs_inode_t *in, uint64_t off,
                                  const void *buf, size_t len, size_t *nw)
{
    if (off + len > FLATFS_INLINE_MAX) return FLATFS_ERR_TOOBIG;

    FMEMCPY(in->data.inline_data + off, buf, len);
    if (off + len > in->size) in->size = off + len;
    in->flags |= FLATFS_FL_INLINE;
    *nw = len;
    return FLATFS_OK;
}

flatfs_err_t flatfs_inline_to_block(flatfs_inode_t *in)
{
    if (!(in->flags & FLATFS_FL_INLINE)) return FLATFS_OK;

    uint64_t blk;
    flatfs_err_t e = flatfs_alloc_block(&blk);
    if (e) return e;

    uint8_t *dst = (uint8_t *)blk_ptr(blk);
    FMEMSET(dst, 0, FLATFS_BLOCK_SIZE);
    FMEMCPY(dst, in->data.inline_data, (size_t)in->size);

    flatfs_block_crc_set(dst, FLATFS_BLOCK_SIZE);

    FMEMSET(&in->data, 0, sizeof(in->data));
    in->data.blk_ptrs[0] = blk;
    in->blocks  = FLATFS_BLOCK_SIZE / 512;
    in->flags  &= ~FLATFS_FL_INLINE;

    return flatfs_inode_write(in);
}
