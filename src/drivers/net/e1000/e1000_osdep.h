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

#ifndef _KOBALT_E1000_OSDEP_H_
#define _KOBALT_E1000_OSDEP_H_

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

#define __le16 u16
#define __le32 u32
#define __le64 u64

#define ASSERT(x)          do { if (!(x)) { for (;;) hlt(); } } while (0)
#define DEBUGOUT(...)      do {} while (0)
#define DEBUGOUT1(...)     do {} while (0)
#define DEBUGOUT2(...)     do {} while (0)
#define DEBUGOUT3(...)     do {} while (0)
#define DEBUGOUT7(...)     do {} while (0)
#define DEBUGFUNC(F)       do {} while (0)

#define STATIC             static
#define CMD_MEM_WRT_INVALIDATE  0x0010
#define PCI_COMMAND_REGISTER    PCI_CFG_COMMAND

#define ASSERT_CTX_LOCK_HELD(hw) do {} while (0)
#define ASSERT_NO_LOCKS()        do {} while (0)

static __inline void prefetch(void *x)
{
    __asm__ volatile("prefetcht0 %0" :: "m"(*(unsigned long *)x));
}

struct e1000_osdep {
    volatile uint8_t *flash_addr;
    pci_device_t     *pdev;
};

static __inline void e1000_outl(uint16_t port, uint32_t val)
{
    __asm__ volatile("outl %0, %w1" :: "a"(val), "Nd"(port) : "memory");
}

static __inline void e1000_udelay(uint32_t us)
{
    for (volatile uint32_t i = 0; i < us * 21u; i++)
        cpu_relax();
}

static __inline void e1000_mdelay(uint32_t ms)
{
    e1000_udelay(ms * 1000u);
}

#define usec_delay(x)      e1000_udelay(x)
#define usec_delay_irq(x)  e1000_udelay(x)
#define msec_delay(x)      e1000_mdelay(x)
#define msec_delay_irq(x)  e1000_mdelay(x)

#define E1000_REGISTER(hw, reg) \
    (((hw)->mac.type >= e1000_82543) ? (reg) : e1000_translate_register_82542(reg))

#define E1000_WRITE_FLUSH(hw) \
    E1000_READ_REG(hw, E1000_STATUS)

#define E1000_READ_OFFSET(hw, off) \
    (*(volatile u32 *)((hw)->hw_addr + (off)))

#define E1000_WRITE_OFFSET(hw, off, val) \
    (*(volatile u32 *)((hw)->hw_addr + (off)) = (val))

#define E1000_READ_REG(hw, reg) \
    (*(volatile u32 *)((hw)->hw_addr + E1000_REGISTER(hw, reg)))

#define E1000_WRITE_REG(hw, reg, val) \
    (*(volatile u32 *)((hw)->hw_addr + E1000_REGISTER(hw, reg)) = (val))

#define E1000_READ_REG_ARRAY(hw, reg, idx) \
    (*(volatile u32 *)((hw)->hw_addr + E1000_REGISTER(hw, reg) + ((idx) << 2)))

#define E1000_WRITE_REG_ARRAY(hw, reg, idx, val) \
    (*(volatile u32 *)((hw)->hw_addr + E1000_REGISTER(hw, reg) + ((idx) << 2)) = (val))

#define E1000_READ_REG_ARRAY_DWORD  E1000_READ_REG_ARRAY
#define E1000_WRITE_REG_ARRAY_DWORD E1000_WRITE_REG_ARRAY

#define E1000_READ_REG_ARRAY_BYTE(hw, reg, idx) \
    (*(volatile u8 *)((hw)->hw_addr + E1000_REGISTER(hw, reg) + (idx)))

#define E1000_WRITE_REG_ARRAY_BYTE(hw, reg, idx, val) \
    (*(volatile u8 *)((hw)->hw_addr + E1000_REGISTER(hw, reg) + (idx)) = (val))

#define E1000_WRITE_REG_ARRAY_WORD(hw, reg, idx, val) \
    (*(volatile u16 *)((hw)->hw_addr + E1000_REGISTER(hw, reg) + ((idx) << 1)) = (val))

#define E1000_WRITE_REG_IO(hw, reg, val) do { \
    e1000_outl((uint16_t)(hw)->io_base,       (u32)(reg)); \
    e1000_outl((uint16_t)((hw)->io_base + 4), (u32)(val)); \
} while (0)

#define E1000_READ_FLASH_REG(hw, reg) \
    (*(volatile u32 *)(((struct e1000_osdep *)(hw)->back)->flash_addr + (reg)))

#define E1000_READ_FLASH_REG16(hw, reg) \
    (*(volatile u16 *)(((struct e1000_osdep *)(hw)->back)->flash_addr + (reg)))

#define E1000_WRITE_FLASH_REG(hw, reg, val) \
    (*(volatile u32 *)(((struct e1000_osdep *)(hw)->back)->flash_addr + (reg)) = (val))

#define E1000_WRITE_FLASH_REG16(hw, reg, val) \
    (*(volatile u16 *)(((struct e1000_osdep *)(hw)->back)->flash_addr + (reg)) = (val))

#endif /* _KOBALT_E1000_OSDEP_H_ */