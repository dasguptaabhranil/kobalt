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

#ifndef USB_INIT_H
#define USB_INIT_H

int  usb_core_init(void);
void usb_core_poll(void);
void usb_hub_init(void);
void hid_init(void);
void hid_kbd_init(void);
void hid_mouse_init(void);
void hid_tablet_init(void);
void usb_msc_init(void);
void usb_audio_init(void);
void usb_midi_init(void);
void usb_cdc_init(void);
void usb_rndis_init(void);
void usb_cdc_acm_init(void);
void usb_ftdi_init(void);
void usb_cp210x_init(void);
void btusb_init(void);

static inline int usb_subsystem_init(void)
{
    usb_hub_init();
    hid_init();
    hid_kbd_init();
    hid_mouse_init();
    hid_tablet_init();
    usb_msc_init();
    usb_audio_init();
    usb_midi_init();
    usb_cdc_init();
    usb_rndis_init();
    usb_cdc_acm_init();
    usb_ftdi_init();
    usb_cp210x_init();
    btusb_init();
    return usb_core_init();
}

static inline void usb_subsystem_poll(void)
{
    usb_core_poll();
}

#endif
