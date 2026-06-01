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

#ifndef _KOBALT_IGC_OSDEP_H_
#define _KOBALT_IGC_OSDEP_H_

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

#define ASSERT(x)       do { if (!(x)) { for (;;) hlt(); } } while (0)
#define DEBUGOUT(...)   do {} while (0)
#define DEBUGOUT1(...)  do {} while (0)
#define DEBUGOUT2(...)  do {} while (0)
#define DEBUGOUT3(...)  do {} while (0)
#define DEBUGOUT7(...)  do {} while (0)
#ifndef DEBUGFUNC
#define DEBUGFUNC(f)    do {} while (0)
#endif

#define STATIC static

#define ASSERT_CTX_LOCK_HELD(hw) do {} while (0)

struct igc_osdep {
    pci_device_t *pdev;
};

/*
 * Kernel virt == phys for static allocations in Kobalt (mcmodel=large,
 * identity-mapped heap_pool). Safe to use as DMA address directly.
 */
#define IGC_DMA_ADDR(p)  ((u64)(uintptr_t)(p))

static __inline void igc_udelay(u32 us)
{
    for (volatile u32 i = 0; i < us * 21u; i++)
        cpu_relax();
}

static __inline void igc_mdelay(u32 ms)
{
    igc_udelay(ms * 1000u);
}

#define DELAY(n)           igc_udelay(n)
#ifndef usec_delay
#define usec_delay(x)      igc_udelay(x)
#endif
#ifndef usec_delay_irq
#define usec_delay_irq(x)  igc_udelay(x)
#endif
#ifndef msec_delay
#define msec_delay(x)      igc_mdelay(x)
#endif
#ifndef msec_delay_irq
#define msec_delay_irq(x)  igc_mdelay(x)
#endif

#ifndef IGC_STATUS
#define IGC_STATUS  0x00008u
#endif

#define IGC_WRITE_FLUSH(hw) \
    ((void)(*(volatile u32 *)((hw)->hw_addr + IGC_STATUS)))

#define IGC_READ_REG(hw, reg) \
    (*(volatile u32 *)((hw)->hw_addr + (reg)))

#define IGC_WRITE_REG(hw, reg, val) \
    (*(volatile u32 *)((hw)->hw_addr + (reg)) = (val))

#define IGC_READ_REG_ARRAY(hw, reg, idx) \
    (*(volatile u32 *)((hw)->hw_addr + (reg) + ((idx) << 2)))

#define IGC_WRITE_REG_ARRAY(hw, reg, idx, val) \
    (*(volatile u32 *)((hw)->hw_addr + (reg) + ((idx) << 2)) = (val))

#define IGC_READ_REG_ARRAY_DWORD   IGC_READ_REG_ARRAY
#define IGC_WRITE_REG_ARRAY_DWORD  IGC_WRITE_REG_ARRAY

#define IGC_READ_REG_ARRAY_BYTE(hw, reg, idx) \
    (*(volatile u8 *)((hw)->hw_addr + (reg) + (idx)))

#define IGC_WRITE_REG_ARRAY_BYTE(hw, reg, idx, val) \
    (*(volatile u8 *)((hw)->hw_addr + (reg) + (idx)) = (val))

#endif /* _KOBALT_IGC_OSDEP_H_ */