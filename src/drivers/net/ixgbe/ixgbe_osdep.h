/* Copyright (C) 2026 Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _KOBALT_IXGBE_OSDEP_H_
#define _KOBALT_IXGBE_OSDEP_H_

#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-function"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <kernel.h>
#include <pci.h>
#include "../../../inc/kmalloc.h"

#ifndef ETHER_ADDR_LEN
#define ETHER_ADDR_LEN 6
#endif

typedef unsigned long long u64;
typedef uint32_t           u32;
typedef uint16_t           u16;
typedef uint8_t            u8;
typedef signed long long   s64;
typedef int32_t            s32;
typedef int16_t            s16;
typedef int8_t             s8;

typedef u16 __le16;
typedef u32 __le32;
typedef u64 __le64;
typedef u16 __be16;
typedef u32 __be32;
typedef u64 __be64;

#define le16_to_cpu(x)  (x)

#define IXGBE_INTEL_VENDOR_ID   0x8086

#define ASSERT(x)        do { if (!(x)) { for (;;) hlt(); } } while (0)
#define EWARN(hw, msg)   do {} while (0)
#define MSGOUT(s, a, b)  do {} while (0)

#define DEBUGOUT(...)    do {} while (0)
#define DEBUGOUT1(...)   do {} while (0)
#define DEBUGOUT2(...)   do {} while (0)
#define DEBUGOUT3(...)   do {} while (0)
#define DEBUGOUT6(...)   do {} while (0)
#define DEBUGOUT7(...)   do {} while (0)
#ifndef DEBUGFUNC
#define DEBUGFUNC(f)     do {} while (0)
#endif

#define STATIC static

#define ASSERT_CTX_LOCK_HELD(hw) do {} while (0)

typedef enum {
    IXGBE_ERROR_SOFTWARE = 0,
    IXGBE_ERROR_POLLING,
    IXGBE_ERROR_INVALID_STATE,
    IXGBE_ERROR_UNSUPPORTED,
    IXGBE_ERROR_ARGUMENT,
    IXGBE_ERROR_CAUTION,
} ixgbe_error_type;

#define ERROR_REPORT1(t, fmt)        do {} while (0)
#define ERROR_REPORT2(t, fmt, a)     do {} while (0)
#define ERROR_REPORT3(t, fmt, a, b)  do {} while (0)

#define UNREFERENCED_PARAMETER(a)         (void)(a)
#define UNREFERENCED_1PARAMETER(a)        (void)(a)
#define UNREFERENCED_2PARAMETER(a,b)      do { (void)(a); (void)(b); } while (0)
#define UNREFERENCED_3PARAMETER(a,b,c)    do { (void)(a); (void)(b); (void)(c); } while (0)
#define UNREFERENCED_4PARAMETER(a,b,c,d)  do { (void)(a); (void)(b); (void)(c); (void)(d); } while (0)

#define IXGBE_CPU_TO_LE16(x)    (x)
#define IXGBE_CPU_TO_LE32(x)    (x)
#define IXGBE_LE16_TO_CPU(x)    (x)
#define IXGBE_LE32_TO_CPU(x)    (x)
#define IXGBE_LE64_TO_CPU(x)    (x)
#define IXGBE_LE32_TO_CPUS(x)   do {} while (0)
#define IXGBE_CPU_TO_BE16(x)    __builtin_bswap16(x)
#define IXGBE_CPU_TO_BE32(x)    __builtin_bswap32(x)
#define IXGBE_BE32_TO_CPU(x)    __builtin_bswap32(x)
#define IXGBE_NTOHL(x)          __builtin_bswap32(x)
#define IXGBE_NTOHS(x)          __builtin_bswap16(x)

#ifndef min
#define min(a, b)  ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b); _a < _b ? _a : _b; })
#endif
#ifndef max
#define max(a, b)  ({ __typeof__(a) _a = (a); __typeof__(b) _b = (b); _a > _b ? _a : _b; })
#endif

#ifndef CMD_MEM_WRT_INVALIDATE
#define CMD_MEM_WRT_INVALIDATE  0x0010
#endif
#ifndef PCI_COMMAND_REGISTER
#define PCI_COMMAND_REGISTER    0x04
#endif

struct ixgbe_lock {
    volatile int locked;
};

struct ixgbe_osdep {
    pci_device_t *pdev;
};

#define IXGBE_DMA_ADDR(p)  ((u64)(uintptr_t)(p))

static __inline void ixgbe_udelay(u32 us)
{
    for (volatile u32 i = 0; i < us * 21u; i++)
        cpu_relax();
}

static __inline void ixgbe_mdelay(u32 ms)
{
    ixgbe_udelay(ms * 1000u);
}

#ifndef DELAY
#define DELAY(n)             ixgbe_udelay(n)
#endif
#ifndef usec_delay
#define usec_delay(x)        ixgbe_udelay(x)
#endif
#ifndef usec_delay_irq
#define usec_delay_irq(x)    ixgbe_udelay(x)
#endif
#ifndef msec_delay
#define msec_delay(x)        ixgbe_mdelay(x)
#endif
#ifndef msec_delay_irq
#define msec_delay_irq(x)    ixgbe_mdelay(x)
#endif

#define IXGBE_WRITE_FLUSH(hw) \
    ((void)(*(volatile u32 *)((hw)->hw_addr + IXGBE_STATUS)))

#define IXGBE_READ_REG(hw, reg) \
    (*(volatile u32 *)((hw)->hw_addr + (reg)))

#define IXGBE_WRITE_REG(hw, reg, val) \
    (*(volatile u32 *)((hw)->hw_addr + (reg)) = (val))

#define IXGBE_READ_REG_ARRAY(hw, reg, idx) \
    (*(volatile u32 *)((hw)->hw_addr + (reg) + ((idx) << 2)))

#define IXGBE_WRITE_REG_ARRAY(hw, reg, idx, val) \
    (*(volatile u32 *)((hw)->hw_addr + (reg) + ((idx) << 2)) = (val))

struct ixgbe_hw;

void ixgbe_read_pci_cfg(struct ixgbe_hw *hw, u32 reg, u16 *value);
void ixgbe_write_pci_cfg(struct ixgbe_hw *hw, u32 reg, u16 *value);
s32  ixgbe_read_pcie_cap_reg(struct ixgbe_hw *hw, u32 reg, u16 *value);
s32  ixgbe_write_pcie_cap_reg(struct ixgbe_hw *hw, u32 reg, u16 *value);

#define IXGBE_READ_PCIE_WORD(hw, reg) \
    ({ u16 _v = 0; ixgbe_read_pcie_cap_reg((hw), (reg), &_v); _v; })
#define IXGBE_WRITE_PCIE_WORD(hw, reg, val) \
    do { u16 _v = (val); ixgbe_write_pcie_cap_reg((hw), (reg), &_v); } while (0)

void ixgbe_init_lock(struct ixgbe_lock *);
void ixgbe_acquire_lock(struct ixgbe_lock *);
void ixgbe_release_lock(struct ixgbe_lock *);
void ixgbe_destroy_lock(struct ixgbe_lock *);

static inline void *ixgbe_calloc(struct ixgbe_hw *hw, size_t n, size_t sz)
{
    (void)hw;
    void *p = kmalloc(n * sz);
    if (p) memset(p, 0, n * sz);
    return p;
}

static inline void *ixgbe_malloc(struct ixgbe_hw *hw, size_t sz)
{
    (void)hw;
    return kmalloc(sz);
}

static inline void ixgbe_free(struct ixgbe_hw *hw, void *p)
{
    (void)hw;
    kfree(p);
}

void ixgbe_info_fwlog(struct ixgbe_hw *hw, uint32_t rowsize,
                      uint32_t groupsize, uint8_t *buf, size_t len);

#endif /* _KOBALT_IXGBE_OSDEP_H_ */