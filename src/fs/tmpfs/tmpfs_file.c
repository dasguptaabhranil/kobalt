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

#include "tmpfs.h"
#include <kmalloc.h>
#include <string.h>

static uint8_t *pg_get(tmpfs_inode_t *ip, uint32_t pgidx, int alloc)
{
    uint32_t l1 = pgidx / TMPFS_L2_SZ;
    uint32_t l2 = pgidx % TMPFS_L2_SZ;

    if (l1 >= TMPFS_L1_SZ) return NULL;

    if (!ip->pg_l1[l1]) {
        if (!alloc) return NULL;
        ip->pg_l1[l1] = kmalloc(TMPFS_L2_SZ * sizeof(uint8_t *));
        if (!ip->pg_l1[l1]) return NULL;
        memset(ip->pg_l1[l1], 0, TMPFS_L2_SZ * sizeof(uint8_t *));
    }

    if (!ip->pg_l1[l1][l2]) {
        if (!alloc) return NULL;
        ip->pg_l1[l1][l2] = kmalloc(TMPFS_PAGE_SIZE);
        if (!ip->pg_l1[l1][l2]) return NULL;
        memset(ip->pg_l1[l1][l2], 0, TMPFS_PAGE_SIZE);
    }

    return ip->pg_l1[l1][l2];
}

void tmpfs_file_freepages(tmpfs_inode_t *ip)
{
    for (unsigned i = 0; i < TMPFS_L1_SZ; i++) {
        if (!ip->pg_l1[i]) continue;
        for (unsigned j = 0; j < TMPFS_L2_SZ; j++) {
            if (ip->pg_l1[i][j]) {
                kfree(ip->pg_l1[i][j]);
                ip->pg_l1[i][j] = NULL;
            }
        }
        kfree(ip->pg_l1[i]);
        ip->pg_l1[i] = NULL;
    }
    ip->size = 0;
}

int tmpfs_file_read(tmpfs_inode_t *ip, void *buf, uint64_t off,
                    size_t len, size_t *got)
{
    if (ip->type != TMPFS_T_REG) return TMPFS_EINVAL;

    spin_lock(&ip->lock);

    if (off >= ip->size) {
        spin_unlock(&ip->lock);
        *got = 0;
        return TMPFS_OK;
    }
    if (off + (uint64_t)len > ip->size)
        len = (size_t)(ip->size - off);

    size_t rem = len;
    uint8_t *dst = buf;

    while (rem) {
        uint32_t pgidx = (uint32_t)(off / TMPFS_PAGE_SIZE);
        uint32_t pgoff = (uint32_t)(off % TMPFS_PAGE_SIZE);
        size_t   chunk = TMPFS_PAGE_SIZE - pgoff;
        if (chunk > rem) chunk = rem;

        uint8_t *pg = pg_get(ip, pgidx, 0);
        if (pg)
            memcpy(dst, pg + pgoff, chunk);
        else
            memset(dst, 0, chunk);

        dst += chunk;
        off += chunk;
        rem -= chunk;
    }

    ip->atime = tmpfs_now();
    spin_unlock(&ip->lock);
    *got = len;
    return TMPFS_OK;
}

int tmpfs_file_write(tmpfs_inode_t *ip, const void *buf, uint64_t off,
                     size_t len, size_t *put)
{
    if (ip->type != TMPFS_T_REG) return TMPFS_EINVAL;
    if (off + (uint64_t)len > TMPFS_MAX_SZ) return TMPFS_EFBIG;

    spin_lock(&ip->lock);

    const uint8_t *src = buf;
    size_t   rem = len;
    uint64_t cur = off;

    while (rem) {
        uint32_t pgidx = (uint32_t)(cur / TMPFS_PAGE_SIZE);
        uint32_t pgoff = (uint32_t)(cur % TMPFS_PAGE_SIZE);
        size_t   chunk = TMPFS_PAGE_SIZE - pgoff;
        if (chunk > rem) chunk = rem;

        uint8_t *pg = pg_get(ip, pgidx, 1);
        if (!pg) {
            spin_unlock(&ip->lock);
            return TMPFS_ENOMEM;
        }

        memcpy(pg + pgoff, src, chunk);
        src += chunk;
        cur += chunk;
        rem -= chunk;
    }

    if (cur > ip->size) ip->size = cur;
    ip->mtime = tmpfs_now();
    spin_unlock(&ip->lock);
    *put = len;
    return TMPFS_OK;
}

int tmpfs_file_truncate(tmpfs_inode_t *ip, uint64_t newsz)
{
    if (ip->type != TMPFS_T_REG) return TMPFS_EINVAL;
    if (newsz > TMPFS_MAX_SZ)    return TMPFS_EFBIG;

    spin_lock(&ip->lock);

    if (newsz < ip->size) {

        uint32_t first_free = (uint32_t)((newsz + TMPFS_PAGE_SIZE - 1u) / TMPFS_PAGE_SIZE);
        uint32_t last_pg    = (uint32_t)((ip->size + TMPFS_PAGE_SIZE - 1u) / TMPFS_PAGE_SIZE);

        for (uint32_t p = first_free; p < last_pg; p++) {
            uint32_t l1 = p / TMPFS_L2_SZ;
            uint32_t l2 = p % TMPFS_L2_SZ;
            if (l1 >= TMPFS_L1_SZ || !ip->pg_l1[l1]) continue;
            if (ip->pg_l1[l1][l2]) {
                kfree(ip->pg_l1[l1][l2]);
                ip->pg_l1[l1][l2] = NULL;
            }
        }

        uint32_t tail_off = (uint32_t)(newsz % TMPFS_PAGE_SIZE);
        if (tail_off) {
            uint32_t tail_pg = (uint32_t)(newsz / TMPFS_PAGE_SIZE);
            uint8_t *pg = pg_get(ip, tail_pg, 0);
            if (pg)
                memset(pg + tail_off, 0, TMPFS_PAGE_SIZE - tail_off);
        }
    }

    ip->size  = newsz;
    ip->mtime = tmpfs_now();
    spin_unlock(&ip->lock);
    return TMPFS_OK;
}
