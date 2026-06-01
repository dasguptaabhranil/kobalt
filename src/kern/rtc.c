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

#include "../inc/rtc.h"
#include <stddef.h>

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static uint8_t cmos_read(uint8_t reg)
{
    outb(0x70, reg & 0x7Fu);
    return inb(0x71);
}

static void wait_not_updating(void)
{
    while (cmos_read(0x0A) & 0x80u)
        ;
}

static uint8_t bcd2bin(uint8_t v)
{
    return (uint8_t)((v >> 4) * 10u + (v & 0x0Fu));
}

void rtc_read(rtc_time_t *t)
{
    wait_not_updating();

    uint8_t sec  = cmos_read(0x00);
    uint8_t min  = cmos_read(0x02);
    uint8_t hour = cmos_read(0x04);
    uint8_t mday = cmos_read(0x07);
    uint8_t mon  = cmos_read(0x08);
    uint8_t yr   = cmos_read(0x09);
    uint8_t cent = cmos_read(0x32);
    uint8_t regb = cmos_read(0x0B);

    if (!(regb & 0x04u)) {
        sec  = bcd2bin(sec);
        min  = bcd2bin(min);
        hour = bcd2bin(hour & 0x7Fu) | (hour & 0x80u);
        mday = bcd2bin(mday);
        mon  = bcd2bin(mon);
        yr   = bcd2bin(yr);
        cent = bcd2bin(cent);
    }

    if (!(regb & 0x02u) && (hour & 0x80u)) {
        hour = (uint8_t)(((hour & 0x7Fu) + 12u) % 24u);
    } else {
        hour &= 0x7Fu;
    }

    uint16_t year;
    if (cent > 0u && cent <= 99u)
        year = (uint16_t)(cent * 100u + yr);
    else
        year = (uint16_t)((yr < 70u ? 2000u : 1900u) + yr);

    t->sec  = sec;
    t->min  = min;
    t->hour = hour;
    t->mday = mday;
    t->mon  = mon;
    t->year = year;
}

static void u2s(char *p, unsigned v, int w)
{
    p += w;
    *p = '\0';
    do {
        *--p = (char)('0' + v % 10u);
        v /= 10u;
    } while (--w > 0);
}

void rtc_fmt(const rtc_time_t *t, char *buf, size_t n)
{
    if (n < 20u) {
        if (n) buf[0] = '\0';
        return;
    }
    char tmp[20];
    u2s(tmp + 0,  t->year, 4);
    tmp[4]  = '-';
    u2s(tmp + 5,  t->mon,  2);
    tmp[7]  = '-';
    u2s(tmp + 8,  t->mday, 2);
    tmp[10] = ' ';
    u2s(tmp + 11, t->hour, 2);
    tmp[13] = ':';
    u2s(tmp + 14, t->min,  2);
    tmp[16] = ':';
    u2s(tmp + 17, t->sec,  2);
    tmp[19] = '\0';

    size_t i = 0;
    while (i < n - 1u && tmp[i]) { buf[i] = tmp[i]; i++; }
    buf[i] = '\0';
}
