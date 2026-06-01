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
#include "../inc/sound.h"
#include "../inc/devfs.h"
#include "intel/hda.h"
#include "intel/hda_mixer.h"

struct sound_stream {
    hda_stream_t *hda;
    sound_pcm_format_t fmt;
    int is_input;
    void (*user_cb)(sound_stream_t *, void *);
    void  *user_priv;
    sound_stream_t *self;
};

#define SOUND_MAX_STREAMS  (HDA_MAX_OUTPUT_STREAMS + HDA_MAX_INPUT_STREAMS)
static sound_stream_t g_streams[SOUND_MAX_STREAMS];

static int     g_sound_up = 0;
static uint8_t g_vol      = 80u;
static int     g_muted    = 0;
static hda_eq_band_t g_eq[HDA_MAX_EQ_BANDS];
static uint8_t g_eq_enabled = 0;

static const hda_eq_band_t g_preset_flat[HDA_MAX_EQ_BANDS] = {
    { 32, 0, 10, 1 }, { 64, 0, 10, 1 }, { 125, 0, 10, 1 },
    { 250, 0, 10, 1 }, { 500, 0, 10, 1 }, { 1000, 0, 10, 1 },
    { 2000, 0, 10, 1 }, { 4000, 0, 10, 1 }, { 8000, 0, 10, 1 },
    { 16000, 0, 10, 1 },
};

static const hda_eq_band_t g_preset_bass_boost[HDA_MAX_EQ_BANDS] = {
    { 32,  60, 7, 1 }, { 64,  60, 7, 1 }, { 125,  40, 8, 1 },
    { 250,  20, 9, 1 }, { 500,   0, 10, 1 }, { 1000,   0, 10, 1 },
    { 2000,   0, 10, 1 }, { 4000,   0, 10, 1 }, { 8000,   0, 10, 1 },
    { 16000,   0, 10, 1 },
};

static const hda_eq_band_t g_preset_treble[HDA_MAX_EQ_BANDS] = {
    { 32,  0, 10, 1 }, { 64,  0, 10, 1 }, { 125,  0, 10, 1 },
    { 250,  0, 10, 1 }, { 500,  0, 10, 1 }, { 1000,  0, 10, 1 },
    { 2000, 10, 10, 1 }, { 4000, 20, 10, 1 }, { 8000, 35, 8, 1 },
    { 16000, 40, 7, 1 },
};

static const hda_eq_band_t g_preset_vocal[HDA_MAX_EQ_BANDS] = {
    { 32,  0, 10, 1 }, { 64,  0, 10, 1 }, { 125,  0, 10, 1 },
    { 250,  5, 10, 1 }, { 500, 10, 10, 1 }, { 1000, 15, 8, 1 },
    { 2000, 30, 7, 1 }, { 4000, 25, 8, 1 }, { 8000, 10, 10, 1 },
    { 16000,  0, 10, 1 },
};

static const hda_eq_band_t g_preset_loudness[HDA_MAX_EQ_BANDS] = {
    { 32,  60, 7, 1 }, { 64,  40, 8, 1 }, { 125,  20, 9, 1 },
    { 250,   0, 10, 1 }, { 500, -10, 10, 1 }, { 1000, -10, 10, 1 },
    { 2000,  -5, 10, 1 }, { 4000,   0, 10, 1 }, { 8000,  20, 9, 1 },
    { 16000,  40, 7, 1 },
};

static const hda_eq_band_t g_preset_headphone[HDA_MAX_EQ_BANDS] = {
    { 32,  30, 8, 1 }, { 64,  20, 9, 1 }, { 125,  10, 10, 1 },
    { 250,   5, 10, 1 }, { 500,   0, 10, 1 }, { 1000,  -5, 10, 1 },
    { 2000,  -3, 10, 1 }, { 4000,   0, 10, 1 }, { 8000,  15, 9, 1 },
    { 16000,  25, 8, 1 },
};

static const hda_eq_band_t g_preset_speaker[HDA_MAX_EQ_BANDS] = {
    { 32, -20, 8, 1 }, { 64,  -5, 9, 1 }, { 125,   5, 10, 1 },
    { 250,  10, 10, 1 }, { 500,   5, 10, 1 }, { 1000,   0, 10, 1 },
    { 2000,  -5, 10, 1 }, { 4000,  -3, 10, 1 }, { 8000,   5, 10, 1 },
    { 16000,  10, 9, 1 },
};

static void sound_period_thunk(hda_stream_t *hda, void *priv)
{
    sound_stream_t *s = (sound_stream_t *)priv;
    if (s && s->user_cb)
        s->user_cb(s, s->user_priv);
    (void)hda;
}

#define SNDCTL_DSP_SYNC      0x5001UL
#define SNDCTL_DSP_SPEED     0x5002UL
#define SNDCTL_DSP_SETFMT    0x5005UL
#define SNDCTL_DSP_CHANNELS  0x5006UL
#define SNDCTL_DSP_GETBLKSIZE 0x5004UL
#define SNDCTL_DSP_GETFMTS   0x5003UL
#define SNDCTL_DSP_SETFRAGMENT 0x5010UL

#define AFMT_S16_LE  0x0010U
#define AFMT_U8      0x0008U
#define AFMT_S8      0x0040U

typedef struct {
    sound_stream_t *stream;
    sound_pcm_format_t fmt;
    int  stream_open;
} dsp_state_t;

static dsp_state_t s_dsp0 = {
    .stream     = NULL,
    .fmt        = { .sample_rate = 44100, .channels = 2, .bits = 16, .is_float = 0 },
    .stream_open = 0,
};

static int dsp_open(void *priv, int flags)
{
    dsp_state_t *st = (dsp_state_t *)priv;
    (void)flags;

    if (!g_sound_up) return -1;
    if (st->stream_open) return -1;

    st->stream = sound_open_output(&st->fmt, NULL, NULL);
    if (!st->stream) return -1;

    st->stream_open = 1;
    return 0;
}

static void dsp_close(void *priv)
{
    dsp_state_t *st = (dsp_state_t *)priv;
    if (st->stream) {
        sound_stream_stop(st->stream);
        sound_stream_close(st->stream);
        st->stream = NULL;
    }
    st->stream_open = 0;
}

static ssize_t dsp_read(void *priv, void *buf, size_t n, uint64_t *pos)
{
    dsp_state_t *st = (dsp_state_t *)priv;
    (void)pos;
    if (!st->stream_open) return -1;
    uint32_t got = sound_stream_read(st->stream, buf, (uint32_t)n);
    return (ssize_t)got;
}

static ssize_t dsp_write(void *priv, const void *buf, size_t n, uint64_t *pos)
{
    dsp_state_t *st = (dsp_state_t *)priv;
    (void)pos;
    if (!st->stream_open) return -1;

    sound_stream_start(st->stream);

    uint32_t written = sound_stream_write(st->stream, buf, (uint32_t)n);
    return (ssize_t)written;
}

static int dsp_ioctl(void *priv, unsigned long cmd, void *arg)
{
    dsp_state_t *st = (dsp_state_t *)priv;

    switch (cmd) {
    case SNDCTL_DSP_SYNC:
        return 0;

    case SNDCTL_DSP_SPEED:
        if (!arg) return -1;
        {
            uint32_t rate = *(uint32_t *)arg;
            if (rate < 8000u || rate > 192000u) return -1;
            st->fmt.sample_rate = rate;
            *(uint32_t *)arg = rate;
        }
        return 0;

    case SNDCTL_DSP_CHANNELS:
        if (!arg) return -1;
        {
            uint32_t ch = *(uint32_t *)arg;
            if (ch < 1u || ch > 8u) return -1;
            st->fmt.channels = (uint8_t)ch;
            *(uint32_t *)arg = ch;
        }
        return 0;

    case SNDCTL_DSP_SETFMT:
        if (!arg) return -1;
        {
            uint32_t fmt = *(uint32_t *)arg;
            if (fmt == AFMT_S16_LE) {
                st->fmt.bits = 16;
                st->fmt.is_float = 0;
            } else if (fmt == AFMT_U8 || fmt == AFMT_S8) {
                st->fmt.bits = 8;
                st->fmt.is_float = 0;
            } else {
                st->fmt.bits = 16;
                *(uint32_t *)arg = AFMT_S16_LE;
                return -1;
            }
            *(uint32_t *)arg = fmt;
        }
        return 0;

    case SNDCTL_DSP_GETFMTS:
        if (!arg) return -1;
        *(uint32_t *)arg = AFMT_S16_LE | AFMT_U8;
        return 0;

    case SNDCTL_DSP_GETBLKSIZE:
        if (!arg) return -1;
        *(uint32_t *)arg = 4096u;
        return 0;

    case SNDCTL_DSP_SETFRAGMENT:
        return 0;

    default:
        return -1;
    }
}

static devfs_ops_t s_dsp_ops = {
    .open  = dsp_open,
    .close = dsp_close,
    .read  = dsp_read,
    .write = dsp_write,
    .ioctl = dsp_ioctl,
    .poll  = NULL,
};

#define SOUND_MIXER_DEVMASK      0x00u
#define SOUND_MIXER_VOLUME       0x00u
#define SOUND_MIXER_MUTE         0xFFu

#define OSS_VOL_PACK(l, r)  (((uint32_t)(r) << 8) | (uint32_t)(l))
#define OSS_VOL_LEFT(v)     ((uint8_t)((v) & 0xFFu))
#define OSS_VOL_RIGHT(v)    ((uint8_t)(((v) >> 8) & 0xFFu))

#define MIXER_READ(ch)  (0x80000000UL | (unsigned long)(ch))
#define MIXER_WRITE(ch) (0xC0000000UL | (unsigned long)(ch))

static int mixer_ioctl(void *priv, unsigned long cmd, void *arg)
{
    (void)priv;
    if (!arg) return -1;

    uint32_t *val = (uint32_t *)arg;
    uint8_t  ch   = (uint8_t)(cmd & 0xFFu);
    int      dir  = (int)(cmd >> 30);

    if (ch == SOUND_MIXER_MUTE) {
        if (dir == 2) {
            *val = (uint32_t)g_muted;
        } else {
            sound_set_mute((int)(*val));
        }
        return 0;
    }

    if (ch == SOUND_MIXER_VOLUME) {
        if (dir == 2) {
            *val = OSS_VOL_PACK(g_vol, g_vol);
        } else {
            uint8_t lv = OSS_VOL_LEFT(*val);
            uint8_t rv = OSS_VOL_RIGHT(*val);
            uint8_t mv = lv > rv ? lv : rv;
            if (mv > 100u) mv = 100u;
            sound_set_volume(mv);
        }
        return 0;
    }

    return -1;
}

static devfs_ops_t s_mixer_ops = {
    .open  = NULL,
    .close = NULL,
    .read  = NULL,
    .write = NULL,
    .ioctl = mixer_ioctl,
    .poll  = NULL,
};

sound_status_t sound_init(void)
{
    __builtin_memset(g_streams, 0, sizeof(g_streams));
    __builtin_memset(g_eq,      0, sizeof(g_eq));
    __builtin_memcpy(g_eq, g_preset_flat, sizeof(g_preset_flat));

    pci_device_t *pdev = NULL;
    uint32_t np = pci_count();
    for (uint32_t i = 0; i < np; i++) {
        pci_device_t *d = pci_get_device(i);
        if (d && d->class_code == HDA_PCI_CLASS && d->subclass == HDA_PCI_SUBCLASS) {
            pdev = d;
            break;
        }
    }
    if (!pdev) return SOUND_ERR_NOT_FOUND;

    hda_status_t rc = hda_init(pdev);
    if (rc == HDA_ERR_NOT_FOUND) return SOUND_ERR_NOT_FOUND;
    if (rc != HDA_OK)            return SOUND_ERR_TIMEOUT;

    hda_mixer_init_channels();

    g_vol      = 80u;
    g_muted    = 0;
    g_sound_up = 1;

    {
        int rc_dsp = devfs_register_cdev(DEVFS_MAJOR_SND, 0u, "dsp0",
                                         DEVFS_CLASS_SOUND,
                                         &s_dsp_ops, &s_dsp0);
        if (rc_dsp < 0)
            klog_warn("sound", "devfs: failed to create /dev/dsp0");
        else
            klog_ok("sound", "/dev/dsp0 registered");

        int rc_mix = devfs_register_cdev(DEVFS_MAJOR_SND, 1u, "mixer0",
                                         DEVFS_CLASS_SOUND,
                                         &s_mixer_ops, NULL);
        if (rc_mix < 0)
            klog_warn("sound", "devfs: failed to create /dev/mixer0");
        else
            klog_ok("sound", "/dev/mixer0 registered");
    }

    return SOUND_OK;
}

void sound_shutdown(void)
{
    if (!g_sound_up) return;

    hda_mixer_ramp_vol(g_vol, 0u);
    hda_set_master_mute(1);

    for (uint8_t i = 0; i < SOUND_MAX_STREAMS; i++) {
        if (g_streams[i].hda && g_streams[i].hda->running)
            hda_stream_stop(g_streams[i].hda);
    }

    g_sound_up = 0;
}

int sound_available(void)
{
    return g_sound_up;
}

sound_status_t sound_get_info(sound_device_info_t *out)
{
    if (!out)          return SOUND_ERR_INVAL;
    if (!g_sound_up)   return SOUND_ERR_NOT_INIT;

    __builtin_memset(out, 0, sizeof(*out));

    const char *name = "Intel HDA";
    uint8_t k = 0;
    while (name[k] && k < 31u) { out->driver_name[k] = name[k]; k++; }

    const hda_codec_t *c = hda_get_codec(0);
    if (c) {
        out->vendor_id = c->vendor_id;
        out->device_id = c->device_id;
    }
    out->codec_count    = hda_codec_count();
    out->output_streams = HDA_MAX_OUTPUT_STREAMS;
    out->input_streams  = HDA_MAX_INPUT_STREAMS;
    out->has_eq         = 1u;
    out->has_jack_detect = 1u;
    return SOUND_OK;
}

static sound_stream_t *alloc_stream_wrapper(void)
{
    for (uint8_t i = 0; i < SOUND_MAX_STREAMS; i++) {
        if (!g_streams[i].hda) {
            __builtin_memset(&g_streams[i], 0, sizeof(g_streams[i]));
            g_streams[i].self = &g_streams[i];
            return &g_streams[i];
        }
    }
    return NULL;
}

sound_stream_t *sound_open_output(const sound_pcm_format_t *fmt,
                                   void (*period_cb)(sound_stream_t *, void *),
                                   void *priv)
{
    if (!g_sound_up || !fmt) return NULL;

    sound_stream_t *s = alloc_stream_wrapper();
    if (!s) return NULL;

    hda_pcm_format_t hfmt = {
        .sample_rate = fmt->sample_rate,
        .channels    = fmt->channels,
        .bits        = fmt->bits,
        .is_float    = fmt->is_float,
    };

    s->user_cb   = period_cb;
    s->user_priv = priv;
    s->is_input  = 0;
    __builtin_memcpy(&s->fmt, fmt, sizeof(*fmt));

    s->hda = hda_open_output(&hfmt, sound_period_thunk, s);
    if (!s->hda) {
        __builtin_memset(s, 0, sizeof(*s));
        return NULL;
    }
    return s;
}

sound_stream_t *sound_open_input(const sound_pcm_format_t *fmt,
                                  void (*period_cb)(sound_stream_t *, void *),
                                  void *priv)
{
    if (!g_sound_up || !fmt) return NULL;

    sound_stream_t *s = alloc_stream_wrapper();
    if (!s) return NULL;

    hda_pcm_format_t hfmt = {
        .sample_rate = fmt->sample_rate,
        .channels    = fmt->channels,
        .bits        = fmt->bits,
        .is_float    = fmt->is_float,
    };

    s->user_cb   = period_cb;
    s->user_priv = priv;
    s->is_input  = 1;
    __builtin_memcpy(&s->fmt, fmt, sizeof(*fmt));

    s->hda = hda_open_input(&hfmt, sound_period_thunk, s);
    if (!s->hda) {
        __builtin_memset(s, 0, sizeof(*s));
        return NULL;
    }
    return s;
}

sound_status_t sound_stream_start(sound_stream_t *s)
{
    if (!s || !s->hda) return SOUND_ERR_INVAL;
    hda_status_t rc = hda_stream_start(s->hda);
    if (rc == HDA_ERR_BUSY) return SOUND_ERR_BUSY;
    return (rc == HDA_OK) ? SOUND_OK : SOUND_ERR_DMA;
}

sound_status_t sound_stream_stop(sound_stream_t *s)
{
    if (!s || !s->hda) return SOUND_ERR_INVAL;
    return (hda_stream_stop(s->hda) == HDA_OK) ? SOUND_OK : SOUND_ERR_DMA;
}

sound_status_t sound_stream_close(sound_stream_t *s)
{
    if (!s || !s->hda) return SOUND_ERR_INVAL;
    hda_stream_close(s->hda);
    __builtin_memset(s, 0, sizeof(*s));
    return SOUND_OK;
}

uint32_t sound_stream_write(sound_stream_t *s, const void *data, uint32_t len)
{
    if (!s || !s->hda) return 0;
    return hda_stream_write(s->hda, data, len);
}

uint32_t sound_stream_read(sound_stream_t *s, void *buf, uint32_t len)
{
    (void)s; (void)buf; (void)len;
    return 0;
}

uint32_t sound_stream_avail(const sound_stream_t *s)
{
    if (!s || !s->hda) return 0;
    return hda_stream_avail(s->hda);
}

sound_status_t sound_stream_get_format(const sound_stream_t *s,
                                        sound_pcm_format_t *out)
{
    if (!s || !out) return SOUND_ERR_INVAL;
    __builtin_memcpy(out, &s->fmt, sizeof(*out));
    return SOUND_OK;
}

sound_status_t sound_set_volume(uint8_t vol_pct)
{
    if (!g_sound_up) return SOUND_ERR_NOT_INIT;
    if (vol_pct > 100u) vol_pct = 100u;
    g_vol = vol_pct;
    hda_set_master_vol(vol_pct);
    return SOUND_OK;
}

uint8_t sound_get_volume(void) { return g_vol; }

sound_status_t sound_set_mute(int mute)
{
    if (!g_sound_up) return SOUND_ERR_NOT_INIT;
    g_muted = mute ? 1 : 0;
    hda_set_master_mute(g_muted);
    return SOUND_OK;
}

int sound_is_muted(void) { return g_muted; }

sound_status_t sound_set_volume_smooth(uint8_t vol_pct)
{
    if (!g_sound_up) return SOUND_ERR_NOT_INIT;
    if (vol_pct > 100u) vol_pct = 100u;
    hda_mixer_ramp_vol(g_vol, vol_pct);
    g_vol = vol_pct;
    return SOUND_OK;
}

sound_status_t sound_set_channel_vol(uint8_t channel,
                                      uint8_t left, uint8_t right)
{
    if (!g_sound_up)            return SOUND_ERR_NOT_INIT;
    hda_status_t rc = hda_set_channel_vol(channel, left, right);
    return (rc == HDA_OK) ? SOUND_OK : SOUND_ERR_INVAL;
}

sound_status_t sound_set_channel_mute(uint8_t channel, int mute)
{
    if (!g_sound_up)                        return SOUND_ERR_NOT_INIT;
    hda_mixer_channel_t *ch = hda_mixer_ch(channel);
    if (!ch)                                return SOUND_ERR_INVAL;
    ch->muted = mute ? 1u : 0u;
    hda_mixer_apply_vol();
    return SOUND_OK;
}

const char *sound_channel_name(uint8_t idx)
{
    hda_mixer_channel_t *ch = hda_mixer_ch(idx);
    return ch ? ch->name : NULL;
}

uint8_t sound_channel_count(void) { return HDA_MIXER_CHANNELS; }

sound_status_t sound_set_eq_band(uint8_t band, const sound_eq_band_t *b)
{
    if (!g_sound_up || !b || band >= HDA_MAX_EQ_BANDS)
        return SOUND_ERR_INVAL;

    g_eq[band].freq_hz     = b->freq_hz;
    g_eq[band].gain_db_x10 = b->gain_db_x10;
    g_eq[band].q_x10       = b->q_x10;
    g_eq[band].enabled     = 1u;

    hda_set_eq_band(band, b->freq_hz, b->gain_db_x10, b->q_x10);
    if (g_eq_enabled)
        hda_eq_enable(1);

    return SOUND_OK;
}

sound_status_t sound_get_eq_band(uint8_t band, sound_eq_band_t *out)
{
    if (!out || band >= HDA_MAX_EQ_BANDS) return SOUND_ERR_INVAL;
    out->freq_hz     = g_eq[band].freq_hz;
    out->gain_db_x10 = g_eq[band].gain_db_x10;
    out->q_x10       = g_eq[band].q_x10;
    return SOUND_OK;
}

sound_status_t sound_eq_enable(int enable)
{
    if (!g_sound_up) return SOUND_ERR_NOT_INIT;
    g_eq_enabled = enable ? 1u : 0u;
    hda_eq_enable(g_eq_enabled);
    return SOUND_OK;
}

int sound_eq_is_enabled(void) { return (int)g_eq_enabled; }

sound_status_t sound_eq_reset(void)
{
    if (!g_sound_up) return SOUND_ERR_NOT_INIT;
    for (uint8_t i = 0; i < HDA_MAX_EQ_BANDS; i++) {
        g_eq[i].gain_db_x10 = 0;
        g_eq[i].enabled     = 0u;
        hda_set_eq_band(i, g_eq[i].freq_hz, 0, g_eq[i].q_x10);
    }
    return SOUND_OK;
}

sound_status_t sound_eq_apply_preset(sound_eq_preset_t preset)
{
    if (!g_sound_up) return SOUND_ERR_NOT_INIT;

    const hda_eq_band_t *bands;
    switch (preset) {
    case SOUND_EQ_FLAT:       bands = g_preset_flat;       break;
    case SOUND_EQ_BASS_BOOST: bands = g_preset_bass_boost;  break;
    case SOUND_EQ_TREBLE:     bands = g_preset_treble;      break;
    case SOUND_EQ_VOCAL:      bands = g_preset_vocal;       break;
    case SOUND_EQ_LOUDNESS:   bands = g_preset_loudness;    break;
    case SOUND_EQ_HEADPHONE:  bands = g_preset_headphone;   break;
    case SOUND_EQ_SPEAKER:    bands = g_preset_speaker;     break;
    default:                  return SOUND_ERR_INVAL;
    }

    __builtin_memcpy(g_eq, bands, sizeof(g_eq));
    for (uint8_t i = 0; i < HDA_MAX_EQ_BANDS; i++) {
        g_eq[i].enabled = 1u;
        hda_set_eq_band(i, g_eq[i].freq_hz, g_eq[i].gain_db_x10, g_eq[i].q_x10);
    }
    g_eq_enabled = (preset != SOUND_EQ_FLAT) ? 1u : 0u;
    hda_eq_enable(g_eq_enabled);
    return SOUND_OK;
}

const char *sound_strerror(sound_status_t st)
{
    switch (st) {
    case SOUND_OK:            return "success";
    case SOUND_ERR_NOT_FOUND: return "no audio hardware found";
    case SOUND_ERR_NOT_INIT:  return "sound subsystem not initialised";
    case SOUND_ERR_BUSY:      return "stream already running";
    case SOUND_ERR_FMT:       return "unsupported PCM format";
    case SOUND_ERR_INVAL:     return "invalid argument";
    case SOUND_ERR_NO_PATH:   return "no audio routing path";
    case SOUND_ERR_DMA:       return "DMA error";
    case SOUND_ERR_NOMEM:     return "out of memory";
    case SOUND_ERR_TIMEOUT:   return "hardware timeout";
    case SOUND_ERR_NOSYS:     return "not supported by hardware";
    default:                  return "unknown error";
    }
}

#ifndef NDEBUG
void sound_debug_dump(void)
{
    extern int kprintf(const char *, ...);
    kprintf("[SOUND] up=%d  vol=%u%%  muted=%d  eq=%d\n",
            g_sound_up, g_vol, g_muted, g_eq_enabled);
    if (g_sound_up) {
        hda_debug_dump();
        hda_mixer_debug_dump();
    }
}
#endif
