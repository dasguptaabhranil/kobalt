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

#include "amx_tile.h"
#include "amx_state.h"
#include <string.h>

void amx_tile_config(amx_tilecfg_t *cfg, uint8_t palette,
                     const uint8_t *rows, const uint16_t *colsb, int ntiles)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->palette_id = palette;
    for (int i = 0; i < ntiles && i < AMX_MAX_TILES; i++) {
        cfg->rows[i]  = rows[i];
        cfg->colsb[i] = colsb[i];
    }
    amx_ldtilecfg(cfg);
}

void amx_tile_release_all(void)
{
    amx_tilerelease();
}
