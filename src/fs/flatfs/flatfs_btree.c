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
#include "flatfs_btree.h"
#include "flatfs_btree_node.h"
#include "flatfs_btree_balance.h"
#include "flatfs_alloc.h"

static struct {
    uint64_t  root_blk;
    uint32_t  height;
    uint64_t  stat_lookups;
    uint64_t  stat_inserts;
    uint64_t  stat_deletes;
    uint64_t  stat_splits;
} g_bt;

flatfs_err_t flatfs_btree_init(uint64_t root_blk)
{
    g_bt.root_blk = root_blk;
    if (!blk_valid(root_blk)) return FLATFS_ERR_INVAL;

    flatfs_btnode_t *r = btnode_load(root_blk);
    if (r->magic != FLATFS_BTREE_MAGIC) {

        btnode_init(r, root_blk, 0, 1, 0);
        r->magic = FLATFS_BTREE_MAGIC;
        btnode_flush(r);
        g_bt.height = 1;
    } else {
        g_bt.height = r->level + 1;
    }
    return FLATFS_OK;
}

flatfs_err_t flatfs_btree_lookup(uint64_t hash, uint64_t ino, uint64_t *val)
{
    g_bt.stat_lookups++;
    flatfs_btkey_t k = { hash, ino };

    uint64_t blk = g_bt.root_blk;
    for (;;) {
        flatfs_btnode_t *n = btnode_load(blk);
        if (!n || n->magic != FLATFS_BTREE_MAGIC) return FLATFS_ERR_CORRUPT;

        uint16_t pos = btnode_lower_bound(n, &k);

        if (n->is_leaf) {
            if (pos < n->count && btkey_cmp(&n->keys[pos], &k) == 0) {
                *val = n->values[pos];
                flatfs_mon_btree_lookup(1);
                return FLATFS_OK;
            }
            flatfs_mon_btree_lookup(0);
            return FLATFS_ERR_NOTFOUND;
        }

        blk = n->children[pos];
    }
}

#define BTREE_MAX_HEIGHT  16u

flatfs_err_t flatfs_btree_insert(uint64_t hash, uint64_t ino, uint64_t val)
{
    g_bt.stat_inserts++;
    flatfs_btkey_t k = { hash, ino };

    uint64_t path_blk[BTREE_MAX_HEIGHT];
    uint16_t path_pos[BTREE_MAX_HEIGHT];
    int      depth = 0;

    uint64_t blk = g_bt.root_blk;

    for (;;) {
        flatfs_btnode_t *n = btnode_load(blk);
        if (!n || n->magic != FLATFS_BTREE_MAGIC) return FLATFS_ERR_CORRUPT;

        if (n->count == FLATFS_BTREE_ORDER) {
            flatfs_btkey_t sep;
            uint64_t sib_blk;
            uint64_t parent = depth > 0 ? path_blk[depth-1] : 0;
            flatfs_err_t e = btree_split(n, parent, &sep, &sib_blk);
            if (e) return e;
            g_bt.stat_splits++;

            if (blk == g_bt.root_blk) {

                flatfs_btnode_t *nr;
                uint64_t nr_blk;
                e = btnode_alloc(&nr, &nr_blk);
                if (e) return e;
                btnode_init(nr, nr_blk, 0, 0, n->level + 1);
                nr->keys[0]     = sep;
                nr->children[0] = blk;
                nr->children[1] = sib_blk;
                nr->count       = 1;
                btnode_flush(nr);
                g_bt.root_blk = nr_blk;
                g_bt.height++;
                g_fs.super->btree_root_blk = nr_blk;

                blk   = nr_blk;
                depth = 0;
                continue;
            } else {

                flatfs_btnode_t *par = btnode_load(path_blk[depth-1]);
                uint16_t ppos = path_pos[depth-1];
                btnode_internal_insert_at(par, ppos, &sep, sib_blk);
                btnode_flush(par);

                if (btkey_cmp(&k, &sep) >= 0) blk = sib_blk;

                continue;
            }
        }

        uint16_t pos = btnode_lower_bound(n, &k);

        if (n->is_leaf) {
            if (pos < n->count && btkey_cmp(&n->keys[pos], &k) == 0)
                return FLATFS_ERR_EXIST;
            btnode_leaf_insert_at(n, pos, &k, val);
            btnode_flush(n);
            return FLATFS_OK;
        }
        path_blk[depth] = blk;
        path_pos[depth] = pos;
        depth++;
        blk = n->children[pos];
    }
}

flatfs_err_t flatfs_btree_delete(uint64_t hash, uint64_t ino)
{
    g_bt.stat_deletes++;
    flatfs_btkey_t k = { hash, ino };

    uint64_t blk = g_bt.root_blk;
    for (;;) {
        flatfs_btnode_t *n = btnode_load(blk);
        if (!n || n->magic != FLATFS_BTREE_MAGIC) return FLATFS_ERR_CORRUPT;

        uint16_t pos = btnode_lower_bound(n, &k);
        if (n->is_leaf) {
            if (pos >= n->count || btkey_cmp(&n->keys[pos], &k) != 0)
                return FLATFS_ERR_NOTFOUND;
            btnode_remove_at(n, pos);
            btnode_flush(n);
            return FLATFS_OK;
        }
        blk = n->children[pos];
    }

}

flatfs_err_t flatfs_btree_verify(void)
{
    uint64_t blk = g_bt.root_blk;
    flatfs_btnode_t *n = btnode_load(blk);
    if (!n || n->magic != FLATFS_BTREE_MAGIC) return FLATFS_ERR_CORRUPT;
    return FLATFS_OK;
}

void flatfs_btree_stats(uint64_t *lookups, uint64_t *inserts,
                         uint64_t *deletes, uint64_t *splits)
{
    *lookups = g_bt.stat_lookups;
    *inserts = g_bt.stat_inserts;
    *deletes = g_bt.stat_deletes;
    *splits  = g_bt.stat_splits;
}
