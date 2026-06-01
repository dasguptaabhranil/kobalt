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

#include "rbtree.h"

static inline void set_parent(struct rb_node *n, struct rb_node *p)
{
    n->__pc = (n->__pc & 1) | (uintptr_t)p;
}

static void rotate_left(rb_root_t *root, struct rb_node *x)
{
    struct rb_node *y  = x->right;
    struct rb_node *yl = y->left;
    struct rb_node *xp = rb_parent(x);

    y->left  = x;
    x->right = yl;
    if (yl) set_parent(yl, x);
    set_parent(x, y);
    set_parent(y, xp);

    if      (!xp)           root->root  = y;
    else if (xp->left == x) xp->left   = y;
    else                    xp->right  = y;
}

static void rotate_right(rb_root_t *root, struct rb_node *x)
{
    struct rb_node *y  = x->left;
    struct rb_node *yr = y->right;
    struct rb_node *xp = rb_parent(x);

    y->right = x;
    x->left  = yr;
    if (yr) set_parent(yr, x);
    set_parent(x, y);
    set_parent(y, xp);

    if      (!xp)            root->root  = y;
    else if (xp->right == x) xp->right  = y;
    else                     xp->left   = y;
}

static void insert_fixup(rb_root_t *root, struct rb_node *z)
{
    struct rb_node *p, *g, *u;

    for (;;) {
        p = rb_parent(z);
        if (!p || rb_is_black(p))
            break;
        g = rb_parent(p);
        if (!g)
            break;

        if (p == g->left) {
            u = g->right;
            if (u && rb_is_red(u)) {
                rb_set_black(p); rb_set_black(u); rb_set_red(g);
                z = g;
                continue;
            }
            if (z == p->right) {
                rotate_left(root, p);
                z = p;
                p = rb_parent(z);
                g = rb_parent(p);
            }
            rb_set_black(p); rb_set_red(g);
            rotate_right(root, g);
        } else {
            u = g->left;
            if (u && rb_is_red(u)) {
                rb_set_black(p); rb_set_black(u); rb_set_red(g);
                z = g;
                continue;
            }
            if (z == p->left) {
                rotate_right(root, p);
                z = p;
                p = rb_parent(z);
                g = rb_parent(p);
            }
            rb_set_black(p); rb_set_red(g);
            rotate_left(root, g);
        }
        break;
    }
    rb_set_black(root->root);
}

void rb_insert(rb_root_t *root, struct rb_node *node,
               int (*cmp)(struct rb_node *, struct rb_node *))
{
    struct rb_node **link = &root->root, *parent = NULL;

    while (*link) {
        parent = *link;
        link   = (cmp(node, parent) < 0) ? &parent->left : &parent->right;
    }

    node->__pc = (uintptr_t)parent;
    node->left = node->right = NULL;
    *link = node;
    insert_fixup(root, node);
}

static void transplant(rb_root_t *root, struct rb_node *u, struct rb_node *v)
{
    struct rb_node *up = rb_parent(u);
    if (!up)
        root->root = v;
    else if (up->left == u)
        up->left  = v;
    else
        up->right = v;
    if (v) set_parent(v, up);
}

static void delete_fixup(rb_root_t *root, struct rb_node *x, struct rb_node *xp)
{
    struct rb_node *w;

    while (x != root->root && (!x || rb_is_black(x))) {
        if (x == xp->left) {
            w = xp->right;

            if (rb_is_red(w)) {

                rb_set_black(w);
                rb_set_red(xp);
                rotate_left(root, xp);
                w = xp->right;
            }
            if ((!w->left  || rb_is_black(w->left)) &&
                (!w->right || rb_is_black(w->right))) {

                rb_set_red(w);
                x  = xp;
                xp = rb_parent(x);
            } else {
                if (!w->right || rb_is_black(w->right)) {

                    if (w->left) rb_set_black(w->left);
                    rb_set_red(w);
                    rotate_right(root, w);
                    w = xp->right;
                }

                w->__pc  = (w->__pc  & ~1UL) | (xp->__pc & 1);
                rb_set_black(xp);
                if (w->right) rb_set_black(w->right);
                rotate_left(root, xp);
                x  = root->root;
                xp = NULL;
            }
        } else {

            w = xp->left;

            if (rb_is_red(w)) {
                rb_set_black(w);
                rb_set_red(xp);
                rotate_right(root, xp);
                w = xp->left;
            }
            if ((!w->right || rb_is_black(w->right)) &&
                (!w->left  || rb_is_black(w->left))) {
                rb_set_red(w);
                x  = xp;
                xp = rb_parent(x);
            } else {
                if (!w->left || rb_is_black(w->left)) {
                    if (w->right) rb_set_black(w->right);
                    rb_set_red(w);
                    rotate_left(root, w);
                    w = xp->left;
                }
                w->__pc  = (w->__pc  & ~1UL) | (xp->__pc & 1);
                rb_set_black(xp);
                if (w->left) rb_set_black(w->left);
                rotate_right(root, xp);
                x  = root->root;
                xp = NULL;
            }
        }
    }
    if (x) rb_set_black(x);
}

void rb_erase(rb_root_t *root, struct rb_node *z)
{
    struct rb_node *y, *x, *xp;
    int orig_black;

    if (!z->left) {
        x  = z->right;
        xp = rb_parent(z);
        orig_black = rb_is_black(z);
        transplant(root, z, z->right);
    } else if (!z->right) {
        x  = z->left;
        xp = rb_parent(z);
        orig_black = rb_is_black(z);
        transplant(root, z, z->left);
    } else {

        y = z->right;
        while (y->left) y = y->left;

        orig_black = rb_is_black(y);
        x  = y->right;

        if (rb_parent(y) == z) {
            xp = y;
        } else {
            xp = rb_parent(y);
            transplant(root, y, y->right);
            y->right = z->right;
            set_parent(y->right, y);
        }
        transplant(root, z, y);
        y->left = z->left;
        set_parent(y->left, y);

        y->__pc = (y->__pc & ~1UL) | (z->__pc & 1);
    }

    if (orig_black)
        delete_fixup(root, x, xp);
}

struct rb_node *rb_first(const rb_root_t *root)
{
    struct rb_node *n = root->root;
    if (!n) return NULL;
    while (n->left) n = n->left;
    return n;
}

struct rb_node *rb_next(const struct rb_node *n)
{
    struct rb_node *p;
    if (n->right) {
        n = n->right;
        while (n->left) n = n->left;
        return (struct rb_node *)n;
    }
    p = rb_parent(n);
    while (p && n == p->right) { n = p; p = rb_parent(n); }
    return p;
}
