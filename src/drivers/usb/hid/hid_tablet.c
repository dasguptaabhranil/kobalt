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

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../inc/usb.h"
#include "../inc/usb_core.h"
#include "../inc/kernel.h"

typedef struct {
    uint16_t x, y;
    uint8_t  pressure;
    uint8_t  tip, barrel, eraser;
} tablet_state_t;

static tablet_state_t g_tab;

const tablet_state_t *usb_tablet_get_state(void) { return &g_tab; }

static void tab_report(usb_device_t *d, const uint8_t *buf, uint8_t len)
{
    (void)d;
    if (len < 5) return;
    g_tab.tip    = buf[0] & 0x01;
    g_tab.barrel = (buf[0] >> 1) & 0x01;
    g_tab.eraser = (buf[0] >> 2) & 0x01;
    g_tab.x      = (uint16_t)(buf[1] | ((uint16_t)buf[2] << 8));
    g_tab.y      = (uint16_t)(buf[3] | ((uint16_t)buf[4] << 8));
    g_tab.pressure = len >= 6 ? buf[5] : 0;
}

static int tab_init(usb_device_t *d, uint8_t iface, uint8_t ep,
                     const uint8_t *rd, uint16_t rl)
{
    (void)d; (void)iface; (void)ep; (void)rd; (void)rl;
    memset(&g_tab, 0, sizeof(g_tab));
    klog_ok("hid_tablet", "USB tablet/digitizer ready");
    return 0;
}

static void tab_deinit(usb_device_t *d) { (void)d; }

void hid_tablet_init(void)
{
    extern void hid_register_subdriver(uint16_t, uint16_t,
        int(*)(usb_device_t*,uint8_t,uint8_t,const uint8_t*,uint16_t),
        void(*)(usb_device_t*,const uint8_t*,uint8_t), void(*)(usb_device_t*));
    hid_register_subdriver(0x000D, 0x0001, tab_init, tab_report, tab_deinit);
    hid_register_subdriver(0x000D, 0x0002, tab_init, tab_report, tab_deinit);
    hid_register_subdriver(0x0001, 0x0004, tab_init, tab_report, tab_deinit);
    klog_ok("hid_tablet", "tablet sub-driver registered");
}
