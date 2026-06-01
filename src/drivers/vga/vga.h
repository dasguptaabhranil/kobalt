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

#ifndef VGA_H
#define VGA_H

#include <kernel.h>

#define VGA_WIDTH    80u
#define VGA_HEIGHT   25u

typedef enum {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_YELLOW        = 14,
    VGA_COLOR_WHITE         = 15,
} vga_color_t;

void    vga_init(void);

void    vga_clear(void);

void    vga_clear_region(uint8_t col, uint8_t row,
                         uint8_t width, uint8_t height);

void    vga_putc(char ch);

void    vga_puts(const char *s);

void    vga_putc_at(uint8_t col, uint8_t row, uint8_t attr, char ch);

void    vga_puts_at(uint8_t col, uint8_t row, uint8_t attr, const char *s);

void    vga_set_color(vga_color_t fg, vga_color_t bg);

void    vga_reset_color(void);

uint8_t vga_get_attr(void);

void    vga_get_cursor(uint8_t *col, uint8_t *row);

void    vga_set_cursor(uint8_t col, uint8_t row);

void    vga_cursor_enable(void);

void    vga_cursor_disable(void);

#endif
