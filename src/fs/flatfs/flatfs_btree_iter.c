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
#include "flatfs_btree_iter.h"
#include "flatfs_btree_node.h"

static uint64_t find_leaf(const flatfs_btkey_t *from)
{
    uint64_t blk = g_fs.super->btree_root_blk;
    for (;;) {
        flatfs_btnode_t *n = btnode_load(blk);
        if (n->is_leaf) return blk;
        uint16_t pos = btnode_lower_bound(n, from);

        if (pos > 0 && btkey_cmp(&n->keys[pos-1], from) == 0)
            blk = n->children[pos];
        else
            blk = (pos < n->count + 1) ? n->children[pos] : n->children[n->count];
    }
}

flatfs_err_t flatfs_btiter_init(flatfs_btiter_t *it,
                                 const flatfs_btkey_t *from)
{
    FMEMSET(it, 0, sizeof(*it));
    it->leaf_blk = find_leaf(from);
    if (!it->leaf_blk) { it->done = 1; return FLATFS_ERR_NOTFOUND; }

    flatfs_btnode_t *leaf = btnode_load(it->leaf_blk);
    if (from) {
        it->pos = btnode_lower_bound(leaf, from);
    } else {
        it->pos = 0;
    }
    if (it->pos >= leaf->count) {

        if (!leaf->next_leaf) { it->done = 1; return FLATFS_OK; }
        it->leaf_blk = leaf->next_leaf;
        it->pos      = 0;
        leaf         = btnode_load(it->leaf_blk);
    }
    it->current_key = leaf->keys[it->pos];
    it->current_val = leaf->values[it->pos];
    return FLATFS_OK;
}

flatfs_err_t flatfs_btiter_next(flatfs_btiter_t *it)
{
    if (it->done) return FLATFS_ERR_NOTFOUND;

    flatfs_btnode_t *leaf = btnode_load(it->leaf_blk);
    it->pos++;
    if (it->pos >= leaf->count) {
        if (!leaf->next_leaf) { it->done = 1; return FLATFS_ERR_NOTFOUND; }
        it->leaf_blk = leaf->next_leaf;
        it->pos      = 0;
        leaf         = btnode_load(it->leaf_blk);
        if (!leaf->count) { it->done = 1; return FLATFS_ERR_NOTFOUND; }
    }
    it->current_key = leaf->keys[it->pos];
    it->current_val = leaf->values[it->pos];
    return FLATFS_OK;
}
