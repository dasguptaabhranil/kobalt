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
#include "../inc/kmalloc.h"

#define MOD_LSHIFT (1u<<1)
#define MOD_RSHIFT (1u<<5)
#define MOD_LCTRL  (1u<<0)
#define MOD_RCTRL  (1u<<4)
#define MOD_SHIFT  (MOD_LSHIFT|MOD_RSHIFT)
#define MOD_CTRL   (MOD_LCTRL|MOD_RCTRL)

static const char g_asc[0x69] = {
    0,0,0,0,'a','b','c','d','e','f','g','h','i','j','k','l',
    'm','n','o','p','q','r','s','t','u','v','w','x','y','z',
    '1','2','3','4','5','6','7','8','9','0',
    '\n',0x1B,'\b','\t',' ','-','=','[',']','\\','#',';','\'','`',',','.',
    '/',
    0,
    0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,
    0,0,0,
    0x7F,
    0,0,
    0,0,0,0,
    0,
    '/','*','-','+','\n',
    '1','2','3','4','5','6','7','8','9','0','.',
    '\\',0,0,'=',0,
};

static const char g_sft[0x69] = {
    0,0,0,0,'A','B','C','D','E','F','G','H','I','J','K','L',
    'M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    '!','@','#','$','%','^','&','*','(',')',
    '\n',0x1B,'\b','\t',' ','_','+','{','}','|','~',':','"','~','<','>',
    '?',
    0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0x7F,0,0,0,0,0,0,0,
    '/',  '*',  '-',  '+',  '\n',
    '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',  '.',
    '|',  0,    0,    '=',  0,
};

static const char * const g_seq[0x69] = {
    [0x3A] = "\x1BOP",   [0x3B] = "\x1BOQ",   [0x3C] = "\x1BOR",
    [0x3D] = "\x1BOS",   [0x3E] = "\x1B[15~", [0x3F] = "\x1B[17~",
    [0x40] = "\x1B[18~", [0x41] = "\x1B[19~", [0x42] = "\x1B[20~",
    [0x43] = "\x1B[21~", [0x44] = "\x1B[23~", [0x45] = "\x1B[24~",
    [0x49] = "\x1B[2~",  [0x4A] = "\x1B[H",   [0x4B] = "\x1B[5~",
    [0x4D] = "\x1B[F",   [0x4E] = "\x1B[6~",
    [0x4F] = "\x1B[C",   [0x50] = "\x1B[D",
    [0x51] = "\x1B[B",   [0x52] = "\x1B[A",
};

static const char * const g_kpnav[11] = {
    "\x1B[F",  "\x1B[B",  "\x1B[6~", "\x1B[D",  NULL,
    "\x1B[C",  "\x1B[H",  "\x1B[A",  "\x1B[5~", "\x1B[2~", "\x7F",
};

#define RING 64
static char     g_ring[RING];
static volatile uint8_t g_rh, g_rt;

static void push(char c)
{
    uint8_t nx = (g_rt + 1u) % RING;
    if (nx != g_rh) { g_ring[g_rt] = c; g_rt = nx; }
}

static void push_str(const char *s) { while (*s) push(*s++); }

char usb_kbd_getc(void)
{
    if (g_rh == g_rt) return 0;
    char c = g_ring[g_rh];
    g_rh = (g_rh + 1u) % RING;
    return c;
}

typedef struct { uint8_t prev[6], caps, num; } kbd_state_t;

static void kbd_report(usb_device_t *d, const uint8_t *buf, uint8_t len)
{
    if (!d->driver_data || len < 3) return;
    kbd_state_t *ks = d->driver_data;
    uint8_t mod = buf[0];
    int sft = !!(mod & MOD_SHIFT) ^ ks->caps;

    for (int i = 2; i < 8 && i < len; i++) {
        uint8_t kc = buf[i];
        if (!kc) continue;
        int already = 0;
        for (int j = 0; j < 6; j++) if (ks->prev[j] == kc) { already = 1; break; }
        if (already) continue;

        if (kc == 0x39) { ks->caps ^= 1; continue; }
        if (kc == 0x53) { ks->num  ^= 1; continue; }

        if (kc < 0x69 && g_seq[kc]) { push_str(g_seq[kc]); continue; }

        if (!ks->num && kc >= 0x59 && kc <= 0x63) {
            const char *s = g_kpnav[kc - 0x59];
            if (s) push_str(s);
            continue;
        }

        char c = 0;
        if (kc < 0x69) c = sft ? g_sft[kc] : g_asc[kc];
        if ((mod & MOD_CTRL) && c >= 'a' && c <= 'z') c = (char)(c - 'a' + 1);
        else if ((mod & MOD_CTRL) && c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 1);
        if (c) push(c);
    }
    for (int i = 0; i < 6; i++) ks->prev[i] = (i + 2 < len) ? buf[i + 2] : 0;
}

static int kbd_init(usb_device_t *d, uint8_t iface, uint8_t ep,
                     const uint8_t *rd, uint16_t rl)
{
    (void)ep; (void)rd; (void)rl;
    kbd_state_t *ks = kmalloc(sizeof(*ks));
    if (!ks) return -1;
    memset(ks, 0, sizeof(*ks));
    ks->num = 1;
    d->driver_data = ks;
    usb_set_protocol(d, iface, USB_HID_PROTO_KEYBOARD);
    klog_ok("hid_kbd", "USB keyboard ready");
    return 0;
}

static void kbd_deinit(usb_device_t *d)
{
    if (d->driver_data) { kfree(d->driver_data); d->driver_data = NULL; }
}

void hid_kbd_init(void)
{
    extern void hid_register_subdriver(uint16_t, uint16_t,
        int(*)(usb_device_t*,uint8_t,uint8_t,const uint8_t*,uint16_t),
        void(*)(usb_device_t*,const uint8_t*,uint8_t), void(*)(usb_device_t*));
    hid_register_subdriver(0x0001, 0x0006, kbd_init, kbd_report, kbd_deinit);
    klog_ok("hid_kbd", "keyboard sub-driver registered");
}
