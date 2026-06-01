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

#include <kernel.h>
#include <pci.h>
#include "hda.h"

#define HDA_MEMSET(d,c,n)  __builtin_memset((d),(c),(n))
#define HDA_MEMCPY(d,s,n)  __builtin_memcpy((d),(s),(n))

extern void irq_register(uint8_t irq, void (*handler)(void));

static inline uint32_t hda_rd32(uintptr_t base, uint32_t off)
{
    return *(volatile uint32_t *)(base + off);
}
static inline void hda_wr32(uintptr_t base, uint32_t off, uint32_t val)
{
    *(volatile uint32_t *)(base + off) = val;
}
static inline uint16_t hda_rd16(uintptr_t base, uint32_t off)
{
    return *(volatile uint16_t *)(base + off);
}
static inline void hda_wr16(uintptr_t base, uint32_t off, uint16_t val)
{
    *(volatile uint16_t *)(base + off) = val;
}
static inline uint8_t hda_rd8(uintptr_t base, uint32_t off)
{
    return *(volatile uint8_t *)(base + off);
}
static inline void hda_wr8(uintptr_t base, uint32_t off, uint8_t val)
{
    *(volatile uint8_t *)(base + off) = val;
}

static inline uint32_t hda_sd_off(uint8_t idx)
{
    return HDA_REG_SD_BASE + (uint32_t)idx * HDA_REG_SD_SIZE;
}

static uint32_t         g_corb[HDA_CORB_ENTRIES]                    __attribute__((aligned(128)));
static hda_rirb_entry_t g_rirb[HDA_RIRB_ENTRIES]                    __attribute__((aligned(128)));
static hda_bdl_entry_t  g_bdl[HDA_MAX_STREAMS][HDA_BDL_ENTRIES]     __attribute__((aligned(128)));

#define HDA_DMA_BUF_SIZE  (HDA_BDL_ENTRIES * HDA_PERIOD_BYTES)
static uint8_t g_dma_buf[HDA_MAX_STREAMS][HDA_DMA_BUF_SIZE]         __attribute__((aligned(4096)));

static struct {
    uintptr_t  mmio;
    uint8_t    pci_bus, pci_dev, pci_fn, irq;
    uint8_t    num_oss, num_iss, num_bss, num_sdo;
    uint8_t    support_64bit;
    uint16_t   corb_wp, rirb_rp;
    hda_codec_t         codecs[HDA_MAX_CODECS];
    uint8_t             codec_count;
    hda_stream_t        streams[HDA_MAX_STREAMS];
    uint8_t             master_vol, master_muted;
    hda_mixer_channel_t mixer[HDA_MIXER_CHANNELS];
    hda_eq_band_t       eq[HDA_MAX_EQ_BANDS];
    uint8_t             eq_enabled;
    int                 initialised;
} g_hda;

extern hda_status_t hda_codec_enumerate(void);
extern void         hda_codec_route_default_paths(void);
extern hda_status_t hda_verb_exec(uint8_t codec, uint8_t nid,
                                   uint32_t verb_id, uint32_t payload,
                                   uint32_t *resp_out);
extern void         hda_mixer_apply_vol(void);

static void hda_udelay(uint32_t us)
{
    volatile uint32_t n = us * 1000u;
    while (n--) __asm__ volatile("nop");
}

static hda_status_t hda_controller_reset(void)
{
    uintptr_t b = g_hda.mmio;

    hda_wr32(b, HDA_REG_GCTL, hda_rd32(b, HDA_REG_GCTL) & ~(uint32_t)HDA_GCTL_RESET);
    for (uint32_t i = 0; i < 500; i++) {
        if (!(hda_rd32(b, HDA_REG_GCTL) & HDA_GCTL_RESET)) break;
        hda_udelay(100);
    }
    hda_udelay(1000);

    hda_wr32(b, HDA_REG_GCTL, hda_rd32(b, HDA_REG_GCTL) | HDA_GCTL_RESET);
    for (uint32_t i = 0; i < 5000; i++) {
        if (hda_rd32(b, HDA_REG_GCTL) & HDA_GCTL_RESET) break;
        hda_udelay(100);
    }
    if (!(hda_rd32(b, HDA_REG_GCTL) & HDA_GCTL_RESET))
        return HDA_ERR_RESET;

    hda_wr32(b, HDA_REG_GCTL, hda_rd32(b, HDA_REG_GCTL) | HDA_GCTL_UNSOL);
    hda_udelay(521);
    return HDA_OK;
}

static void hda_corb_init(void)
{
    uintptr_t b = g_hda.mmio;

    hda_wr8(b, HDA_REG_CORBCTL, 0);
    hda_udelay(50);

    uint64_t phys = (uint64_t)(uintptr_t)g_corb;
    hda_wr32(b, HDA_REG_CORBLBASE, (uint32_t)(phys & 0xFFFFFFFFu));
    hda_wr32(b, HDA_REG_CORBUBASE, (uint32_t)(phys >> 32));
    hda_wr8(b, HDA_REG_CORBSIZE, HDA_RINGBUF_SIZE_256);

    hda_wr16(b, HDA_REG_CORBRP, hda_rd16(b, HDA_REG_CORBRP) | HDA_CORBRP_RST);
    hda_udelay(50);
    hda_wr16(b, HDA_REG_CORBRP, hda_rd16(b, HDA_REG_CORBRP) & ~(uint16_t)HDA_CORBRP_RST);
    hda_wr16(b, HDA_REG_CORBWP, 0);
    g_hda.corb_wp = 0;

    hda_wr8(b, HDA_REG_CORBCTL, HDA_CORBCTL_RUN);
}

static void hda_rirb_init(void)
{
    uintptr_t b = g_hda.mmio;

    hda_wr8(b, HDA_REG_RIRBCTL, 0);
    hda_udelay(50);

    uint64_t phys = (uint64_t)(uintptr_t)g_rirb;
    hda_wr32(b, HDA_REG_RIRBLBASE, (uint32_t)(phys & 0xFFFFFFFFu));
    hda_wr32(b, HDA_REG_RIRBUBASE, (uint32_t)(phys >> 32));
    hda_wr8(b, HDA_REG_RIRBSIZE, HDA_RINGBUF_SIZE_256);
    hda_wr16(b, HDA_REG_RIRBWP, HDA_RIRBWP_RST);
    hda_wr16(b, HDA_REG_RINTCNT, 1);
    g_hda.rirb_rp = 0;

    hda_wr8(b, HDA_REG_RIRBCTL, HDA_RIRBCTL_DMAEN | HDA_RIRBCTL_RINTCTL);
}

hda_status_t hda_verb_exec(uint8_t codec, uint8_t nid,
                            uint32_t verb_id, uint32_t payload,
                            uint32_t *resp_out)
{
    uintptr_t b = g_hda.mmio;

    uint32_t verb = (verb_id <= 0xFFFu)
        ? HDA_VERB(codec, nid, verb_id, payload & 0xFFu)
        : verb_id;

    uint16_t wp = (g_hda.corb_wp + 1u) & 0xFFu;
    g_corb[wp] = verb;
    g_hda.corb_wp = wp;
    hda_wr16(b, HDA_REG_CORBWP, wp);

    uint32_t timeout = HDA_VERB_TIMEOUT_US * 10u;
    while (timeout--) {
        uint16_t rirb_wp = hda_rd16(b, HDA_REG_RIRBWP) & 0xFFu;
        uint16_t rp      = (g_hda.rirb_rp + 1u) & 0xFFu;
        if (rirb_wp == g_hda.rirb_rp) { hda_udelay(1); continue; }
        g_hda.rirb_rp = rp;
        if (resp_out) *resp_out = g_rirb[rp].response;
        hda_wr8(b, HDA_REG_RIRBSTS, hda_rd8(b, HDA_REG_RIRBSTS));
        return HDA_OK;
    }
    return HDA_ERR_VERB_TIMEOUT;
}

static hda_status_t hda_calc_fmt(const hda_pcm_format_t *f, uint16_t *fmt_out)
{
    uint16_t fmt = 0, base, mult, div;

    switch (f->sample_rate) {
    case   8000: base = HDA_FMT_BASE_48;  mult = HDA_FMT_MULT_1X; div = 6u; break;
    case  11025: base = HDA_FMT_BASE_441; mult = HDA_FMT_MULT_1X; div = 4u; break;
    case  16000: base = HDA_FMT_BASE_48;  mult = HDA_FMT_MULT_1X; div = 3u; break;
    case  22050: base = HDA_FMT_BASE_441; mult = HDA_FMT_MULT_1X; div = 2u; break;
    case  32000: base = HDA_FMT_BASE_48;  mult = HDA_FMT_MULT_2X; div = 3u; break;
    case  44100: base = HDA_FMT_BASE_441; mult = HDA_FMT_MULT_1X; div = 1u; break;
    case  48000: base = HDA_FMT_BASE_48;  mult = HDA_FMT_MULT_1X; div = 1u; break;
    case  88200: base = HDA_FMT_BASE_441; mult = HDA_FMT_MULT_2X; div = 1u; break;
    case  96000: base = HDA_FMT_BASE_48;  mult = HDA_FMT_MULT_2X; div = 1u; break;
    case 176400: base = HDA_FMT_BASE_441; mult = HDA_FMT_MULT_4X; div = 1u; break;
    case 192000: base = HDA_FMT_BASE_48;  mult = HDA_FMT_MULT_4X; div = 1u; break;
    default:     return HDA_ERR_FMT;
    }
    fmt |= base | mult | HDA_FMT_DIV(div);

    uint16_t bits;
    if (f->is_float) {
        bits = HDA_FMT_BITS_32;
        fmt |= (1u << 15);
    } else {
        switch (f->bits) {
        case  8: bits = HDA_FMT_BITS_8;  break;
        case 16: bits = HDA_FMT_BITS_16; break;
        case 20: bits = HDA_FMT_BITS_20; break;
        case 24: bits = HDA_FMT_BITS_24; break;
        case 32: bits = HDA_FMT_BITS_32; break;
        default: return HDA_ERR_FMT;
        }
    }
    fmt |= bits;

    if (f->channels < 1u || f->channels > 8u) return HDA_ERR_FMT;
    fmt |= HDA_FMT_CHAN(f->channels);
    *fmt_out = fmt;
    return HDA_OK;
}

static void hda_build_bdl(hda_stream_t *s)
{
    hda_bdl_entry_t *bdl = g_bdl[s->idx];
    s->bdl          = bdl;
    s->bdl_phys     = (uint64_t)(uintptr_t)bdl;
    s->dma_buf      = g_dma_buf[s->idx];
    s->dma_buf_phys = (uint64_t)(uintptr_t)s->dma_buf;
    HDA_MEMSET(s->dma_buf, 0, HDA_DMA_BUF_SIZE);

    for (uint32_t i = 0; i < HDA_BDL_ENTRIES; i++) {
        uint64_t ep    = s->dma_buf_phys + i * HDA_PERIOD_BYTES;
        bdl[i].addr_lo = (uint32_t)(ep & 0xFFFFFFFFu);
        bdl[i].addr_hi = (uint32_t)(ep >> 32);
        bdl[i].length  = HDA_PERIOD_BYTES;
        bdl[i].flags   = HDA_BDL_IOC;
    }

    s->buf_size    = HDA_DMA_BUF_SIZE;
    s->period_size = HDA_PERIOD_BYTES;
    s->periods     = HDA_BDL_ENTRIES;
    s->write_pos   = 0;
    s->read_pos    = 0;
}

static hda_status_t hda_sd_reset(hda_stream_t *s)
{
    uintptr_t b   = g_hda.mmio;
    uint32_t  off = hda_sd_off(s->idx);

    hda_wr32(b, off + HDA_SD_CTL, hda_rd32(b, off + HDA_SD_CTL) | HDA_SDCTL_SRST);
    for (uint32_t i = 0; i < 300; i++) {
        if (hda_rd32(b, off + HDA_SD_CTL) & HDA_SDCTL_SRST) break;
        hda_udelay(10);
    }
    hda_udelay(100);

    hda_wr32(b, off + HDA_SD_CTL, hda_rd32(b, off + HDA_SD_CTL) & ~(uint32_t)HDA_SDCTL_SRST);
    for (uint32_t i = 0; i < 300; i++) {
        if (!(hda_rd32(b, off + HDA_SD_CTL) & HDA_SDCTL_SRST)) break;
        hda_udelay(10);
    }
    if (hda_rd32(b, off + HDA_SD_CTL) & HDA_SDCTL_SRST)
        return HDA_ERR_RESET;
    return HDA_OK;
}

static hda_status_t hda_sd_program(hda_stream_t *s, uint16_t fmt)
{
    uintptr_t b   = g_hda.mmio;
    uint32_t  off = hda_sd_off(s->idx);

    hda_wr8(b,  off + HDA_SD_STS,  HDA_SDSTS_BCIS | HDA_SDSTS_FIFOE | HDA_SDSTS_DESE);
    hda_wr32(b, off + HDA_SD_CBL,  s->buf_size);
    hda_wr16(b, off + HDA_SD_LVI,  (uint16_t)(s->periods - 1u));
    hda_wr16(b, off + HDA_SD_FMT,  fmt);
    hda_wr32(b, off + HDA_SD_BDPL, (uint32_t)(s->bdl_phys & 0xFFFFFFFFu));
    hda_wr32(b, off + HDA_SD_BDPU, (uint32_t)(s->bdl_phys >> 32));
    hda_wr32(b, off + HDA_SD_CTL,
             ((uint32_t)s->strm_num << HDA_SDCTL_STRM_SHIFT) | HDA_SDCTL_IOCE);
    return HDA_OK;
}

void hda_irq_handler(void)
{
    uintptr_t b      = g_hda.mmio;
    uint32_t  intsts = hda_rd32(b, HDA_REG_INTSTS);
    if (!(intsts & HDA_INTSTS_GIS)) return;

    if (intsts & HDA_INTSTS_CIS) {
        uint8_t sts = hda_rd8(b, HDA_REG_RIRBSTS);
        if (sts & 0x04u) kprintf("[HDA] RIRB overrun!\n");
        hda_wr8(b, HDA_REG_RIRBSTS, sts);
    }

    uint16_t rirb_wp = hda_rd16(b, HDA_REG_RIRBWP) & 0xFFu;
    uint32_t guard   = HDA_RIRB_ENTRIES;
    while (g_hda.rirb_rp != rirb_wp && guard--) {
        g_hda.rirb_rp = (g_hda.rirb_rp + 1u) & 0xFFu;
        const hda_rirb_entry_t *e = &g_rirb[g_hda.rirb_rp];
        if (e->ex_resp & 0x1u) {
            (void)e;
        }
    }

    uint32_t ntotal = g_hda.num_iss + g_hda.num_oss + g_hda.num_bss;
    for (uint32_t i = 0; i < ntotal && i < HDA_MAX_STREAMS; i++) {
        if (!(intsts & (1u << i))) continue;
        uint32_t off = hda_sd_off((uint8_t)i);
        uint8_t  sts = hda_rd8(b, off + HDA_SD_STS);
        hda_wr8(b, off + HDA_SD_STS, sts);
        if (!(sts & HDA_SDSTS_BCIS)) continue;
        hda_stream_t *s = &g_hda.streams[i];
        if (!s->running) continue;
        s->read_pos = hda_rd32(b, off + HDA_SD_LPIB);
        if (s->period_cb) s->period_cb(s, s->cb_priv);
    }

    hda_wr32(b, HDA_REG_INTSTS, intsts);
}

hda_status_t hda_init(pci_device_t *pdev)
{
    HDA_MEMSET(&g_hda, 0, sizeof(g_hda));

    uint16_t cmd = pci_read_config16(pdev, 0x04u);
    cmd |= (1u << 1) | (1u << 2);
    pci_write_config16(pdev, 0x04u, cmd);

    uint32_t bl = pci_read_config32(pdev, 0x10u) & ~0xFU;
    uint32_t bh = pci_read_config32(pdev, 0x14u);
    g_hda.mmio = (uintptr_t)(((uint64_t)bh << 32) | bl);
    if (!g_hda.mmio)
        return HDA_ERR_NOT_FOUND;

    uint16_t gcap = hda_rd16(g_hda.mmio, HDA_REG_GCAP);
    if (gcap == 0x0000u || gcap == 0xFFFFu)
        return HDA_ERR_NOT_FOUND;

    g_hda.num_oss       = (uint8_t)((gcap & HDA_GCAP_OSS_MASK)  >> HDA_GCAP_OSS_SHIFT);
    g_hda.num_iss       = (uint8_t)((gcap & HDA_GCAP_ISS_MASK)  >> HDA_GCAP_ISS_SHIFT);
    g_hda.num_bss       = (uint8_t)((gcap & HDA_GCAP_BSS_MASK)  >> HDA_GCAP_BSS_SHIFT);
    g_hda.num_sdo       = (uint8_t)((gcap & HDA_GCAP_NSDO_MASK) >> HDA_GCAP_NSDO_SHIFT);
    g_hda.support_64bit = (gcap & HDA_GCAP_64OK) ? 1u : 0u;

    hda_status_t rc = hda_controller_reset();
    if (rc != HDA_OK) return rc;

    hda_corb_init();
    hda_rirb_init();

    g_hda.irq = (uint8_t)(pci_read_config32(pdev, 0x3Cu) & 0xFFu);
    if (g_hda.irq && g_hda.irq != 0xFFu)
        irq_register(g_hda.irq, hda_irq_handler);

    hda_wr32(g_hda.mmio, HDA_REG_INTCTL,
             HDA_INTCTL_GIE | HDA_INTCTL_CIE | HDA_INTCTL_SIE_ALL);

    rc = hda_codec_enumerate();
    if (rc != HDA_OK) return rc;

    hda_codec_route_default_paths();

    g_hda.master_vol   = 80u;
    g_hda.master_muted = 0u;
    hda_mixer_apply_vol();

    g_hda.initialised = 1;
    kprintf("[HDA] ready -- %u codec(s)\n", g_hda.codec_count);
    return HDA_OK;
}

static hda_stream_t *hda_alloc_stream(int is_output)
{
    uint8_t start = is_output ? g_hda.num_iss : 0u;
    uint8_t end   = is_output ? (g_hda.num_iss + g_hda.num_oss) : g_hda.num_iss;

    for (uint8_t i = start; i < end; i++) {
        hda_stream_t *s = &g_hda.streams[i];
        if (!s->running && !s->dma_buf) {
            HDA_MEMSET(s, 0, sizeof(*s));
            s->idx       = i;
            s->strm_num  = (uint8_t)(i + 1u);
            s->is_output = is_output ? 1u : 0u;
            return s;
        }
    }
    return NULL;
}

hda_stream_t *hda_open_output(const hda_pcm_format_t *fmt,
                               void (*period_cb)(hda_stream_t *, void *),
                               void *priv)
{
    if (!g_hda.initialised || !fmt) return NULL;
    hda_stream_t *s = hda_alloc_stream(1);
    if (!s) return NULL;
    uint16_t hw_fmt;
    if (hda_calc_fmt(fmt, &hw_fmt) != HDA_OK) return NULL;
    HDA_MEMCPY(&s->fmt, fmt, sizeof(hda_pcm_format_t));
    s->period_cb = period_cb;
    s->cb_priv   = priv;
    hda_sd_reset(s);
    hda_build_bdl(s);
    hda_sd_program(s, hw_fmt);
    return s;
}

hda_stream_t *hda_open_input(const hda_pcm_format_t *fmt,
                              void (*period_cb)(hda_stream_t *, void *),
                              void *priv)
{
    if (!g_hda.initialised || !fmt) return NULL;
    hda_stream_t *s = hda_alloc_stream(0);
    if (!s) return NULL;
    uint16_t hw_fmt;
    if (hda_calc_fmt(fmt, &hw_fmt) != HDA_OK) return NULL;
    HDA_MEMCPY(&s->fmt, fmt, sizeof(hda_pcm_format_t));
    s->period_cb = period_cb;
    s->cb_priv   = priv;
    hda_sd_reset(s);
    hda_build_bdl(s);
    hda_sd_program(s, hw_fmt);
    return s;
}

hda_status_t hda_stream_start(hda_stream_t *s)
{
    if (!s) return HDA_ERR_INVAL;
    if (s->running) return HDA_ERR_BUSY;
    uint32_t off = hda_sd_off(s->idx);
    hda_wr32(g_hda.mmio, off + HDA_SD_CTL,
             hda_rd32(g_hda.mmio, off + HDA_SD_CTL) | HDA_SDCTL_RUN);
    s->running = 1;
    return HDA_OK;
}

hda_status_t hda_stream_stop(hda_stream_t *s)
{
    if (!s) return HDA_ERR_INVAL;
    uint32_t off = hda_sd_off(s->idx);
    hda_wr32(g_hda.mmio, off + HDA_SD_CTL,
             hda_rd32(g_hda.mmio, off + HDA_SD_CTL) & ~(uint32_t)HDA_SDCTL_RUN);
    s->running = s->write_pos = s->read_pos = 0;
    return HDA_OK;
}

hda_status_t hda_stream_close(hda_stream_t *s)
{
    if (!s) return HDA_ERR_INVAL;
    if (s->running) hda_stream_stop(s);
    hda_sd_reset(s);
    HDA_MEMSET(s, 0, sizeof(*s));
    return HDA_OK;
}

uint32_t hda_stream_write(hda_stream_t *s, const void *data, uint32_t len)
{
    if (!s || !data || !len || !s->dma_buf) return 0;
    uint32_t avail = hda_stream_avail(s);
    if (len > avail) len = avail;
    if (!len) return 0;
    uint32_t wp = s->write_pos, sz = s->buf_size, tail = sz - wp;
    if (len <= tail) {
        HDA_MEMCPY(s->dma_buf + wp, data, len);
    } else {
        HDA_MEMCPY(s->dma_buf + wp, data, tail);
        HDA_MEMCPY(s->dma_buf, (const uint8_t *)data + tail, len - tail);
    }
    s->write_pos = (wp + len) % sz;
    return len;
}

uint32_t hda_stream_avail(const hda_stream_t *s)
{
    if (!s || !s->dma_buf) return 0;
    uint32_t wp = s->write_pos, rp = s->read_pos, sz = s->buf_size;
    return (wp >= rp) ? sz - (wp - rp) - 1u : rp - wp - 1u;
}

uint32_t hda_stream_lpib(const hda_stream_t *s)
{
    if (!s) return 0;
    return hda_rd32(g_hda.mmio, hda_sd_off(s->idx) + HDA_SD_LPIB);
}

uintptr_t    hda_mmio(void)            { return g_hda.mmio;         }
uint8_t      hda_codec_count(void)     { return g_hda.codec_count;  }
void         hda_codec_count_inc(void) { g_hda.codec_count++;       }
uint8_t      hda_num_iss(void)         { return g_hda.num_iss;      }
uint8_t      hda_num_oss(void)         { return g_hda.num_oss;      }
uint8_t      hda_master_vol(void)      { return g_hda.master_vol;   }
uint8_t      hda_master_muted(void)    { return g_hda.master_muted; }

hda_codec_t *hda_codec_ptr(uint8_t i)
{
    return (i < HDA_MAX_CODECS) ? &g_hda.codecs[i] : NULL;
}

hda_mixer_channel_t *hda_mixer_ch(uint8_t i)
{
    return (i < HDA_MIXER_CHANNELS) ? &g_hda.mixer[i] : NULL;
}

const hda_codec_t *hda_get_codec(uint8_t idx)
{
    if (idx >= g_hda.codec_count) return NULL;
    return &g_hda.codecs[idx];
}

hda_status_t hda_set_master_vol(uint8_t pct)
{
    if (!g_hda.initialised) return HDA_ERR_INVAL;
    if (pct > 100u) pct = 100u;
    g_hda.master_vol = pct;
    hda_mixer_apply_vol();
    return HDA_OK;
}

hda_status_t hda_set_master_mute(int mute)
{
    if (!g_hda.initialised) return HDA_ERR_INVAL;
    g_hda.master_muted = mute ? 1u : 0u;
    hda_mixer_apply_vol();
    return HDA_OK;
}

hda_status_t hda_set_channel_vol(uint8_t ch, uint8_t left, uint8_t right)
{
    if (ch >= HDA_MIXER_CHANNELS) return HDA_ERR_INVAL;
    if (left  > 100u) left  = 100u;
    if (right > 100u) right = 100u;
    g_hda.mixer[ch].vol_left  = (int8_t)left;
    g_hda.mixer[ch].vol_right = (int8_t)right;
    hda_mixer_apply_vol();
    return HDA_OK;
}

hda_status_t hda_set_eq_band(uint8_t band, uint32_t freq_hz,
                              int32_t gain_db_x10, uint32_t q_x10)
{
    if (band >= HDA_MAX_EQ_BANDS) return HDA_ERR_INVAL;
    g_hda.eq[band].freq_hz     = freq_hz;
    g_hda.eq[band].gain_db_x10 = gain_db_x10;
    g_hda.eq[band].q_x10       = q_x10;
    g_hda.eq[band].enabled     = 1u;
    return HDA_OK;
}

hda_status_t hda_eq_enable(int enable)
{
    g_hda.eq_enabled = enable ? 1u : 0u;
    return HDA_OK;
}

const char *hda_strerror(hda_status_t st)
{
    switch (st) {
    case HDA_OK:               return "success";
    case HDA_ERR_NOT_FOUND:    return "HDA controller not found";
    case HDA_ERR_RESET:        return "controller reset timed out";
    case HDA_ERR_NO_CODECS:    return "no codecs found";
    case HDA_ERR_VERB_TIMEOUT: return "verb response timed out";
    case HDA_ERR_INVAL:        return "invalid argument";
    case HDA_ERR_BUSY:         return "stream already running";
    case HDA_ERR_NO_PATH:      return "no audio path available";
    case HDA_ERR_FMT:          return "unsupported PCM format";
    case HDA_ERR_DMA:          return "DMA setup failure";
    case HDA_ERR_NOMEM:        return "allocation failure";
    default:                   return "unknown error";
    }
}

#ifndef NDEBUG
void hda_debug_dump(void)
{
    kprintf("[HDA] mmio=0x%lx codecs=%u oss=%u iss=%u bss=%u vol=%u muted=%u\n",
            (unsigned long)g_hda.mmio, g_hda.codec_count,
            g_hda.num_oss, g_hda.num_iss, g_hda.num_bss,
            g_hda.master_vol, g_hda.master_muted);
    uint32_t n = g_hda.num_iss + g_hda.num_oss + g_hda.num_bss;
    for (uint32_t i = 0; i < n && i < HDA_MAX_STREAMS; i++) {
        const hda_stream_t *s = &g_hda.streams[i];
        if (!s->dma_buf) continue;
        kprintf("  sd[%u] tag=%u out=%u run=%u wp=%u rp=%u\n",
                s->idx, s->strm_num, s->is_output,
                s->running, s->write_pos, s->read_pos);
    }
}
#endif
