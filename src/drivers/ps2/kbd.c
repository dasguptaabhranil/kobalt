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

#include "kbd.h"
#include <sched.h>

static kbd_state_t g_kbd = { .mods = 0, .e0_pending = 0 };

static const char kbd_map_normal[0x59] = {
    '\0',
    '\0',
    '1','2','3','4','5','6','7','8','9','0',
    '-','=',
    '\b',
    '\t',
    'q','w','e','r','t','y','u','i','o','p',
    '[',']',
    '\n',
    '\0',
    'a','s','d','f','g','h','j','k','l',
    ';','\'','`',
    '\0',
    '\\',
    'z','x','c','v','b','n','m',
    ',','.','/',
    '\0',
    '*',
    '\0',
    ' ',
    '\0',
    '\0','\0','\0','\0','\0','\0','\0','\0','\0','\0',
    '\0','\0',
    '7','8','9',
    '-',
    '4','5','6',
    '+',
    '1','2','3',
    '0',
    '.',
    '\0','\0','\0',
    '\0','\0',
};

static const char kbd_map_shifted[0x59] = {
    '\0',
    '\0',
    '!','@','#','$','%','^','&','*','(',')',
    '_','+',
    '\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P',
    '{','}',
    '\n',
    '\0',
    'A','S','D','F','G','H','J','K','L',
    ':','"','~',
    '\0',
    '|',
    'Z','X','C','V','B','N','M',
    '<','>','?',
    '\0','*','\0',' ','\0',
    '\0','\0','\0','\0','\0','\0','\0','\0','\0','\0',
    '\0','\0',
    '\0','\0','\0','\0',
    '\0','\0','\0','\0',
    '\0','\0','\0','\0','\0',
    '\0','\0','\0','\0','\0',
};

static uint16_t kbd_translate(uint8_t raw)
{

    if (g_kbd.e0_pending) {
        g_kbd.e0_pending = 0;

        const int     rel = (raw & 0x80u) != 0;
        const uint8_t sc  = raw & 0x7Fu;

        if (rel) {
            if (sc == 0x1D) g_kbd.mods &= (uint8_t)~KBD_MOD_RCTRL;
            if (sc == 0x38) g_kbd.mods &= (uint8_t)~KBD_MOD_RALT;
            return KBD_KEY_NONE;
        }
        switch (sc) {
            case 0x1D: g_kbd.mods |= KBD_MOD_RCTRL; return KBD_KEY_NONE;
            case 0x38: g_kbd.mods |= KBD_MOD_RALT;  return KBD_KEY_NONE;
            case 0x47: return KBD_KEY_HOME;
            case 0x48: return KBD_KEY_UP;
            case 0x49: return KBD_KEY_PGUP;
            case 0x4B: return KBD_KEY_LEFT;
            case 0x4D: return KBD_KEY_RIGHT;
            case 0x4F: return KBD_KEY_END;
            case 0x50: return KBD_KEY_DOWN;
            case 0x51: return KBD_KEY_PGDN;
            case 0x52: return KBD_KEY_INS;
            case 0x53: return KBD_KEY_DEL;
            default:   return KBD_KEY_NONE;
        }
    }

    if (raw == 0xE0u) {
        g_kbd.e0_pending = 1;
        return KBD_KEY_NONE;
    }

    const int     rel = (raw & 0x80u) != 0;
    const uint8_t sc  = raw & 0x7Fu;

    if (rel) {
        switch (sc) {
            case 0x2A: g_kbd.mods &= (uint8_t)~KBD_MOD_LSHIFT; break;
            case 0x36: g_kbd.mods &= (uint8_t)~KBD_MOD_RSHIFT; break;
            case 0x1D: g_kbd.mods &= (uint8_t)~KBD_MOD_LCTRL;  break;
            case 0x38: g_kbd.mods &= (uint8_t)~KBD_MOD_LALT;   break;
            default: break;
        }
        return KBD_KEY_NONE;
    }

    switch (sc) {
        case 0x2A: g_kbd.mods |= KBD_MOD_LSHIFT; return KBD_KEY_NONE;
        case 0x36: g_kbd.mods |= KBD_MOD_RSHIFT; return KBD_KEY_NONE;
        case 0x1D: g_kbd.mods |= KBD_MOD_LCTRL;  return KBD_KEY_NONE;
        case 0x38: g_kbd.mods |= KBD_MOD_LALT;   return KBD_KEY_NONE;
        case 0x3A: g_kbd.mods ^= KBD_MOD_CAPS;   return KBD_KEY_CAPSLOCK;
        case 0x45: g_kbd.mods ^= KBD_MOD_NUM;    return KBD_KEY_NUMLOCK;
        case 0x46:                                return KBD_KEY_SCRLOCK;
        case 0x01:                                return KBD_KEY_ESC;
        case 0x3B:                                return KBD_KEY_F1;
        case 0x3C:                                return KBD_KEY_F2;
        case 0x3D:                                return KBD_KEY_F3;
        case 0x3E:                                return KBD_KEY_F4;
        case 0x3F:                                return KBD_KEY_F5;
        case 0x40:                                return KBD_KEY_F6;
        case 0x41:                                return KBD_KEY_F7;
        case 0x42:                                return KBD_KEY_F8;
        case 0x43:                                return KBD_KEY_F9;
        case 0x44:                                return KBD_KEY_F10;
        case 0x57:                                return KBD_KEY_F11;
        case 0x58:                                return KBD_KEY_F12;
        default: break;
    }

    if (sc >= 0x59u)
        return KBD_KEY_NONE;

    const int shifted = (g_kbd.mods & KBD_MOD_SHIFT) != 0;
    char ch = shifted ? kbd_map_shifted[sc] : kbd_map_normal[sc];

    if (g_kbd.mods & KBD_MOD_CAPS) {
        if      (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
        else if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
    }

    if (g_kbd.mods & KBD_MOD_CTRL) {
        if (ch >= '@' && ch <= '_') return (uint16_t)(ch & 0x1Fu);
        if (ch >= 'a' && ch <= 'z') return (uint16_t)((ch - 'a') + 1u);
        if (ch >= 'A' && ch <= 'Z') return (uint16_t)((ch - 'A') + 1u);
    }

    return (ch != '\0') ? (uint16_t)(uint8_t)ch : KBD_KEY_NONE;
}

void kbd_init(void)
{
    g_kbd.mods       = 0;
    g_kbd.e0_pending = 0;

    while (inb(KBD_STATUS_PORT) & KBD_STATUS_OBF)
        (void)inb(KBD_DATA_PORT);
}

uint16_t kbd_poll(void)
{
    if (!(inb(KBD_STATUS_PORT) & KBD_STATUS_OBF))
        return KBD_KEY_NONE;
    return kbd_translate(inb(KBD_DATA_PORT));
}

uint16_t kbd_getkey(void)
{
    uint16_t key;
    do {

        while (!(inb(KBD_STATUS_PORT) & KBD_STATUS_OBF)) {
            sched_yield();
        }

        key = kbd_translate(inb(KBD_DATA_PORT));

    } while (key == KBD_KEY_NONE);

    return key;
}

char kbd_getc(void)
{
    for (;;) {
        const uint16_t key = kbd_getkey();
        if (key > 0u && key < 0x100u)
            return (char)(uint8_t)key;
    }
}

kbd_state_t kbd_state(void)
{
    return g_kbd;
}
