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

#ifndef USB_CORE_H
#define USB_CORE_H

#include <stdint.h>
#include <stddef.h>
#include "usb.h"
#include "xhci.h"

#define USB_MAX_ENDPOINTS 16
#define USB_MAX_DEVICES   128
#define USB_MAX_DRIVERS   32
#define USB_MAX_CFG_BUFSZ 4096

typedef struct {
    uint8_t  addr;
    uint8_t  type;
    uint16_t mps;
    uint8_t  interval;
    uint8_t  ep_id;
    uint8_t  active;
    uint8_t  _pad;
} usb_endpoint_t;

typedef struct usb_device {
    uint8_t   slot;
    uint8_t   addr;
    uint8_t   speed;
    uint8_t   port;
    uint8_t   config;
    uint8_t   iface;
    uint8_t   alt;
    uint8_t   depth;

    usb_dev_desc_t   dev_desc;
    uint8_t         *cfg_buf;
    uint16_t         cfg_len;

    usb_endpoint_t   eps[USB_MAX_ENDPOINTS];
    uint8_t          num_eps;

    xhci_ctrl_t     *xc;
    xhci_input_ctx_t *in_ctx;
    xhci_dev_ctx_t   *dev_ctx;

    struct usb_device *parent;
    uint8_t            parent_port;

    void *driver_data;
    int   active;
} usb_device_t;

typedef struct usb_driver {
    const char *name;
    int  (*probe)(usb_device_t *dev, const usb_iface_desc_t *iface,
                  const void *cfg_buf, uint16_t cfg_len);
    void (*disconnect)(usb_device_t *dev);
    void (*poll)(usb_device_t *dev);
} usb_driver_t;

int  usb_core_init(void);
void usb_core_poll(void);
void usb_driver_register(usb_driver_t *drv);

int  usb_control_in(usb_device_t *dev, const usb_setup_t *s,
                     void *buf, uint16_t len);
int  usb_control_out(usb_device_t *dev, const usb_setup_t *s,
                      const void *buf, uint16_t len);
int  usb_bulk_in(usb_device_t *dev, uint8_t ep_addr,
                  void *buf, uint32_t len);
int  usb_bulk_out(usb_device_t *dev, uint8_t ep_addr,
                   const void *buf, uint32_t len);
int  usb_intr_in(usb_device_t *dev, uint8_t ep_addr,
                  void *buf, uint32_t len);

int  usb_get_descriptor(usb_device_t *dev, uint8_t type, uint8_t idx,
                         uint16_t lang, void *buf, uint16_t len);
int  usb_set_configuration(usb_device_t *dev, uint8_t val);
int  usb_set_interface(usb_device_t *dev, uint8_t iface, uint8_t alt);
int  usb_clear_halt(usb_device_t *dev, uint8_t ep_addr);
int  usb_set_idle(usb_device_t *dev, uint8_t iface,
                   uint8_t report_id, uint8_t duration);
int  usb_set_protocol(usb_device_t *dev, uint8_t iface, uint8_t proto);
int  usb_get_report(usb_device_t *dev, uint8_t iface, uint8_t type,
                     uint8_t id, void *buf, uint16_t len);
int  usb_set_report(usb_device_t *dev, uint8_t iface, uint8_t type,
                     uint8_t id, const void *buf, uint16_t len);

usb_device_t *usb_get_device(uint8_t slot);
int           usb_device_count(void);

void usb_port_changed(xhci_ctrl_t *xc, uint8_t port, uint32_t portsc);

#endif
