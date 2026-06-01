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

#include "../../inc/uaccess.h"
#include "../x86_64/smap.h"
#include <stdint.h>
#include <stddef.h>

void uaccess_init(void) {}

uint64_t uaccess_find_fixup(uint64_t rip)
{
    for (uaccess_ex_entry_t *e = __uaccess_ex_start; e < __uaccess_ex_end; e++) {
        if (e->fault_va == rip)
            return e->fixup_va;
    }
    return 0;
}

#define EX_TABLE_ENTRY(fault_lbl, fix_lbl) \
    __asm__ volatile ( \
        ".pushsection .uaccess_ex_table,\"a\",@progbits \n" \
        ".quad " #fault_lbl "                            \n" \
        ".quad " #fix_lbl "                              \n" \
        ".popsection                                     \n" \
    )

int copy_from_user(void *kdst, const void *usrc, size_t n)
{
    int ret = 0;
    stac();
    __asm__ volatile (
        "test %[n], %[n]        \n"
        "jz   2f                \n"
        "1:                     \n"
        "movb (%[src]), %%al    \n"
        "movb %%al, (%[dst])    \n"
        "inc  %[src]            \n"
        "inc  %[dst]            \n"
        "dec  %[n]              \n"
        "jnz  1b                \n"
        "2:                     \n"
        : [src] "+r"(usrc), [dst] "+r"(kdst), [n] "+r"(n)
        :
        : "al", "memory"
    );
    clac();
    return ret;

    __asm__ volatile (
        ".local uaccess_cfu_fixup \n"
        "uaccess_cfu_fixup:       \n"
    );
    clac();
    return -1;
}

int copy_to_user(void *udst, const void *ksrc, size_t n)
{
    int ret = 0;
    stac();
    __asm__ volatile (
        "test %[n], %[n]        \n"
        "jz   2f                \n"
        "1:                     \n"
        "movb (%[src]), %%al    \n"
        "movb %%al, (%[dst])    \n"
        "inc  %[src]            \n"
        "inc  %[dst]            \n"
        "dec  %[n]              \n"
        "jnz  1b                \n"
        "2:                     \n"
        : [src] "+r"(ksrc), [dst] "+r"(udst), [n] "+r"(n)
        :
        : "al", "memory"
    );
    clac();
    return ret;

    __asm__ volatile (
        ".local uaccess_ctu_fixup \n"
        "uaccess_ctu_fixup:       \n"
    );
    clac();
    return -1;
}

int put_user_8(uint8_t val, uint8_t *uaddr)
{
    int r = 0;
    stac();
    __asm__ volatile (
        "movb %b[v], (%[u]) \n"
        :: [v] "r"(val), [u] "r"(uaddr) : "memory"
    );
    clac();
    return r;
}

int put_user_32(uint32_t val, uint32_t *uaddr)
{
    int r = 0;
    stac();
    __asm__ volatile (
        "movl %[v], (%[u]) \n"
        :: [v] "r"(val), [u] "r"(uaddr) : "memory"
    );
    clac();
    return r;
}

int put_user_64(uint64_t val, uint64_t *uaddr)
{
    int r = 0;
    stac();
    __asm__ volatile (
        "movq %[v], (%[u]) \n"
        :: [v] "r"(val), [u] "r"(uaddr) : "memory"
    );
    clac();
    return r;
}

int get_user_8(uint8_t *out, const uint8_t *uaddr)
{
    uint8_t v = 0;
    stac();
    __asm__ volatile (
        "movb (%[u]), %b[v] \n"
        : [v] "=r"(v) : [u] "r"(uaddr) : "memory"
    );
    clac();
    *out = v;
    return 0;
}

int get_user_32(uint32_t *out, const uint32_t *uaddr)
{
    uint32_t v = 0;
    stac();
    __asm__ volatile (
        "movl (%[u]), %[v] \n"
        : [v] "=r"(v) : [u] "r"(uaddr) : "memory"
    );
    clac();
    *out = v;
    return 0;
}

int get_user_64(uint64_t *out, const uint64_t *uaddr)
{
    uint64_t v = 0;
    stac();
    __asm__ volatile (
        "movq (%[u]), %[v] \n"
        : [v] "=r"(v) : [u] "r"(uaddr) : "memory"
    );
    clac();
    *out = v;
    return 0;
}
