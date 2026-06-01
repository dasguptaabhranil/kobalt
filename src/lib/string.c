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

#include <string.h>

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

char *strncpy(char * restrict dest, const char * restrict src, size_t n)
{
    char *d = dest;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dest;
}

size_t strlcat(char * restrict dest, const char * restrict src, size_t size)
{
    size_t dlen = strnlen(dest, size);
    size_t slen = strlen(src);
    if (dlen == size) return size + slen;
    size_t copy = slen < size - dlen - 1 ? slen : size - dlen - 1;
    memcpy(dest + dlen, src, copy);
    dest[dlen + copy] = '\0';
    return dlen + slen;
}
