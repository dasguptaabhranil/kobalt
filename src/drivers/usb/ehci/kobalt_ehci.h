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

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "../../../inc/pci.h"
#include "ehcireg.h"

#define EHCI_MAX_PORTS  15

typedef volatile uint32_t *ehci_opreg_t;

typedef struct ehci_qtd ehci_qtd_t;
typedef struct ehci_qh  ehci_qh_t;

typedef struct kobalt_ehci {
    pci_device_t *pdev;
    uintptr_t     cap_base;
    ehci_opreg_t  op;
    uint8_t       n_ports;
    ehci_qh_t    *qh_head;
    uint32_t     *frame_list;
    int           vec;
} kobalt_ehci_t;

typedef struct {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed)) ehci_setup_t;

int  kobalt_ehci_init(pci_device_t *pdev, kobalt_ehci_t *hc);
int  kobalt_ehci_control(kobalt_ehci_t *hc, uint8_t addr, uint8_t ep,
                         uint16_t mps, ehci_setup_t *setup,
                         void *data, uint16_t len, int dir_in);
void kobalt_ehci_poll(kobalt_ehci_t *hc);
void kobalt_ehci_irq(kobalt_ehci_t *hc);
