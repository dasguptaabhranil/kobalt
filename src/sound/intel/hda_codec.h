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

#ifndef KOBALT_HDA_CODEC_H
#define KOBALT_HDA_CODEC_H

#include "hda.h"

hda_status_t hda_codec_enumerate(void);
void         hda_codec_route_default_paths(void);
hda_status_t hda_codec_set_power(uint8_t codec_idx, uint8_t state);

hda_status_t hda_verb_exec(uint8_t codec, uint8_t nid,
                            uint32_t verb_id, uint32_t payload,
                            uint32_t *resp_out);

uintptr_t    hda_mmio(void);
uint8_t      hda_codec_count(void);
hda_codec_t *hda_codec_ptr(uint8_t i);
void         hda_codec_count_inc(void);
uint8_t      hda_num_iss(void);
uint8_t      hda_num_oss(void);

uint8_t                 hda_codec_find_dac(uint8_t codec_idx, uint8_t pin_nid);
uint8_t                 hda_codec_get_path_count(uint8_t codec_idx);
const hda_audio_path_t *hda_codec_get_path(uint8_t codec_idx, uint8_t path_idx);

#ifndef NDEBUG
void hda_codec_debug_dump(uint8_t codec_idx);
#else
static inline void hda_codec_debug_dump(uint8_t codec_idx) { (void)codec_idx; }
#endif

#endif
