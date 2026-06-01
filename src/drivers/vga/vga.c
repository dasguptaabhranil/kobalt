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

#include "vga.h"
#include "fb.h"
#include "fb_console.h"

#define VGA_BASE            0xB8000u
#define VGA_CELLS           (VGA_WIDTH * VGA_HEIGHT)

#define VGA_CRTC_ADDR       0x3D4u
#define VGA_CRTC_DATA       0x3D5u
#define VGA_CRTC_CURSOR_SRT 0x0Au
#define VGA_CRTC_CURSOR_END 0x0Bu
#define VGA_CRTC_CURSOR_HI  0x0Eu
#define VGA_CRTC_CURSOR_LO  0x0Fu

#define VGA_CURSOR_SCANLINE_START 0x0Du
#define VGA_CURSOR_SCANLINE_END   0x0Eu
#define VGA_CURSOR_DISABLE_BIT    0x20u

#define VGA_ENTRY(ch, attr) \
    ((uint16_t)(uint8_t)(ch) | ((uint16_t)(uint8_t)(attr) << 8))

#define VGA_ATTR(fg, bg) \
    ((uint8_t)(((uint8_t)(bg) & 0x07u) << 4) | ((uint8_t)(fg) & 0x0Fu))

static uint16_t *const g_buf = (uint16_t *)VGA_BASE;

typedef struct {
    uint8_t col;
    uint8_t row;
    uint8_t attr;
    uint8_t hw_cursor_on;
} vga_state_t;

static vga_state_t g_vga = {
    .col          = 0,
    .row          = 0,
    .attr         = VGA_ATTR(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK),
    .hw_cursor_on = 1,
};

static inline uint16_t cell_index(uint8_t col, uint8_t row)
{
    return (uint16_t)((uint16_t)row * VGA_WIDTH + (uint16_t)col);
}

static inline void cell_write(uint8_t col, uint8_t row, char ch, uint8_t attr)
{
    g_buf[cell_index(col, row)] = VGA_ENTRY(ch, attr);
}

static void hw_cursor_sync(void)
{
    if (!g_vga.hw_cursor_on) return;
    const uint16_t pos = cell_index(g_vga.col, g_vga.row);
    outb(VGA_CRTC_ADDR, VGA_CRTC_CURSOR_LO);
    outb(VGA_CRTC_DATA, (uint8_t)(pos        & 0xFFu));
    outb(VGA_CRTC_ADDR, VGA_CRTC_CURSOR_HI);
    outb(VGA_CRTC_DATA, (uint8_t)((pos >> 8) & 0xFFu));
}

static void scroll_up(void)
{
    const size_t   move  = (size_t)(VGA_HEIGHT - 1u) * VGA_WIDTH;
    const uint16_t blank = VGA_ENTRY(' ', g_vga.attr);
    for (size_t i = 0; i < move; i++) g_buf[i] = g_buf[i + VGA_WIDTH];
    for (size_t i = move; i < VGA_CELLS; i++) g_buf[i] = blank;
}

static void newline(void)
{
    g_vga.col = 0;
    if ((uint16_t)(g_vga.row + 1u) < VGA_HEIGHT)
        g_vga.row++;
    else
        scroll_up();
}

void vga_init(void)
{
    g_vga.col          = 0;
    g_vga.row          = 0;
    g_vga.attr         = VGA_ATTR(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    g_vga.hw_cursor_on = 1;

    outb(VGA_CRTC_ADDR, VGA_CRTC_CURSOR_SRT);
    outb(VGA_CRTC_DATA, VGA_CURSOR_SCANLINE_START);
    outb(VGA_CRTC_ADDR, VGA_CRTC_CURSOR_END);
    outb(VGA_CRTC_DATA, VGA_CURSOR_SCANLINE_END);

    vga_clear();
}

void vga_set_color(vga_color_t fg, vga_color_t bg)
{
    if (fb_ready()) { fb_console_set_color((uint8_t)fg, (uint8_t)bg); return; }
    g_vga.attr = VGA_ATTR(fg, bg);
}

void vga_reset_color(void)
{
    if (fb_ready()) { fb_console_reset_color(); return; }
    g_vga.attr = VGA_ATTR(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

uint8_t vga_get_attr(void)
{
    return g_vga.attr;
}

void vga_get_cursor(uint8_t *col, uint8_t *row)
{
    if (col) *col = g_vga.col;
    if (row) *row = g_vga.row;
}

void vga_set_cursor(uint8_t col, uint8_t row)
{
    if (col >= VGA_WIDTH)  col = (uint8_t)(VGA_WIDTH  - 1u);
    if (row >= VGA_HEIGHT) row = (uint8_t)(VGA_HEIGHT - 1u);
    g_vga.col = col;
    g_vga.row = row;
    hw_cursor_sync();
}

void vga_cursor_enable(void)
{
    outb(VGA_CRTC_ADDR, VGA_CRTC_CURSOR_SRT);
    outb(VGA_CRTC_DATA, VGA_CURSOR_SCANLINE_START);
    g_vga.hw_cursor_on = 1;
    hw_cursor_sync();
}

void vga_cursor_disable(void)
{
    outb(VGA_CRTC_ADDR, VGA_CRTC_CURSOR_SRT);
    outb(VGA_CRTC_DATA, VGA_CURSOR_DISABLE_BIT);
    g_vga.hw_cursor_on = 0;
}

void vga_clear(void)
{
    if (fb_ready()) { fb_console_clear(); return; }
    const uint16_t blank = VGA_ENTRY(' ', g_vga.attr);
    for (size_t i = 0; i < VGA_CELLS; i++) g_buf[i] = blank;
    g_vga.col = 0;
    g_vga.row = 0;
    hw_cursor_sync();
}

void vga_clear_region(uint8_t col, uint8_t row, uint8_t width, uint8_t height)
{
    const uint16_t blank = VGA_ENTRY(' ', g_vga.attr);
    for (uint8_t r = row; r < (uint8_t)(row + height) && r < VGA_HEIGHT; r++)
        for (uint8_t c = col; c < (uint8_t)(col + width) && c < VGA_WIDTH; c++)
            g_buf[cell_index(c, r)] = blank;
}

void vga_putc(char ch)
{
    if (fb_ready()) { fb_console_putc(ch); return; }

    switch ((uint8_t)ch) {
    case '\n':
        newline();
        break;
    case '\r':
        g_vga.col = 0;
        break;
    case '\b':
        if (g_vga.col > 0) {
            g_vga.col--;
        } else if (g_vga.row > 0) {
            g_vga.row--;
            g_vga.col = (uint8_t)(VGA_WIDTH - 1u);
        }
        cell_write(g_vga.col, g_vga.row, ' ', g_vga.attr);
        break;
    case '\t':
        do {
            cell_write(g_vga.col, g_vga.row, ' ', g_vga.attr);
            g_vga.col++;
            if (g_vga.col >= VGA_WIDTH) newline();
        } while (g_vga.col % 8u != 0);
        break;
    default:
        if ((uint8_t)ch < 0x20u || (uint8_t)ch == 0x7Fu)
            break;
        cell_write(g_vga.col, g_vga.row, ch, g_vga.attr);
        g_vga.col++;
        if (g_vga.col >= VGA_WIDTH) newline();
        break;
    }

    hw_cursor_sync();
}

void vga_puts(const char *s)
{
    if (fb_ready()) { fb_console_puts(s); return; }
    while (*s) vga_putc(*s++);
}

void vga_putc_at(uint8_t col, uint8_t row, uint8_t attr, char ch)
{
    if (col < VGA_WIDTH && row < VGA_HEIGHT)
        cell_write(col, row, ch, attr);
}

void vga_puts_at(uint8_t col, uint8_t row, uint8_t attr, const char *s)
{
    while (*s && col < VGA_WIDTH) {
        cell_write(col, row, *s, attr);
        col++;
        s++;
    }
}
