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

#include <stdint.h>
#include <stddef.h>
#include "../inc/usb.h"
#include "../inc/usb_core.h"

usb_endpoint_t *usb_ep_find(usb_device_t *d, uint8_t ep_addr)
{
    for (uint8_t i = 0; i < d->num_eps; i++)
        if (d->eps[i].active && d->eps[i].addr == ep_addr) return &d->eps[i];
    return NULL;
}

usb_endpoint_t *usb_ep_find_type(usb_device_t *d, uint8_t type, int dir_in)
{
    for (uint8_t i = 0; i < d->num_eps; i++) {
        usb_endpoint_t *ep = &d->eps[i];
        if (!ep->active || ep->type != type) continue;
        if (type == USB_EP_TYPE_CTRL) return ep;
        if (dir_in && USB_EP_IS_IN(ep->addr)) return ep;
        if (!dir_in && !USB_EP_IS_IN(ep->addr)) return ep;
    }
    return NULL;
}

int usb_ep_clear_halt(usb_device_t *d, uint8_t ep_addr)
{
    return usb_clear_halt(d, ep_addr);
}

uint8_t usb_ep_bulk_in_addr(usb_device_t *d)
{
    usb_endpoint_t *ep = usb_ep_find_type(d, USB_EP_TYPE_BULK, 1);
    return ep ? ep->addr : 0;
}

uint8_t usb_ep_bulk_out_addr(usb_device_t *d)
{
    usb_endpoint_t *ep = usb_ep_find_type(d, USB_EP_TYPE_BULK, 0);
    return ep ? ep->addr : 0;
}

uint8_t usb_ep_intr_in_addr(usb_device_t *d)
{
    usb_endpoint_t *ep = usb_ep_find_type(d, USB_EP_TYPE_INTR, 1);
    return ep ? ep->addr : 0;
}
