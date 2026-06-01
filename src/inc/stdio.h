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

#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
#  define KPRINTF_LIKE(fmt_idx, va_idx) \
       __attribute__((format(printf, fmt_idx, va_idx)))
#else
#  define KPRINTF_LIKE(fmt_idx, va_idx)
#endif

#if defined(__GNUC__) || defined(__clang__)
#  define KNORETURN     __attribute__((noreturn))
#else
#  define KNORETURN     _Noreturn
#endif

int snprintf(char * restrict buf, size_t size,
             const char * restrict fmt, ...) KPRINTF_LIKE(3, 4);

int vsnprintf(char * restrict buf, size_t size,
              const char * restrict fmt, va_list ap);

int scnprintf(char * restrict buf, size_t size,
              const char * restrict fmt, ...) KPRINTF_LIKE(3, 4);

int vscnprintf(char * restrict buf, size_t size,
               const char * restrict fmt, va_list ap);

void kputc(char c);

void kputs(const char *s);

#ifndef kprintf
int kprintf(const char * restrict fmt, ...) KPRINTF_LIKE(1, 2);
#endif

int kvprintf(const char * restrict fmt, va_list ap);

int kprintf_buf(char *buf, size_t size,
                const char * restrict fmt, ...) KPRINTF_LIKE(3, 4);

#define KPRINTF_BUFSZ   256u

#define KLOG_STATUS_COL     60u

KNORETURN void kpanicf(const char * restrict fmt, ...) KPRINTF_LIKE(1, 2);

KNORETURN void kvpanicf(const char * restrict fmt, va_list ap);

#ifndef NDEBUG

#  define KASSERT(expr)                                                     \
     do {                                                                   \
         if (__builtin_expect(!(expr), 0))                                  \
             kpanicf("KASSERT failed: %s  at %s:%d in %s()",               \
                     #expr, __FILE__, __LINE__, __func__);                  \
     } while (0)

#  define KASSERTF(expr, fmt, ...)                                          \
     do {                                                                   \
         if (__builtin_expect(!(expr), 0))                                  \
             kpanicf("KASSERT failed: %s  at %s:%d in %s(): " fmt,         \
                     #expr, __FILE__, __LINE__, __func__, ##__VA_ARGS__);   \
     } while (0)

#else

#  define KASSERT(expr)                 ((void)0)
#  define KASSERTF(expr, fmt, ...)      ((void)0)

#endif

#define KASSERT_NOT_NULL(ptr)                                               \
    KASSERTF((ptr) != NULL, "unexpected NULL pointer: " #ptr)

#define DECIMAL_WIDTH(n)    \
    ((n) < 10u          ? 1 : \
     (n) < 100u         ? 2 : \
     (n) < 1000u        ? 3 : \
     (n) < 10000u       ? 4 : \
     (n) < 100000u      ? 5 : \
     (n) < 1000000u     ? 6 : \
     (n) < 10000000u    ? 7 : \
     (n) < 100000000u   ? 8 : \
     (n) < 1000000000u  ? 9 : 10)

#define HEX_WIDTH(type)     (sizeof(type) * 2u)

#endif
