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
#include "../inc/usb.h"
#include "../inc/usb_core.h"
#include "../inc/kernel.h"

typedef struct {
    int16_t  x, y, wheel;
    uint8_t  buttons;
} mouse_state_t;

static mouse_state_t g_mouse;

mouse_state_t *usb_mouse_get_state(void) { return &g_mouse; }

static void mouse_report(usb_device_t *d, const uint8_t *buf, uint8_t len)
{
    (void)d;
    if (len < 3) return;
    g_mouse.buttons = buf[0] & 0x07;
    g_mouse.x       = (int16_t)(int8_t)buf[1];
    g_mouse.y       = (int16_t)(int8_t)buf[2];
    g_mouse.wheel   = (len >= 4) ? (int16_t)(int8_t)buf[3] : 0;
}

static int mouse_init(usb_device_t *d, uint8_t iface, uint8_t ep,
                       const uint8_t *rd, uint16_t rl)
{
    (void)ep; (void)rd; (void)rl;
    usb_set_protocol(d, iface, USB_HID_PROTO_MOUSE);
    klog_ok("hid_mouse", "USB mouse ready");
    return 0;
}

static void mouse_deinit(usb_device_t *d) { (void)d; }

void hid_mouse_init(void)
{
    extern void hid_register_subdriver(uint16_t, uint16_t,
        int(*)(usb_device_t*,uint8_t,uint8_t,const uint8_t*,uint16_t),
        void(*)(usb_device_t*,const uint8_t*,uint8_t), void(*)(usb_device_t*));
    hid_register_subdriver(0x0001, 0x0002, mouse_init, mouse_report, mouse_deinit);
    klog_ok("hid_mouse", "mouse sub-driver registered");
}
