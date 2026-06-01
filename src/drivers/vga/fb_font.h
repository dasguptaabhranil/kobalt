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

int      fb_font_loaded(void);
uint32_t fb_font_w(void);
uint32_t fb_font_h(void);

void     fb_font_set_scale(uint32_t s);
uint32_t fb_font_get_scale(void);

void     fb_draw_glyph(uint32_t x, uint32_t y, uint32_t fg_rgb, uint32_t bg_rgb,
                       uint32_t cp);
