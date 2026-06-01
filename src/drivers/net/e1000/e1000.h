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

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "e1000_osdep.h"
#include "e1000_hw.h"
#include <pci.h>
#include <spinlock.h>
#include <../net/lwip/include/lwip/netif.h>

#define E1000_NUM_TX_DESC  256
#define E1000_NUM_RX_DESC  256
#define E1000_BUF_SIZE     2048

struct e1000_legacy_tx_desc {
    uint64_t buf_addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed));

struct e1000_legacy_rx_desc {
    uint64_t buf_addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed));

typedef struct {
    struct e1000_hw      hw;
    struct e1000_osdep   osdep;
    pci_device_t        *pdev;

    struct e1000_legacy_tx_desc *tx_ring;
    uint8_t  *tx_bufs[E1000_NUM_TX_DESC];
    uint32_t  tx_head;
    uint32_t  tx_tail;

    struct e1000_legacy_rx_desc *rx_ring;
    uint8_t  *rx_bufs[E1000_NUM_RX_DESC];
    uint32_t  rx_tail;

    spinlock_t lock;
    struct netif netif;
} e1000_adapter_t;

void         e1000_init(void);
int          e1000_present(void);
void         e1000_poll(void);
struct netif *e1000_get_netif(void);
void         e1000_irq_handler(uint8_t irq, void *arg);