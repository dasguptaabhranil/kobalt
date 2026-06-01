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
#include "ixgbe_type.h"
#include "ixgbe_osdep.h"
#include "../../../inc/pci.h"

#define IXGBE_NUM_TX_DESC  256
#define IXGBE_NUM_RX_DESC  256
#define IXGBE_RX_BUF_SIZE  2048

typedef struct {
    union ixgbe_adv_tx_desc *ring;
    void                    *raw;
    void                    *bufs[IXGBE_NUM_TX_DESC];
    uint16_t                 head, tail;
} ixgbe_txq_t;

typedef struct {
    union ixgbe_adv_rx_desc *ring;
    void                    *raw;
    uint8_t                 *bufs[IXGBE_NUM_RX_DESC];
    uint16_t                 head, tail;
} ixgbe_rxq_t;

typedef struct kobalt_ixgbe {
    pci_device_t      *pdev;
    struct ixgbe_hw    hw;
    struct ixgbe_osdep osdep;
    ixgbe_txq_t        tx;
    ixgbe_rxq_t        rx;
    uint8_t            mac[6];
    int                vec_tx, vec_rx, vec_lsc;
    int                up;
} kobalt_ixgbe_t;

int  kobalt_ixgbe_init(pci_device_t *pdev, kobalt_ixgbe_t *nic);
int  kobalt_ixgbe_send(kobalt_ixgbe_t *nic, const void *buf, uint16_t len);
int  kobalt_ixgbe_poll(kobalt_ixgbe_t *nic, void *buf, uint16_t *out_len);
void kobalt_ixgbe_link_update(kobalt_ixgbe_t *nic);
