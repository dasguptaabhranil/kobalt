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

#ifndef KOBALT_SOUND_H
#define KOBALT_SOUND_H

#ifdef __KERNEL__
#  include <kernel.h>
#else
#  include <stdint.h>
#  include <stddef.h>
#endif

typedef enum {
    SOUND_OK              =  0,
    SOUND_ERR_NOT_FOUND   = -1,
    SOUND_ERR_NOT_INIT    = -2,
    SOUND_ERR_BUSY        = -3,
    SOUND_ERR_FMT         = -4,
    SOUND_ERR_INVAL       = -5,
    SOUND_ERR_NO_PATH     = -6,
    SOUND_ERR_DMA         = -7,
    SOUND_ERR_NOMEM       = -8,
    SOUND_ERR_TIMEOUT     = -9,
    SOUND_ERR_NOSYS       = -10,
} sound_status_t;

typedef struct {
    uint32_t  sample_rate;
    uint8_t   channels;
    uint8_t   bits;
    uint8_t   is_float;
    uint8_t   _pad;
} sound_pcm_format_t;

typedef struct sound_stream sound_stream_t;

typedef struct {
    uint32_t  freq_hz;
    int32_t   gain_db_x10;
    uint32_t  q_x10;
} sound_eq_band_t;

typedef struct {
    char      driver_name[32];
    uint16_t  vendor_id;
    uint16_t  device_id;
    uint8_t   codec_count;
    uint8_t   output_streams;
    uint8_t   input_streams;
    uint8_t   has_eq;
    uint8_t   has_jack_detect;
    uint8_t   has_spdif;
    uint8_t   has_hdmi;
    uint8_t   _pad[1];
} sound_device_info_t;

sound_status_t sound_init(void);

void sound_shutdown(void);

int sound_available(void);

sound_status_t sound_get_info(sound_device_info_t *out);

sound_stream_t *sound_open_output(const sound_pcm_format_t *fmt,
                                   void (*period_cb)(sound_stream_t *, void *),
                                   void *priv);

sound_stream_t *sound_open_input(const sound_pcm_format_t *fmt,
                                  void (*period_cb)(sound_stream_t *, void *),
                                  void *priv);

sound_status_t sound_stream_start(sound_stream_t *s);

sound_status_t sound_stream_stop(sound_stream_t *s);

sound_status_t sound_stream_close(sound_stream_t *s);

uint32_t sound_stream_write(sound_stream_t *s, const void *data, uint32_t len);

uint32_t sound_stream_read(sound_stream_t *s, void *buf, uint32_t len);

uint32_t sound_stream_avail(const sound_stream_t *s);

sound_status_t sound_stream_get_format(const sound_stream_t *s,
                                        sound_pcm_format_t *out);

sound_status_t sound_set_volume(uint8_t vol_pct);

uint8_t sound_get_volume(void);

sound_status_t sound_set_mute(int mute);

int sound_is_muted(void);

sound_status_t sound_set_volume_smooth(uint8_t vol_pct);

sound_status_t sound_set_channel_vol(uint8_t channel,
                                      uint8_t left, uint8_t right);

sound_status_t sound_set_channel_mute(uint8_t channel, int mute);

const char *sound_channel_name(uint8_t idx);

uint8_t sound_channel_count(void);

sound_status_t sound_set_eq_band(uint8_t band, const sound_eq_band_t *b);

sound_status_t sound_get_eq_band(uint8_t band, sound_eq_band_t *out);

sound_status_t sound_eq_enable(int enable);

int sound_eq_is_enabled(void);

sound_status_t sound_eq_reset(void);

typedef enum {
    SOUND_EQ_FLAT       = 0,
    SOUND_EQ_BASS_BOOST = 1,
    SOUND_EQ_TREBLE     = 2,
    SOUND_EQ_VOCAL      = 3,
    SOUND_EQ_LOUDNESS   = 4,
    SOUND_EQ_HEADPHONE  = 5,
    SOUND_EQ_SPEAKER    = 6,
} sound_eq_preset_t;

sound_status_t sound_eq_apply_preset(sound_eq_preset_t preset);

const char *sound_strerror(sound_status_t st);

#ifndef NDEBUG

void sound_debug_dump(void);
#else
static inline void sound_debug_dump(void) {}
#endif

#endif
