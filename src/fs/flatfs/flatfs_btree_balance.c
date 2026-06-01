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

flatfs_err_t btree_split(flatfs_btnode_t *n, uint64_t parent_blk,
                          flatfs_btkey_t *out_sep, uint64_t *out_sib_blk)
{
    flatfs_btnode_t *sib;
    uint64_t sib_blk;
    flatfs_err_t e = btnode_alloc(&sib, &sib_blk);
    if (e) return e;

    uint16_t mid  = (uint16_t)(n->count / 2);
    uint16_t half = (uint16_t)(n->count - mid);

    btnode_init(sib, sib_blk, parent_blk, n->is_leaf, n->level);

    if (n->is_leaf) {

        *out_sep = n->keys[mid];
        for (uint16_t i = 0; i < half; i++) {
            sib->keys[i]   = n->keys[mid + i];
            sib->values[i] = n->values[mid + i];
        }
        sib->count = half;
        n->count   = mid;

        sib->next_leaf = n->next_leaf;
        sib->prev_leaf = n->self_blk;
        n->next_leaf   = sib_blk;
        if (sib->next_leaf) {
            flatfs_btnode_t *rn = btnode_load(sib->next_leaf);
            rn->prev_leaf = sib_blk;
            btnode_flush(rn);
        }
    } else {

        *out_sep = n->keys[mid];
        for (uint16_t i = 0; i < half - 1; i++)
            sib->keys[i] = n->keys[mid + 1 + i];
        for (uint16_t i = 0; i < half; i++)
            sib->children[i] = n->children[mid + 1 + i];
        sib->count = (uint16_t)(half - 1);
        n->count   = mid;
    }

    btnode_flush(n);
    btnode_flush(sib);
    *out_sib_blk = sib_blk;
    return FLATFS_OK;
}

flatfs_err_t btree_merge(flatfs_btnode_t *ln, flatfs_btnode_t *rn,
                          const flatfs_btkey_t *sep)
{
    if (ln->is_leaf) {
        for (uint16_t i = 0; i < rn->count; i++) {
            ln->keys[ln->count + i]   = rn->keys[i];
            ln->values[ln->count + i] = rn->values[i];
        }
        ln->count    += rn->count;
        ln->next_leaf = rn->next_leaf;
        if (rn->next_leaf) {
            flatfs_btnode_t *nx = btnode_load(rn->next_leaf);
            nx->prev_leaf = ln->self_blk;
            btnode_flush(nx);
        }
    } else {
        ln->keys[ln->count] = *sep;
        ln->count++;
        for (uint16_t i = 0; i < rn->count; i++)
            ln->keys[ln->count + i] = rn->keys[i];
        for (uint16_t i = 0; i <= rn->count; i++)
            ln->children[ln->count + i] = rn->children[i];
        ln->count += rn->count;
    }
    btnode_flush(ln);
    btnode_free(rn->self_blk);
    return FLATFS_OK;
}
