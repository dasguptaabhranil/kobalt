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

#ifndef KBD_H
#define KBD_H
#include <kernel.h>

#define KBD_DATA_PORT    0x60u
#define KBD_STATUS_PORT  0x64u
#define KBD_STATUS_OBF   0x01u
#define KBD_STATUS_IBF   0x02u

#define KBD_KEY_NONE     0x0000u
#define KBD_KEY_ESC      0x0100u
#define KBD_KEY_F1       0x0101u
#define KBD_KEY_F2       0x0102u
#define KBD_KEY_F3       0x0103u
#define KBD_KEY_F4       0x0104u
#define KBD_KEY_F5       0x0105u
#define KBD_KEY_F6       0x0106u
#define KBD_KEY_F7       0x0107u
#define KBD_KEY_F8       0x0108u
#define KBD_KEY_F9       0x0109u
#define KBD_KEY_F10      0x010Au
#define KBD_KEY_F11      0x010Bu
#define KBD_KEY_F12      0x010Cu
#define KBD_KEY_UP       0x0200u
#define KBD_KEY_DOWN     0x0201u
#define KBD_KEY_LEFT     0x0202u
#define KBD_KEY_RIGHT    0x0203u
#define KBD_KEY_HOME     0x0204u
#define KBD_KEY_END      0x0205u
#define KBD_KEY_PGUP     0x0206u
#define KBD_KEY_PGDN     0x0207u
#define KBD_KEY_INS      0x0208u
#define KBD_KEY_DEL      0x0209u
#define KBD_KEY_NUMLOCK  0x0300u
#define KBD_KEY_SCRLOCK  0x0301u
#define KBD_KEY_CAPSLOCK 0x0302u

#define KBD_MOD_LSHIFT   0x01u
#define KBD_MOD_RSHIFT   0x02u
#define KBD_MOD_SHIFT    (KBD_MOD_LSHIFT | KBD_MOD_RSHIFT)
#define KBD_MOD_LCTRL    0x04u
#define KBD_MOD_RCTRL    0x08u
#define KBD_MOD_CTRL     (KBD_MOD_LCTRL  | KBD_MOD_RCTRL)
#define KBD_MOD_LALT     0x10u
#define KBD_MOD_RALT     0x20u
#define KBD_MOD_ALT      (KBD_MOD_LALT   | KBD_MOD_RALT)
#define KBD_MOD_CAPS     0x40u
#define KBD_MOD_NUM      0x80u

typedef struct {
    uint8_t  mods;
    int      e0_pending;
} kbd_state_t;

void        kbd_init(void);
uint16_t    kbd_poll(void);
uint16_t    kbd_getkey(void);
char        kbd_getc(void);
kbd_state_t kbd_state(void);

#endif
