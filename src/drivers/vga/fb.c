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
#include <kernel.h>
#include <stdint.h>
#include <string.h>

#define MB2_TAG_END         0u
#define MB2_TAG_FB          8u
#define MB2_FB_TYPE_DIRECT  1u

typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t size;
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t  bpp;
    uint8_t  fb_type;
    uint16_t reserved;
    uint8_t  r_pos;
    uint8_t  r_sz;
    uint8_t  g_pos;
    uint8_t  g_sz;
    uint8_t  b_pos;
    uint8_t  b_sz;
} mb2_tag_fb_t;

#define VBE_IDX_PORT    0x01CEu
#define VBE_DATA_PORT   0x01CFu
#define VBE_IDX_ID      0u
#define VBE_IDX_XRES    1u
#define VBE_IDX_YRES    2u
#define VBE_IDX_BPP     3u
#define VBE_IDX_ENABLE  4u
#define VBE_ENABLE_LFB  0x41u

#define PCI_ADDR_PORT   0xCF8u
#define PCI_DATA_PORT   0xCFCu

static fb_desc_t g_fb;
static int       g_ready = 0;

static __inline__ __attribute__((always_inline))
uint16_t _inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile ("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static __inline__ __attribute__((always_inline))
void _outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" :: "a"(val), "Nd"(port));
}

static __inline__ __attribute__((always_inline))
uint32_t _inl(uint16_t port) {
    uint32_t val;
    __asm__ volatile ("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static __inline__ __attribute__((always_inline))
void _outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" :: "a"(val), "Nd"(port));
}

int fb_init_from_mbi(void *mbi)
{
    uint8_t *p = (uint8_t *)mbi + 8;

    for (;;) {
        uint32_t type = *(uint32_t *)p;
        uint32_t sz   = *(uint32_t *)(p + 4);

        if (type == MB2_TAG_END)
            break;

        if (type == MB2_TAG_FB) {
            mb2_tag_fb_t *t = (mb2_tag_fb_t *)p;
            if (t->fb_type != MB2_FB_TYPE_DIRECT || (t->bpp != 32 && t->bpp != 24))
                return -1;
            g_fb.base   = (uintptr_t)t->addr;
            g_fb.pitch  = t->pitch;
            g_fb.width  = t->width;
            g_fb.height = t->height;
            g_fb.bpp    = t->bpp;
            g_fb.r_pos  = t->r_pos;
            g_fb.g_pos  = t->g_pos;
            g_fb.b_pos  = t->b_pos;
            return 0;
        }

        p = (uint8_t *)(((uintptr_t)(p + sz) + 7u) & ~7ul);
    }

    return -1;
}

static inline void vbe_wr(uint16_t idx, uint16_t val)
{
    _outw(VBE_IDX_PORT, idx);
    _outw(VBE_DATA_PORT, val);
}

static inline uint16_t vbe_rd(uint16_t idx)
{
    _outw(VBE_IDX_PORT, idx);
    return _inw(VBE_DATA_PORT);
}

static uint32_t pci_rd(uint8_t bus, uint8_t dev, uint8_t reg)
{
    _outl(PCI_ADDR_PORT,
         (1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)(dev & 0x1F) << 11) | (reg & 0xFC));
    return _inl(PCI_DATA_PORT);
}

int fb_init_pvh(void *pvh)
{
    (void)pvh;

    uint16_t id = vbe_rd(VBE_IDX_ID);
    if (id < 0xB0C0u || id > 0xB0C5u)
        return -1;

    vbe_wr(VBE_IDX_ENABLE, 0);
    vbe_wr(VBE_IDX_XRES,   1280);
    vbe_wr(VBE_IDX_YRES,    800);
    vbe_wr(VBE_IDX_BPP,      32);
    vbe_wr(VBE_IDX_ENABLE,  VBE_ENABLE_LFB);

    uintptr_t base = 0;
    for (uint8_t bus = 0; bus < 4 && !base; bus++) {
        for (uint8_t dev = 0; dev < 32 && !base; dev++) {
            if ((pci_rd(bus, dev, 0) & 0xFFFFu) == 0xFFFFu) continue;
            uint32_t cls = pci_rd(bus, dev, 8) >> 16;
            if (cls != 0x0300u && cls != 0x0380u) continue;
            uint32_t bar0 = pci_rd(bus, dev, 0x10);
            if (bar0 & 1u) continue;
            base = (uintptr_t)(bar0 & ~0xFu);
        }
    }

    if (!base) return -1;

    g_fb.base   = base;
    g_fb.width  = 1280;
    g_fb.height = 800;
    g_fb.bpp    = 32;
    g_fb.pitch  = 1280u * 4u;
    g_fb.r_pos  = 16;
    g_fb.g_pos  = 8;
    g_fb.b_pos  = 0;
    return 0;
}

int fb_ready(void) { return g_ready; }
void fb_set_ready(void) { g_ready = 1; }

const fb_desc_t *fb_get(void) { return &g_fb; }

uint32_t fb_rgb_to_pixel(uint32_t rgb)
{
    uint32_t r = (rgb >> 16) & 0xFFu;
    uint32_t g = (rgb >>  8) & 0xFFu;
    uint32_t b =  rgb        & 0xFFu;
    return (r << g_fb.r_pos) | (g << g_fb.g_pos) | (b << g_fb.b_pos);
}

void fb_putpixel(uint32_t x, uint32_t y, uint32_t rgb)
{
    uint8_t *dst = (uint8_t *)g_fb.base + (uint32_t)y * g_fb.pitch + x * (g_fb.bpp >> 3);
    uint32_t px  = fb_rgb_to_pixel(rgb);
    if (g_fb.bpp == 32) {
        *(uint32_t *)dst = px;
    } else {
        dst[0] = (uint8_t)(px);
        dst[1] = (uint8_t)(px >> 8);
        dst[2] = (uint8_t)(px >> 16);
    }
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t rgb)
{
    uint32_t px  = fb_rgb_to_pixel(rgb);
    uint8_t  bpp = g_fb.bpp >> 3;

    for (uint32_t row = y; row < y + h; row++) {
        uint8_t *dst = (uint8_t *)g_fb.base + row * g_fb.pitch + x * bpp;
        if (g_fb.bpp == 32) {
            uint32_t *d32 = (uint32_t *)dst;
            for (uint32_t col = 0; col < w; col++)
                d32[col] = px;
        } else {
            for (uint32_t col = 0; col < w; col++, dst += 3) {
                dst[0] = (uint8_t)(px);
                dst[1] = (uint8_t)(px >> 8);
                dst[2] = (uint8_t)(px >> 16);
            }
        }
    }
}

void fb_blit_row(uint32_t dst_y, uint32_t src_y, uint32_t h)
{
    uint8_t *dst = (uint8_t *)g_fb.base + dst_y * g_fb.pitch;
    uint8_t *src = (uint8_t *)g_fb.base + src_y * g_fb.pitch;
    memmove(dst, src, (size_t)h * g_fb.pitch);
}

void fb_clear_rows(uint32_t y, uint32_t h, uint32_t rgb)
{
    fb_fill_rect(0, y, g_fb.width, h, rgb);
}
