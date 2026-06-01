/* Copyright (c) 2026  Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef KOBALT_ARCH_CC_H
#define KOBALT_ARCH_CC_H

#include <stdint.h>
#include <stddef.h>

/* --- Hardware Purity --- */
#define LWIP_NO_STDINT_H      1
#define LWIP_NO_INTTYPES_H    1
#define LWIP_NO_CTYPE_H       1
#define BYTE_ORDER            LITTLE_ENDIAN

/* --- Kobalt Fixed-Width Types --- */
typedef uint8_t   u8_t;
typedef int8_t    s8_t;
typedef uint16_t  u16_t;
typedef int16_t   s16_t;
typedef uint32_t  u32_t;
typedef int32_t   s32_t;
typedef uint64_t  u64_t;
typedef int64_t   s64_t;
typedef uintptr_t mem_ptr_t;
typedef uintptr_t sys_prot_t;

/* --- THE TWO-BYTE FIX: STRUCTURE PACKING --- 
 * This prevents Clang from adding "slop" bytes between MAC addresses.
 * Without these, an Ethernet header is not 14 bytes!
 */
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END
#define PACK_STRUCT_STRUCT  __attribute__((packed))
#define PACK_STRUCT_FIELD(x) x

/* --- Hardware Doorbell Alignment --- */
#define ETH_PAD_SIZE          0   /* Ensure NO 2-byte padding shifts */

/* --- IO / Diagnostics --- */
void kputs(const char *s);
int  kprintf(const char *fmt, ...);
uint32_t sys_now(void);

#define LWIP_PLATFORM_DIAG(x)   do { kprintf x; } while(0)
#define LWIP_PLATFORM_ASSERT(x) do { kprintf("LWIP ASSERT: %s\n", x); while(1); } while(0)

/* --- POSIX errno variable and all constants (no libc available) ----------- */
extern int errno;   /* defined in sys_arch.c */

#define EPERM           1
#define ENOENT          2
#define ESRCH           3
#define EINTR           4
#define EIO             5
#define ENXIO           6
#define E2BIG           7
#define ENOEXEC         8
#define EBADF           9
#define ECHILD          10
#define EAGAIN          11
#define EWOULDBLOCK     11
#define ENOMEM          12
#define EACCES          13
#define EFAULT          14
#define ENOTBLK         15
#define EBUSY           16
#define EEXIST          17
#define EXDEV           18
#define ENODEV          19
#define ENOTDIR         20
#define EISDIR          21
#define EINVAL          22
#define ENFILE          23
#define EMFILE          24
#define ENOTTY          25
#define ETXTBSY         26
#define EFBIG           27
#define ENOSPC          28
#define ESPIPE          29
#define EROFS           30
#define EMLINK          31
#define EPIPE           32
#define EDOM            33
#define ERANGE          34
#define EDEADLK         35
#define ENAMETOOLONG    36
#define ENOLCK          37
#define ENOSYS          38
#define ENOTEMPTY       39
#define ELOOP           40
#define ENOMSG          42
#define EIDRM           43
#define ENOSTR          60
#define ENODATA         61
#define ETIME           62
#define ENOSR           63
#define EREMOTE         66
#define ENOLINK         67
#define EPROTO          71
#define EMULTIHOP       72
#define EBADMSG         74
#define EOVERFLOW       75
#define EILSEQ          84
#define EUSERS          87
#define ENOTSOCK        88
#define EDESTADDRREQ    89
#define EMSGSIZE        90
#define EPROTOTYPE      91
#define ENOPROTOOPT     92
#define EPROTONOSUPPORT 93
#define ESOCKTNOSUPPORT 94
#define EOPNOTSUPP      95
#define ENOTSUP         95
#define EAFNOSUPPORT    97
#define EADDRINUSE      98
#define EADDRNOTAVAIL   99
#define ENETDOWN        100
#define ENETUNREACH     101
#define ENETRESET       102
#define ECONNABORTED    103
#define ECONNRESET      104
#define ENOBUFS         105
#define EISCONN         106
#define ENOTCONN        107
#define ESHUTDOWN       108
#define ETOOMANYREFS    109
#define ETIMEDOUT       110
#define ECONNREFUSED    111
#define EHOSTDOWN       112
#define EHOSTUNREACH    113
#define EALREADY        114
#define EINPROGRESS     115
#define ESTALE          116
#define ECANCELED       125

#endif