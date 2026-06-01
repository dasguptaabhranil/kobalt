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

#ifndef KOBALT_HDA_MIXER_H
#define KOBALT_HDA_MIXER_H

#include "hda.h"

void hda_mixer_init_channels(void);
void hda_mixer_apply_vol(void);
void hda_mixer_ramp_vol(uint8_t from_pct, uint8_t to_pct);
void hda_mixer_jack_event(uint8_t codec_idx, uint8_t pin_nid);
void hda_mixer_apply_eq(const hda_eq_band_t *bands,
                         uint8_t band_count, uint8_t enabled);

uint8_t              hda_master_vol(void);
uint8_t              hda_master_muted(void);
hda_mixer_channel_t *hda_mixer_ch(uint8_t i);

#ifndef NDEBUG
void hda_mixer_debug_dump(void);
#else
static inline void hda_mixer_debug_dump(void) {}
#endif

#endif
