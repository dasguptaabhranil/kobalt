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

#ifndef __KERNEL__
#define __KERNEL__
#endif

#include <kernel.h>
#include "hda.h"

extern uint8_t            hda_codec_count(void);
extern hda_codec_t       *hda_codec_ptr(uint8_t i);
extern hda_status_t       hda_verb_exec(uint8_t codec, uint8_t nid,
                                         uint32_t verb_id, uint32_t payload,
                                         uint32_t *resp_out);
extern uint8_t            hda_master_vol(void);
extern uint8_t            hda_master_muted(void);
extern hda_mixer_channel_t *hda_mixer_ch(uint8_t i);
extern int                kprintf(const char *fmt, ...);

static inline int32_t q15_mul(int32_t a, int32_t b)
{
    return (int32_t)(((int64_t)a * b) >> 15);
}

static inline int16_t clamp16(int32_t x)
{
    if (x >  32767) return  32767;
    if (x < -32768) return -32768;
    return (int16_t)x;
}

static uint32_t isqrt(uint32_t n)
{
    if (n == 0u) return 0u;
    uint32_t x = n;
    uint32_t y = (x + 1u) >> 1;
    while (y < x) {
        x = y;
        y = (x + n / x) >> 1;
    }
    return x;
}

static uint8_t pct_to_step(uint32_t amp_cap, uint8_t pct)
{
    const uint32_t num_steps = (amp_cap >> HDA_AMPCAP_NUM_STEPS_SHIFT) & 0x7Fu;
    if (num_steps == 0u) return 0u;
    if (pct == 0u)       return 0u;
    if (pct >= 100u)     return (uint8_t)num_steps;

    const uint32_t step = ((uint32_t)pct * num_steps + 50u) / 100u;
    return (step > num_steps) ? (uint8_t)num_steps : (uint8_t)step;
}

static void hda_set_widget_amp(uint8_t codec_addr, uint8_t nid,
                                uint32_t amp_cap,
                                uint8_t vol_left_pct, uint8_t vol_right_pct,
                                int mute)
{
    if (!(amp_cap & (HDA_AMPCAP_NUM_STEPS_MASK | HDA_AMPCAP_MUTE)))
        return;

    const uint8_t step_l = pct_to_step(amp_cap, vol_left_pct);
    const uint8_t step_r = pct_to_step(amp_cap, vol_right_pct);
    const uint8_t mute_bit = mute ? HDA_AMP_SET_MUTE : 0u;

    const uint16_t payload_l =
        (uint16_t)(HDA_AMP_SET_OUTPUT | HDA_AMP_SET_LEFT |
                   mute_bit | (step_l & HDA_AMP_SET_GAIN_MASK));
    hda_verb_exec(codec_addr, nid,
                  HDA_VERB4(codec_addr, nid, HDA_VERB4_SET_AMP_GAIN, payload_l),
                  0, NULL);

    const uint16_t payload_r =
        (uint16_t)(HDA_AMP_SET_OUTPUT | HDA_AMP_SET_RIGHT |
                   mute_bit | (step_r & HDA_AMP_SET_GAIN_MASK));
    hda_verb_exec(codec_addr, nid,
                  HDA_VERB4(codec_addr, nid, HDA_VERB4_SET_AMP_GAIN, payload_r),
                  0, NULL);
}

static int32_t sin_q15(uint32_t angle_units)
{

    if (angle_units == 0u)     return 0;
    if (angle_units >= 32768u) return 32767;

    const int32_t pi_q15 = 102943;
    const int32_t theta  = (int32_t)(((int64_t)angle_units * (pi_q15 >> 1)) >> 15);
    const int32_t numer  = (int32_t)(((int64_t)4 * theta * (pi_q15 - theta)) >> 15);
    const int32_t denom  = (int32_t)(((int64_t)pi_q15 * pi_q15) >> 15);
    if (denom == 0) return 0;
    return (int32_t)(((int64_t)numer << 15) / denom);
}

static int32_t cos_q15(uint32_t angle_units)
{
    if (angle_units >= 32768u) return 0;
    return sin_q15(32768u - angle_units);
}

typedef struct {
    int16_t b0, b1, b2;
    int16_t a1, a2;
} biquad_coeff_t;

static void biquad_peaking_eq(uint32_t freq_hz, int32_t gain_q10,
                               uint32_t q_x10, uint32_t sample_rate_hz,
                               biquad_coeff_t *out)
{
    if (!out || sample_rate_hz == 0u) return;

    const uint32_t w0_units =
        (uint32_t)(((uint64_t)freq_hz * 65536u) / sample_rate_hz);

    const uint32_t half = w0_units >> 1;
    const int32_t  sh   = sin_q15(half > 32768u ? 32768u : half);
    const int32_t  ch   = cos_q15(half > 32768u ? 32768u : half);

    const int32_t  sw0  = clamp16(2 * q15_mul(sh, ch));

    const int32_t  cw0  = clamp16((int32_t)(32768 - 2 * q15_mul(sh, sh)));

    const int32_t  alpha = (q_x10 > 0u)
                         ? (int32_t)((int64_t)sw0 * 10 / (int32_t)(2u * q_x10))
                         : sw0;

    const int32_t  A2_q15 = (int32_t)(32768u +
                             (uint32_t)((int64_t)gain_q10 * 32768 / 200));
    const int32_t  A_q15  = clamp16((int32_t)isqrt((uint32_t)A2_q15 * 32768u));

    const int32_t alpha_A  = clamp16(q15_mul(alpha, A_q15));
    const int32_t alpha_dA = (A_q15 > 0)
                           ? clamp16((int32_t)(((int64_t)alpha << 15) / A_q15))
                           : alpha;

    const int32_t b0 = clamp16(32768 + alpha_A);
    const int32_t b1 = clamp16(-2 * cw0);
    const int32_t b2 = clamp16(32768 - alpha_A);
    const int32_t a0 = clamp16(32768 + alpha_dA);

    if (a0 == 0) return;
    out->b0 = clamp16((int32_t)(((int64_t)b0 << 15) / a0));
    out->b1 = clamp16((int32_t)(((int64_t)b1 << 15) / a0));
    out->b2 = clamp16((int32_t)(((int64_t)b2 << 15) / a0));
    out->a1 = clamp16((int32_t)(((int64_t)(-2 * cw0) << 15) / a0));
    out->a2 = clamp16((int32_t)(((int64_t)(32768 - alpha_dA) << 15) / a0));
}

static void hda_write_proc_coeff(uint8_t codec_addr, uint8_t nid,
                                  uint8_t index, uint16_t value)
{

    hda_verb_exec(codec_addr, nid, 0x500u, index, NULL);
    hda_verb_exec(codec_addr, nid, 0x400u, (uint32_t)value, NULL);
}

static void hda_apply_eq_to_codec(const hda_codec_t *c,
                                   const hda_eq_band_t *bands,
                                   uint8_t band_count, uint8_t enabled)
{

    uint8_t proc_nid = 0u;
    for (uint8_t n = c->node_start; n < c->node_start + c->node_count; n++) {
        const hda_widget_t *w = &c->widgets[n];
        if (w->valid && (w->wcap & HDA_WCAP_PROC_WIDGET)) {
            proc_nid = n;
            break;
        }
    }
    if (proc_nid == 0u) return;

    if (!enabled) {

        for (uint8_t b = 0; b < band_count; b++) {
            const uint8_t base = (uint8_t)(b * 5u);
            hda_write_proc_coeff(c->addr, proc_nid, base + 0u, 0x7FFFu);
            hda_write_proc_coeff(c->addr, proc_nid, base + 1u, 0x0000u);
            hda_write_proc_coeff(c->addr, proc_nid, base + 2u, 0x0000u);
            hda_write_proc_coeff(c->addr, proc_nid, base + 3u, 0x0000u);
            hda_write_proc_coeff(c->addr, proc_nid, base + 4u, 0x0000u);
        }
        return;
    }

    for (uint8_t b = 0; b < band_count; b++) {
        if (!bands[b].enabled || bands[b].freq_hz == 0u) continue;

        biquad_coeff_t coeff;

        biquad_peaking_eq(bands[b].freq_hz, bands[b].gain_db_x10,
                          bands[b].q_x10, 48000u, &coeff);

        const uint8_t base = (uint8_t)(b * 5u);
        hda_write_proc_coeff(c->addr, proc_nid, base + 0u, (uint16_t)coeff.b0);
        hda_write_proc_coeff(c->addr, proc_nid, base + 1u, (uint16_t)coeff.b1);
        hda_write_proc_coeff(c->addr, proc_nid, base + 2u, (uint16_t)coeff.b2);
        hda_write_proc_coeff(c->addr, proc_nid, base + 3u, (uint16_t)coeff.a1);
        hda_write_proc_coeff(c->addr, proc_nid, base + 4u, (uint16_t)coeff.a2);
    }
}

void hda_mixer_apply_eq(const hda_eq_band_t *bands,
                         uint8_t band_count, uint8_t enabled)
{
    if (!bands || band_count == 0u) return;
    if (band_count > HDA_MAX_EQ_BANDS)
        band_count = HDA_MAX_EQ_BANDS;

    for (uint8_t ci = 0; ci < hda_codec_count(); ci++) {
        const hda_codec_t *c = hda_codec_ptr(ci);
        if (!c || !c->valid) continue;
        hda_apply_eq_to_codec(c, bands, band_count, enabled);
    }
}

void hda_mixer_apply_vol(void)
{
    const uint8_t master_vol   = hda_master_vol();
    const uint8_t master_muted = hda_master_muted();

    for (uint8_t ci = 0; ci < hda_codec_count(); ci++) {
        const hda_codec_t *c = hda_codec_ptr(ci);
        if (!c || !c->valid) continue;

        for (uint8_t pi = 0; pi < c->path_count; pi++) {
            const hda_audio_path_t *ap = &c->paths[pi];
            if (!ap->active) continue;

            uint8_t vol_l = master_vol;
            uint8_t vol_r = master_vol;
            int     muted = master_muted;

            const hda_mixer_channel_t *ch = hda_mixer_ch(pi);
            if (ch && ch->active) {
                if (ch->muted) {
                    muted = 1;
                } else {

                    vol_l = (uint8_t)((uint32_t)master_vol *
                                      (uint32_t)(uint8_t)ch->vol_left / 100u);
                    vol_r = (uint8_t)((uint32_t)master_vol *
                                      (uint32_t)(uint8_t)ch->vol_right / 100u);
                }
            }

            for (uint8_t wi = 0; wi < ap->path_len; wi++) {
                const uint8_t      nid = ap->path[wi];
                const hda_widget_t *w  = &c->widgets[nid];
                if (!w->valid || !(w->wcap & HDA_WCAP_OUT_AMP)) continue;

                hda_set_widget_amp(c->addr, nid, w->amp_cap_out,
                                   vol_l, vol_r, muted);
            }
        }
    }
}

void hda_mixer_ramp_vol(uint8_t from_pct, uint8_t to_pct)
{
    extern hda_status_t hda_set_master_vol(uint8_t);
    if (from_pct == to_pct) return;

    int8_t  step = (to_pct > from_pct) ? 5 : -5;
    uint8_t cur  = from_pct;

    while ((step > 0 && cur < to_pct) || (step < 0 && cur > to_pct)) {
        const int32_t next = (int32_t)cur + step;
        if (step > 0 && next > (int32_t)to_pct)  cur = to_pct;
        else if (step < 0 && next < (int32_t)to_pct) cur = to_pct;
        else cur = (uint8_t)next;

        hda_set_master_vol(cur);

        for (volatile uint32_t i = 0; i < 1000000u; i++)
            __asm__ volatile("nop");
    }
}

static const char *const g_ch_names[HDA_MIXER_CHANNELS] = {
    "Master",
    "Headphone",
    "Speaker",
    "Line-Out",
    "AUX",
    "SPDIF",
    "HDMI",
    "Mono",
};

void hda_mixer_init_channels(void)
{
    for (uint8_t i = 0; i < HDA_MIXER_CHANNELS; i++) {
        hda_mixer_channel_t *ch = hda_mixer_ch(i);
        if (!ch) continue;
        ch->vol_left  = 100;
        ch->vol_right = 100;
        ch->muted     = 0;
        ch->active    = 1;

        const char *name = g_ch_names[i];
        uint8_t     k    = 0;
        while (name[k] && k < 31u) { ch->name[k] = name[k]; k++; }
        ch->name[k] = '\0';
    }
}

void hda_mixer_jack_event(uint8_t codec_idx, uint8_t pin_nid)
{
    const hda_codec_t *c = hda_codec_ptr(codec_idx);
    if (!c || !c->valid) return;

    uint32_t sense;
    if (hda_verb_exec(c->addr, pin_nid,
                      HDA_VERB_GET_PIN_SENSE, 0, &sense) != HDA_OK)
        return;

    const int hp_present = (sense >> 31) & 1;
    kprintf("[HDA] Codec %u pin %u: %s\n",
            codec_idx, pin_nid,
            hp_present ? "headphone inserted" : "headphone removed");

    for (uint8_t pi = 0; pi < c->path_count; pi++) {
        const hda_audio_path_t *ap = &c->paths[pi];
        if (!ap->active) continue;

        hda_mixer_channel_t *ch = hda_mixer_ch(pi);
        if (!ch) continue;

        if (ap->is_headphone) {
            ch->muted = hp_present ? 0 : 1;
        } else if (ap->is_speaker) {
            ch->muted = hp_present ? 1 : 0;
        }
    }

    hda_mixer_apply_vol();
}

#ifndef NDEBUG
void hda_mixer_debug_dump(void)
{
    kprintf("[HDA MIXER] master=%u%% muted=%u\n",
            hda_master_vol(), hda_master_muted());
    for (uint8_t i = 0; i < HDA_MIXER_CHANNELS; i++) {
        const hda_mixer_channel_t *ch = hda_mixer_ch(i);
        if (!ch || !ch->active) continue;
        kprintf("  ch[%u] %-12s L=%d R=%d mute=%u\n",
                i, ch->name, (int)ch->vol_left, (int)ch->vol_right, ch->muted);
    }
}
#endif
