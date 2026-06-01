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
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uintptr_t base;
    uint32_t  pitch;
    uint32_t  width;
    uint32_t  height;
    uint8_t   bpp;
    uint8_t   r_pos;
    uint8_t   g_pos;
    uint8_t   b_pos;
} fb_desc_t;

int              fb_init_from_mbi(void *mbi);
int              fb_init_pvh(void *pvh);
int              fb_ready(void);
void             fb_set_ready(void);
const fb_desc_t *fb_get(void);

uint32_t fb_rgb_to_pixel(uint32_t rgb);
void     fb_putpixel(uint32_t x, uint32_t y, uint32_t rgb);
void     fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t rgb);
void     fb_blit_row(uint32_t dst_y, uint32_t src_y, uint32_t h);
void     fb_clear_rows(uint32_t y, uint32_t h, uint32_t rgb);
