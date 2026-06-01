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

#ifndef STRING_H
#define STRING_H

#include <stddef.h>

void *memcpy(void * restrict dest, const void * restrict src, size_t n);

void *memmove(void *dest, const void *src, size_t n);

void *memset(void *s, int c, size_t n);

int   memcmp(const void *s1, const void *s2, size_t n);

void *memchr(const void *s, int c, size_t n);

size_t strlen(const char *s);

size_t strnlen(const char *s, size_t maxlen);

int strcmp(const char *s1, const char *s2);

int strncmp(const char *s1, const char *s2, size_t n);

static inline char *strchr(const char *s, int c)
{
    unsigned char uc = (unsigned char)c;
    for (; *s != '\0'; s++)
        if ((unsigned char)*s == uc)
            return (char *)s;
    return uc == '\0' ? (char *)s : NULL;
}

static inline char *strrchr(const char *s, int c)
{
    unsigned char uc = (unsigned char)c;
    const char *last = NULL;
    for (; *s != '\0'; s++)
        if ((unsigned char)*s == uc)
            last = s;
    return uc == '\0' ? (char *)s : (char *)last;
}

char *strncpy(char * restrict dest, const char * restrict src, size_t n);

size_t strlcpy(char * restrict dest, const char * restrict src, size_t size);

size_t strlcat(char * restrict dest, const char * restrict src, size_t size);

static inline int kstrcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

static inline size_t kstrlen(const char *s)
{
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

static inline int kstreq(const char *s1, const char *s2)
{
    return kstrcmp(s1, s2) == 0;
}

static inline void kmemzero(void *ptr, size_t n)
{
    memset(ptr, 0, n);
}

#endif
