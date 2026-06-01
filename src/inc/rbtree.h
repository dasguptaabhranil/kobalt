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

#pragma once
#include <stddef.h>
#include <stdint.h>

struct rb_node {
    uintptr_t       __pc;
    struct rb_node *left;
    struct rb_node *right;
};

typedef struct { struct rb_node *root; } rb_root_t;

#define RB_ROOT             { NULL }
#define rb_parent(n)        ((struct rb_node *)((n)->__pc & ~1UL))
#define rb_is_red(n)        (!((n)->__pc & 1))
#define rb_is_black(n)      ((n)->__pc & 1)
#define rb_color(n)         ((int)((n)->__pc & 1))
#define rb_set_black(n)     ((n)->__pc |= 1UL)
#define rb_set_red(n)       ((n)->__pc &= ~1UL)

#define rb_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

void            rb_insert(rb_root_t *root, struct rb_node *node,
                           int (*cmp)(struct rb_node *, struct rb_node *));
void            rb_erase(rb_root_t *root, struct rb_node *node);
struct rb_node *rb_first(const rb_root_t *root);
struct rb_node *rb_next(const struct rb_node *n);
