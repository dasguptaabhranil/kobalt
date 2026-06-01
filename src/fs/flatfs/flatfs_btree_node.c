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
#include "flatfs_btree_node.h"
#include "flatfs_tykid.h"
#include "flatfs_alloc.h"
#include "flatfs_crc.h"

#define NCACHE  32u

typedef struct {
    uint64_t        blk;
    uint64_t        stamp;
    int             dirty;
    flatfs_btnode_t node;
} nc_entry_t;

static nc_entry_t  nc[NCACHE];
static uint64_t    nc_clock;

static nc_entry_t *nc_find(uint64_t blk)
{
    for (uint32_t i = 0; i < NCACHE; i++)
        if (nc[i].blk == blk && nc[i].stamp) return &nc[i];
    return NULL;
}

static nc_entry_t *nc_evict(void)
{
    uint64_t lo = UINT64_MAX;
    nc_entry_t *v = &nc[0];
    for (uint32_t i = 0; i < NCACHE; i++) {
        if (!nc[i].stamp) return &nc[i];
        if (nc[i].stamp < lo) { lo = nc[i].stamp; v = &nc[i]; }
    }
    if (v->dirty && v->blk) {
        v->node.magic = FLATFS_BTREE_MAGIC;
        v->node.crc32 = 0;
        v->node.crc32 = flatfs_crc32(&v->node, sizeof(v->node));
        FMEMCPY(blk_ptr(v->blk), &v->node, FLATFS_BLOCK_SIZE);
    }
    return v;
}

flatfs_btnode_t *btnode_load(uint64_t blk)
{
    nc_entry_t *e = nc_find(blk);
    if (e) { e->stamp = ++nc_clock; return &e->node; }

    e = nc_evict();
    e->blk   = blk;
    e->stamp = ++nc_clock;
    e->dirty = 0;
    FMEMCPY(&e->node, blk_ptr(blk), FLATFS_BLOCK_SIZE);
    return &e->node;
}

flatfs_err_t btnode_flush(flatfs_btnode_t *n)
{
    for (uint32_t i = 0; i < NCACHE; i++) {
        if (&nc[i].node == n) {
            n->magic = FLATFS_BTREE_MAGIC;
            uint32_t sv = n->crc32; n->crc32 = 0;
            n->crc32 = flatfs_crc32(n, sizeof(*n));

            flatfs_tykid_mac_set(n, sizeof(*n));
            n->crc32 = 0;
            n->crc32 = flatfs_crc32(n, sizeof(*n));
            FMEMCPY(blk_ptr(nc[i].blk), n, FLATFS_BLOCK_SIZE);
            nc[i].dirty = 0;
            (void)sv;
            return FLATFS_OK;
        }
    }
    return FLATFS_ERR_INVAL;
}

flatfs_err_t btnode_alloc(flatfs_btnode_t **out, uint64_t *out_blk)
{
    uint64_t blk;
    flatfs_err_t e = flatfs_alloc_block(&blk);
    if (e) return e;

    FMEMSET(blk_ptr(blk), 0, FLATFS_BLOCK_SIZE);
    nc_entry_t *slot = nc_evict();
    slot->blk   = blk;
    slot->stamp = ++nc_clock;
    slot->dirty = 1;
    FMEMSET(&slot->node, 0, sizeof(slot->node));
    *out     = &slot->node;
    *out_blk = blk;
    return FLATFS_OK;
}

flatfs_err_t btnode_free(uint64_t blk)
{
    nc_entry_t *e = nc_find(blk);
    if (e) { e->stamp = 0; e->dirty = 0; }
    return flatfs_free_block(blk);
}

void btnode_init(flatfs_btnode_t *n, uint64_t blk, uint64_t parent,
                 int is_leaf, uint32_t level)
{
    FMEMSET(n, 0, sizeof(*n));
    n->magic      = FLATFS_BTREE_MAGIC;
    n->self_blk   = blk;
    n->parent_blk = parent;
    n->is_leaf    = (uint16_t)is_leaf;
    n->level      = level;
}

int btkey_cmp(const flatfs_btkey_t *a, const flatfs_btkey_t *b)
{
    if (a->hash < b->hash) return -1;
    if (a->hash > b->hash) return  1;
    if (a->ino  < b->ino)  return -1;
    if (a->ino  > b->ino)  return  1;
    return 0;
}

uint16_t btnode_lower_bound(const flatfs_btnode_t *n, const flatfs_btkey_t *k)
{
    uint16_t lo = 0, hi = n->count;
    while (lo < hi) {
        uint16_t mid = (uint16_t)(lo + (hi - lo) / 2);
        if (btkey_cmp(&n->keys[mid], k) < 0) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

void btnode_leaf_insert_at(flatfs_btnode_t *n, uint16_t pos,
                            const flatfs_btkey_t *k, uint64_t val)
{
    uint16_t cnt = n->count;
    for (uint16_t i = cnt; i > pos; i--) {
        n->keys[i]   = n->keys[i-1];
        n->values[i] = n->values[i-1];
    }
    n->keys[pos]   = *k;
    n->values[pos] = val;
    n->count++;
}

void btnode_internal_insert_at(flatfs_btnode_t *n, uint16_t pos,
                                const flatfs_btkey_t *k, uint64_t child)
{
    uint16_t cnt = n->count;
    for (uint16_t i = cnt; i > pos; i--)
        n->keys[i] = n->keys[i-1];
    for (uint16_t i = cnt + 1; i > pos + 1; i--)
        n->children[i] = n->children[i-1];
    n->keys[pos]        = *k;
    n->children[pos+1]  = child;
    n->count++;
}

void btnode_remove_at(flatfs_btnode_t *n, uint16_t pos)
{
    uint16_t cnt = n->count;
    for (uint16_t i = pos; i < cnt - 1; i++) {
        n->keys[i] = n->keys[i+1];
        if (n->is_leaf)
            n->values[i] = n->values[i+1];
    }
    if (!n->is_leaf)
        for (uint16_t i = pos + 1; i < cnt; i++)
            n->children[i] = n->children[i+1];
    n->count--;
}
