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

#include "fb.h"
#include "fb_font.h"
#include "fb_console.h"

static const uint32_t g_pal[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
    0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
    0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

#define DEFAULT_FG 7u
#define DEFAULT_BG 0u

static struct {
    uint32_t row, col;
    uint32_t rows, cols;
    uint32_t fw, fh;
    uint32_t fg_rgb, bg_rgb;
} g_cons;

static void cursor_erase(void)
{
    uint32_t x = g_cons.col * g_cons.fw;
    uint32_t y = g_cons.row * g_cons.fh + g_cons.fh - 2u * fb_font_get_scale();
    fb_fill_rect(x, y, g_cons.fw, 2u * fb_font_get_scale(), g_cons.bg_rgb);
}

static void cursor_draw(void)
{
    uint32_t x = g_cons.col * g_cons.fw;
    uint32_t y = g_cons.row * g_cons.fh + g_cons.fh - 2u * fb_font_get_scale();
    fb_fill_rect(x, y, g_cons.fw, 2u * fb_font_get_scale(), g_cons.fg_rgb);
}

static void scroll_up(void)
{
    fb_blit_row(0, g_cons.fh, (g_cons.rows - 1u) * g_cons.fh);
    fb_clear_rows((g_cons.rows - 1u) * g_cons.fh, g_cons.fh, g_cons.bg_rgb);
}

static void newline(void)
{
    g_cons.col = 0;
    if (g_cons.row + 1u < g_cons.rows)
        g_cons.row++;
    else
        scroll_up();
}

int fb_console_init(void)
{
    if (!fb_font_loaded()) return -1;

    const fb_desc_t *fb = fb_get();
    g_cons.fw   = fb_font_w();
    g_cons.fh   = fb_font_h();
    g_cons.cols = fb->width  / g_cons.fw;
    g_cons.rows = fb->height / g_cons.fh;
    g_cons.row  = 0;
    g_cons.col  = 0;
    g_cons.fg_rgb = g_pal[DEFAULT_FG];
    g_cons.bg_rgb = g_pal[DEFAULT_BG];

    if (g_cons.cols == 0 || g_cons.rows == 0) return -1;

    extern void fb_set_ready(void);
    fb_fill_rect(0, 0, fb->width, fb->height, g_cons.bg_rgb);
    fb_set_ready();
    cursor_draw();
    return 0;
}

int fb_console_set_zoom(uint32_t scale)
{
    fb_font_set_scale(scale);
    return fb_console_init();
}

void fb_console_putc(char c)
{
    cursor_erase();

    switch ((uint8_t)c) {
    case '\n':
        newline();
        break;
    case '\r':
        g_cons.col = 0;
        break;
    case '\b':
        if (g_cons.col > 0) {
            g_cons.col--;
        } else if (g_cons.row > 0) {
            g_cons.row--;
            g_cons.col = g_cons.cols - 1u;
        }
        fb_draw_glyph(g_cons.col * g_cons.fw, g_cons.row * g_cons.fh,
                      g_cons.fg_rgb, g_cons.bg_rgb, ' ');
        break;
    case '\t':
        do {
            fb_draw_glyph(g_cons.col * g_cons.fw, g_cons.row * g_cons.fh,
                          g_cons.fg_rgb, g_cons.bg_rgb, ' ');
            g_cons.col++;
            if (g_cons.col >= g_cons.cols) newline();
        } while (g_cons.col & 7u);
        break;
    default:
        if ((uint8_t)c < 0x20u || (uint8_t)c == 0x7Fu)
            break;
        fb_draw_glyph(g_cons.col * g_cons.fw, g_cons.row * g_cons.fh,
                      g_cons.fg_rgb, g_cons.bg_rgb, (uint32_t)(uint8_t)c);
        g_cons.col++;
        if (g_cons.col >= g_cons.cols) newline();
        break;
    }

    cursor_draw();
}

void fb_console_puts(const char *s)
{
    while (*s) fb_console_putc(*s++);
}

void fb_console_set_color(uint8_t fg_idx, uint8_t bg_idx)
{
    if (fg_idx > 15u) fg_idx = DEFAULT_FG;
    if (bg_idx > 15u) bg_idx = DEFAULT_BG;
    g_cons.fg_rgb = g_pal[fg_idx];
    g_cons.bg_rgb = g_pal[bg_idx];
}

void fb_console_reset_color(void)
{
    g_cons.fg_rgb = g_pal[DEFAULT_FG];
    g_cons.bg_rgb = g_pal[DEFAULT_BG];
}

void fb_console_clear(void)
{
    const fb_desc_t *fb = fb_get();
    fb_fill_rect(0, 0, fb->width, fb->height, g_cons.bg_rgb);
    g_cons.row = 0;
    g_cons.col = 0;
    cursor_draw();
}
