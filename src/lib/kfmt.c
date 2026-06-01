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

#include "../inc/kfmt.h"
#include "../drivers/vga/vga.h"
#include <stdint.h>
#ifdef klog_ok
#  undef klog_ok
#endif
#ifdef klog_fail
#  undef klog_fail
#endif
#ifdef klog_info
#  undef klog_info
#endif
#ifdef klog_warn
#  undef klog_warn
#endif
#ifdef kprintf
#  undef kprintf
#endif

static int kfmt_vcore(char *buf, size_t n, const char *fmt, va_list ap)
{
    if (n == 0) return 0;
    size_t pos = 0;

    for (const char *f = fmt; *f && pos < n - 1; f++) {
        if (*f != '%') { buf[pos++] = *f; continue; }
        f++;

        int left_align = 0;
        int zero_pad   = 0;

        int parsing_flags = 1;
        while (parsing_flags) {
            if      (*f == '-') { left_align = 1; f++; }
            else if (*f == '0') { zero_pad   = 1; f++; }
            else                { parsing_flags = 0;   }
        }

        int width = 0;
        while (*f >= '0' && *f <= '9')
            width = width * 10 + (*f++ - '0');

        int is_ll = 0, is_l = 0, is_z = 0;
        if (*f == 'l') {
            f++;
            if (*f == 'l') { is_ll = 1; f++; }
            else              is_l  = 1;
        } else if (*f == 'z') {
            is_z = 1; f++;
        }

        if (*f == 's') {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            size_t slen = 0; while (s[slen]) slen++;
            int padding = width - (int)slen;
            if (!left_align) while (padding-- > 0 && pos < n - 1) buf[pos++] = ' ';
            for (size_t i = 0; i < slen && pos < n - 1; i++) buf[pos++] = s[i];
            if (left_align)  while (padding-- > 0 && pos < n - 1) buf[pos++] = ' ';
        }
        else if (*f == 'x' || *f == 'X') {
            unsigned long long val;
            if (is_ll)          val = va_arg(ap, unsigned long long);
            else if (is_l || is_z) val = va_arg(ap, unsigned long);
            else                val = va_arg(ap, unsigned int);
            static char tmp[24]; int ti = 0;
            if (val == 0) tmp[ti++] = '0';
            while (val > 0) {
                uint8_t nib = (uint8_t)(val & 0xF);
                tmp[ti++] = (nib < 10) ? (char)('0' + nib)
                                       : (char)((*f == 'X' ? 'A' : 'a') + (nib - 10));
                val >>= 4;
            }
            char pad_ch = (zero_pad && !left_align) ? '0' : ' ';
            int padding = width - ti;
            if (!left_align) while (padding-- > 0 && pos < n - 1) buf[pos++] = pad_ch;
            while (ti > 0  && pos < n - 1) buf[pos++] = tmp[--ti];
            if (left_align)  while (padding-- > 0 && pos < n - 1) buf[pos++] = ' ';
        }
        else if (*f == 'd' || *f == 'i' || *f == 'u') {
            unsigned long long val;
            int neg = 0;
            if (*f == 'd' || *f == 'i') {
                long long sv;
                if (is_ll)          sv = va_arg(ap, long long);
                else if (is_l || is_z) sv = va_arg(ap, long);
                else           sv = va_arg(ap, int);
                if (sv < 0) { neg = 1; val = (unsigned long long)(-sv); }
                else          val = (unsigned long long)sv;
            } else {
                if (is_ll)          val = va_arg(ap, unsigned long long);
                else if (is_l || is_z) val = va_arg(ap, unsigned long);
                else           val = va_arg(ap, unsigned int);
            }
            static char tmp[24]; int ti = 0;
            if (val == 0) tmp[ti++] = '0';
            while (val > 0) { tmp[ti++] = (char)('0' + (val % 10)); val /= 10; }

            char pad_ch = (zero_pad && !left_align) ? '0' : ' ';
            int num_width = ti + (neg ? 1 : 0);
            int padding   = width - num_width;

            if (!left_align && !zero_pad)
                while (padding-- > 0 && pos < n - 1) buf[pos++] = pad_ch;
            if (neg && pos < n - 1) buf[pos++] = '-';
            if (!left_align && zero_pad)
                while (padding-- > 0 && pos < n - 1) buf[pos++] = pad_ch;
            while (ti > 0  && pos < n - 1) buf[pos++] = tmp[--ti];
            if (left_align)
                while (padding-- > 0 && pos < n - 1) buf[pos++] = ' ';
        }
        else if (*f == 'c') {
            if (pos < n - 1) buf[pos++] = (char)va_arg(ap, int);
        }
        else if (*f == 'p') {
            uintptr_t pval = (uintptr_t)va_arg(ap, void *);
            if (pos < n - 1) buf[pos++] = '0';
            if (pos < n - 1) buf[pos++] = 'x';
            int ptr_digits = (int)(sizeof(uintptr_t) * 2);
            static char tmp[16]; int ti = 0;
            if (pval == 0) tmp[ti++] = '0';
            while (pval > 0) {
                uint8_t nib = pval & 0xF;
                tmp[ti++] = (nib < 10) ? (char)('0' + nib) : (char)('a' + (nib - 10));
                pval >>= 4;
            }
            int padding = ptr_digits - ti;
            while (padding-- > 0 && pos < n - 1) buf[pos++] = '0';
            while (ti > 0 && pos < n - 1) buf[pos++] = tmp[--ti];
        }
        else if (*f == '%') {
            if (pos < n - 1) buf[pos++] = '%';
        }
    }
    buf[pos] = '\0';
    return (int)pos;
}

int ksnprintf(char *buf, size_t n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = kfmt_vcore(buf, n, fmt, ap);
    va_end(ap);
    return ret;
}

int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap) {
    return kfmt_vcore(buf, n, fmt, ap);
}

int snprintf(char *buf, size_t n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = kfmt_vcore(buf, n, fmt, ap);
    va_end(ap);
    return ret;
}

#define KLOG_SUBSYS_W    28
#define KLOG_DETAIL_MAX  38
#define KLOG_TAG_COL     70

void klog_status(const char *subsys, const char *detail,
                 const char *tag, const char *msg)
{
    (void)msg;

    char det[KLOG_DETAIL_MAX + 1];
    size_t dlen = 0;
    while (detail[dlen]) dlen++;
    if (dlen > (size_t)KLOG_DETAIL_MAX) {
        size_t copy = (size_t)(KLOG_DETAIL_MAX - 2);
        for (size_t i = 0; i < copy; i++) det[i] = detail[i];
        det[copy] = '.'; det[copy+1] = '.'; det[copy+2] = '\0';
    } else {
        for (size_t i = 0; i <= dlen; i++) det[i] = detail[i];
    }

    char line[96];
    int n = ksnprintf(line, sizeof(line), "  %-28s %s", subsys, det);
    while (n < KLOG_TAG_COL && n < (int)(sizeof(line) - 12)) line[n++] = ' ';
    line[n++] = '['; line[n++] = ' ';
    for (const char *t = tag; *t && n < (int)(sizeof(line)-4);) line[n++] = *t++;
    line[n++] = ']'; line[n++] = ' '; line[n++] = '\n'; line[n] = '\0';
    kputs(line);
}

void* memcpy(void* restrict dest, const void* restrict src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    while (n--) *d++ = *s++;
    return dest;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char *p1 = s1, *p2 = s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}

size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = s;
    while (n--) {
        if (*p == (unsigned char)c) return (void *)p;
        p++;
    }
    return NULL;
}

size_t strnlen(const char *s, size_t maxlen) {
    size_t len;
    for (len = 0; len < maxlen; len++) {
        if (!s[len]) break;
    }
    return len;
}

size_t strlcpy(char * restrict dest, const char * restrict src, size_t size) {
    size_t len = 0;
    while (src[len]) len++;

    if (size > 0) {
        size_t copy_len = (len < size - 1) ? len : size - 1;
        memcpy(dest, src, copy_len);
        dest[copy_len] = '\0';
    }
    return len;
}

int atoi(const char *s) {
    int res = 0;
    int sign = 1;

    while (*s == ' ' || (*s >= '\t' && *s <= '\r')) s++;

    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    while (*s >= '0' && *s <= '9') {
        res = res * 10 + (*s - '0');
        s++;
    }

    return res * sign;
}

int abs(int j) {
    return (j < 0) ? -j : j;
}

#ifndef KPRINTF_BUFSZ
#  define KPRINTF_BUFSZ 256u
#endif

int kprintf(const char *fmt, ...)
{
    static char buf[KPRINTF_BUFSZ];
    va_list ap;
    va_start(ap, fmt);
    int n = kfmt_vcore(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    kputs(buf);
    return n;
}

extern void uart_puts(const char *s);
extern void vga_putc(char c);
extern void dmesg_write(const char *s);

static void klog_colored(const char *sys, const char *detail,
                         const char *badge, const char *ansi,
                         vga_color_t color)
{
    char line[96];
    int n = ksnprintf(line, sizeof(line), "  %-28s ", sys);

    size_t dlen = 0; while (detail[dlen]) dlen++;
    if (dlen > 38) {
        for (int i = 0; i < 36; i++) line[n++] = detail[i];
        line[n++] = '.'; line[n++] = '.';
    } else {
        for (size_t i = 0; i < dlen; i++) line[n++] = detail[i];
    }
    while (n < 70 && n < 88) line[n++] = ' ';
    line[n] = '\0';

    vga_puts(line);
    vga_set_color(color, VGA_COLOR_BLACK);
    vga_puts(badge);
    vga_reset_color();
    vga_putc('\n');

    uart_puts(line);
    uart_puts(ansi);
    uart_puts(badge);
    uart_puts("\033[0m\n");

    dmesg_write(line);
    dmesg_write(badge);
    dmesg_write("\n");
}

void klog_ok  (const char *sys, const char *detail)
    { klog_colored(sys, detail, "[  OK  ]", "\033[32m", VGA_COLOR_LIGHT_GREEN); }

void klog_fail(const char *sys, const char *detail)
    { klog_colored(sys, detail, "[ FAIL ]", "\033[1;31m", VGA_COLOR_LIGHT_RED); }

void klog_info(const char *sys, const char *detail)
    { klog_colored(sys, detail, "[ INFO ]", "\033[37m", VGA_COLOR_WHITE);       }

void klog_warn(const char *sys, const char *detail)
    { klog_colored(sys, detail, "[ WARN ]", "\033[33m", VGA_COLOR_YELLOW);      }

int strncmp(const char *s1, const char *s2, size_t n)
{
    while (n--) {
        unsigned char c1 = (unsigned char)*s1++;
        unsigned char c2 = (unsigned char)*s2++;
        if (c1 != c2) return (int)c1 - (int)c2;
        if (c1 == '\0') break;
    }
    return 0;
}

void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    if (d == s || n == 0)
        return dst;

    if (d < s) {

        while (n--) *d++ = *s++;
    } else {

        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}
