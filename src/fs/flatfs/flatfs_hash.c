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
#include "flatfs_hash.h"
#include "flatfs_alloc.h"
#include "flatfs_inode.h"
#include "flatfs_crc.h"

#define FNV1A_32_BASIS  0x811c9dc5u
#define FNV1A_32_PRIME  0x01000193u

uint32_t flatfs_hash_name(const char *name, size_t len)
{
    uint32_t h = FNV1A_32_BASIS;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint8_t)name[i];
        h *= FNV1A_32_PRIME;
    }
    return h;
}

static flatfs_htree_root_t *htroot(uint64_t dir_ino)
{
    flatfs_inode_t *in = inode_ptr(dir_ino);
    if (!in->dir_htree_blk) return NULL;
    return (flatfs_htree_root_t *)blk_ptr(in->dir_htree_blk);
}

static flatfs_dirent_t *leaf_blk(uint64_t blk)
{
    return (flatfs_dirent_t *)blk_ptr(blk);
}

flatfs_err_t flatfs_htree_init(uint64_t dir_ino, uint64_t root_blk)
{
    flatfs_htree_root_t *rt = (flatfs_htree_root_t *)blk_ptr(root_blk);
    FMEMSET(rt, 0, sizeof(*rt));
    rt->magic = FLATFS_HTREE_MAGIC;
    rt->depth = 1;

    rt->crc32 = 0;
    rt->crc32 = flatfs_crc32(rt, sizeof(*rt));

    flatfs_inode_t *in = inode_ptr(dir_ino);
    in->dir_htree_blk = root_blk;
    in->htree_depth   = 1;
    in->flags |= FLATFS_FL_HTREE;
    return flatfs_inode_write(in);
}

static uint64_t ensure_leaf(flatfs_htree_root_t *rt, uint8_t bucket)
{
    if (rt->children[bucket]) return rt->children[bucket];

    uint64_t blk;
    if (flatfs_alloc_block(&blk) != FLATFS_OK) return 0;
    FMEMSET(blk_ptr(blk), 0, FLATFS_BLOCK_SIZE);
    rt->children[bucket] = blk;
    rt->crc32 = 0;
    rt->crc32 = flatfs_crc32(rt, sizeof(*rt));
    return blk;
}

flatfs_err_t flatfs_htree_lookup(uint64_t dir_ino, const char *name,
                                  uint64_t *out_ino)
{
    flatfs_htree_root_t *rt = htroot(dir_ino);
    if (!rt || rt->magic != FLATFS_HTREE_MAGIC) return FLATFS_ERR_CORRUPT;

    size_t nlen = fstrlen(name, FLATFS_NAME_MAX);
    uint32_t h  = flatfs_hash_name(name, nlen);
    uint8_t  bucket = (uint8_t)(h >> 24);

    uint64_t lb = rt->children[bucket];
    if (!lb) return FLATFS_ERR_NOTFOUND;

    flatfs_dirent_t *de = leaf_blk(lb);
    for (uint32_t i = 0; i < FLATFS_DIRENTS_PER_BLK; i++) {
        if (!de[i].ino) continue;
        if (de[i].hash != h) continue;
        if (de[i].name_len != (uint8_t)nlen) continue;
        if (fstrncmp(de[i].name, name, nlen) == 0) {
            *out_ino = de[i].ino;
            flatfs_mon_htree_lookup(1);
            return FLATFS_OK;
        }
    }
    flatfs_mon_htree_lookup(0);
    return FLATFS_ERR_NOTFOUND;
}

flatfs_err_t flatfs_htree_insert(uint64_t dir_ino, const char *name,
                                  uint64_t ino, uint8_t ftype)
{
    flatfs_htree_root_t *rt = htroot(dir_ino);
    if (!rt || rt->magic != FLATFS_HTREE_MAGIC) return FLATFS_ERR_CORRUPT;

    size_t nlen = fstrlen(name, FLATFS_NAME_MAX);
    if (nlen >= FLATFS_NAME_MAX) return FLATFS_ERR_NAMETOOLONG;

    uint32_t h  = flatfs_hash_name(name, nlen);
    uint8_t  bk = (uint8_t)(h >> 24);

    uint64_t lb = ensure_leaf(rt, bk);
    if (!lb) return FLATFS_ERR_NOSPACE;

    flatfs_dirent_t *de = leaf_blk(lb);
    for (uint32_t i = 0; i < FLATFS_DIRENTS_PER_BLK; i++) {
        if (de[i].ino) {

            if (de[i].hash == h && de[i].name_len == (uint8_t)nlen
                && fstrncmp(de[i].name, name, nlen) == 0)
                return FLATFS_ERR_EXIST;
            continue;
        }

        FMEMSET(&de[i], 0, sizeof(de[i]));
        de[i].ino       = ino;
        de[i].hash      = h;
        de[i].name_len  = (uint8_t)nlen;
        de[i].file_type = ftype;
        de[i].rec_len   = FLATFS_DIRENT_SIZE;
        FMEMCPY(de[i].name, name, nlen);
        de[i].crc32 = 0;
        de[i].crc32 = flatfs_crc32(&de[i], sizeof(de[i]));

        rt->count++;
        rt->crc32 = 0;
        rt->crc32 = flatfs_crc32(rt, sizeof(*rt));

        flatfs_inode_t *dirin = inode_ptr(dir_ino);
        dirin->size++;
        return flatfs_inode_write(dirin);
    }
    return FLATFS_ERR_NOSPACE;
}

flatfs_err_t flatfs_htree_remove(uint64_t dir_ino, const char *name)
{
    flatfs_htree_root_t *rt = htroot(dir_ino);
    if (!rt || rt->magic != FLATFS_HTREE_MAGIC) return FLATFS_ERR_CORRUPT;

    size_t nlen = fstrlen(name, FLATFS_NAME_MAX);
    uint32_t h  = flatfs_hash_name(name, nlen);
    uint8_t  bk = (uint8_t)(h >> 24);

    uint64_t lb = rt->children[bk];
    if (!lb) return FLATFS_ERR_NOTFOUND;

    flatfs_dirent_t *de = leaf_blk(lb);
    for (uint32_t i = 0; i < FLATFS_DIRENTS_PER_BLK; i++) {
        if (!de[i].ino || de[i].hash != h) continue;
        if (de[i].name_len != (uint8_t)nlen) continue;
        if (fstrncmp(de[i].name, name, nlen) == 0) {
            FMEMSET(&de[i], 0, sizeof(de[i]));
            rt->count--;
            rt->crc32 = 0;
            rt->crc32 = flatfs_crc32(rt, sizeof(*rt));

            flatfs_inode_t *dirin = inode_ptr(dir_ino);
            if (dirin->size) dirin->size--;
            return flatfs_inode_write(dirin);
        }
    }
    return FLATFS_ERR_NOTFOUND;
}

flatfs_err_t flatfs_htree_iterate(uint64_t dir_ino, uint32_t *pos,
                                   flatfs_dirent_info_t *out)
{
    flatfs_htree_root_t *rt = htroot(dir_ino);
    if (!rt || rt->magic != FLATFS_HTREE_MAGIC) return FLATFS_ERR_CORRUPT;

    uint32_t global = *pos;
    uint32_t per    = FLATFS_DIRENTS_PER_BLK;

    for (uint32_t bk = global / per; bk < FLATFS_HTREE_FANOUT; bk++) {
        if (!rt->children[bk]) { global = (bk + 1) * per; continue; }
        flatfs_dirent_t *de = leaf_blk(rt->children[bk]);
        for (uint32_t i = (bk == global / per) ? (global % per) : 0;
             i < per; i++) {
            global = bk * per + i + 1;
            if (!de[i].ino) continue;
            out->ino      = de[i].ino;
            out->type     = de[i].file_type;
            out->name_len = de[i].name_len;
            FMEMCPY(out->name, de[i].name, de[i].name_len);
            out->name[de[i].name_len] = 0;
            *pos = global;
            return FLATFS_OK;
        }
    }
    *pos = FLATFS_HTREE_FANOUT * per;
    return FLATFS_ERR_NOTFOUND;
}

flatfs_err_t flatfs_htree_isempty(uint64_t dir_ino, int *empty)
{
    flatfs_htree_root_t *rt = htroot(dir_ino);
    if (!rt) { *empty = 1; return FLATFS_OK; }
    *empty = (rt->count == 0);
    return FLATFS_OK;
}
