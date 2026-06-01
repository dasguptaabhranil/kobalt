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

#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>
#include <stdint.h>

int atoi(const char *nptr);

long atol(const char *nptr);

long long atoll(const char *nptr);

long strtol(const char * restrict nptr, char ** restrict endptr, int base);

long long strtoll(const char * restrict nptr,
                  char ** restrict endptr, int base);

unsigned long strtoul(const char * restrict nptr,
                      char ** restrict endptr, int base);

unsigned long long strtoull(const char * restrict nptr,
                             char ** restrict endptr, int base);

uintmax_t strtoumax(const char * restrict nptr,
                    char ** restrict endptr, int base);

intmax_t strtoimax(const char * restrict nptr,
                   char ** restrict endptr, int base);

#define ITOA_BUFSZ      34
char *itoa(int value, char *buf, int base);

char *utoa(unsigned int value, char *buf, int base);

int abs(int j);

long labs(long j);

long long llabs(long long j);

typedef struct { int       quot; int       rem; } div_t;
typedef struct { long      quot; long      rem; } ldiv_t;
typedef struct { long long quot; long long rem; } lldiv_t;

div_t   div(int numer, int denom);

ldiv_t  ldiv(long numer, long denom);

lldiv_t lldiv(long long numer, long long denom);

void *bsearch(const void *key, const void *base,
              size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));

#define KLIB_ERANGE     1
#define KLIB_EINVAL     2

int klib_errno(void);

void klib_errno_clear(void);

#define kclamp(val, lo, hi)     (((val) < (lo)) ? (lo) : (((val) > (hi)) ? (hi) : (val)))

#define kmin(a, b) ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b);                       _a < _b ? _a : _b; })
#define kmax(a, b) ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b);                       _a > _b ? _a : _b; })

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define KOBALT_UNUSED(x)    ((void)(x))

#define KOBALT_STATIC_ASSERT(expr, msg)     _Static_assert(expr, msg)

#endif
