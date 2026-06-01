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

#ifndef USB_H
#define USB_H

#include <stdint.h>
#include <stddef.h>

#define USB_SPEED_LOW        0
#define USB_SPEED_FULL       1
#define USB_SPEED_HIGH       2
#define USB_SPEED_SUPER      3
#define USB_SPEED_SUPER_PLUS 4

typedef struct __attribute__((packed)) {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_t;

#define USB_DIR_OUT       0x00
#define USB_DIR_IN        0x80
#define USB_TYPE_STANDARD (0 << 5)
#define USB_TYPE_CLASS    (1 << 5)
#define USB_TYPE_VENDOR   (2 << 5)
#define USB_RECIP_DEVICE  0
#define USB_RECIP_IFACE   1
#define USB_RECIP_ENDPOINT 2
#define USB_RECIP_OTHER   3

#define USB_REQ_GET_STATUS        0
#define USB_REQ_CLEAR_FEATURE     1
#define USB_REQ_SET_FEATURE       3
#define USB_REQ_SET_ADDRESS       5
#define USB_REQ_GET_DESCRIPTOR    6
#define USB_REQ_SET_DESCRIPTOR    7
#define USB_REQ_GET_CONFIGURATION 8
#define USB_REQ_SET_CONFIGURATION 9
#define USB_REQ_GET_INTERFACE     10
#define USB_REQ_SET_INTERFACE     11
#define USB_REQ_SYNCH_FRAME       12

#define USB_DESC_DEVICE           1
#define USB_DESC_CONFIGURATION    2
#define USB_DESC_STRING           3
#define USB_DESC_INTERFACE        4
#define USB_DESC_ENDPOINT         5
#define USB_DESC_INTERFACE_ASSOC  11
#define USB_DESC_BOS              15

typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} usb_dev_desc_t;

typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} usb_cfg_desc_t;

typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bInterfaceNumber;
    uint8_t  bAlternateSetting;
    uint8_t  bNumEndpoints;
    uint8_t  bInterfaceClass;
    uint8_t  bInterfaceSubClass;
    uint8_t  bInterfaceProtocol;
    uint8_t  iInterface;
} usb_iface_desc_t;

typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} usb_ep_desc_t;

typedef struct __attribute__((packed)) {
    uint8_t  bDescLength;
    uint8_t  bDescriptorType;
    uint8_t  bNbrPorts;
    uint16_t wHubCharacteristics;
    uint8_t  bPwrOn2PwrGood;
    uint8_t  bHubContrCurrent;
    uint8_t  bDeviceRemovable[4];
} usb_hub_desc_t;

typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bFirstInterface;
    uint8_t  bInterfaceCount;
    uint8_t  bFunctionClass;
    uint8_t  bFunctionSubClass;
    uint8_t  bFunctionProtocol;
    uint8_t  iFunction;
} usb_iad_desc_t;

#define USB_EP_NUM(a)     ((a) & 0x0F)
#define USB_EP_IS_IN(a)   (!!((a) & 0x80))
#define USB_EP_TYPE(attr) ((attr) & 0x03)
#define USB_EP_TYPE_CTRL  0
#define USB_EP_TYPE_ISOCH 1
#define USB_EP_TYPE_BULK  2
#define USB_EP_TYPE_INTR  3

#define USB_CLASS_PER_IFACE    0x00
#define USB_CLASS_AUDIO        0x01
#define USB_CLASS_CDC          0x02
#define USB_CLASS_HID          0x03
#define USB_CLASS_MASS_STORAGE 0x08
#define USB_CLASS_HUB          0x09
#define USB_CLASS_CDC_DATA     0x0A
#define USB_CLASS_WIRELESS     0xE0
#define USB_CLASS_MISC         0xEF
#define USB_CLASS_VENDOR       0xFF

#define USB_HID_SUBCLASS_NONE 0
#define USB_HID_SUBCLASS_BOOT 1
#define USB_HID_PROTO_NONE    0
#define USB_HID_PROTO_KEYBOARD 1
#define USB_HID_PROTO_MOUSE   2

#define USB_MSC_SUBCLASS_SCSI   0x06
#define USB_MSC_SUBCLASS_ATAPI  0x02
#define USB_MSC_PROTOCOL_BOT    0x50

#define USB_CDC_SUBCLASS_ACM     0x02
#define USB_CDC_SUBCLASS_ETHERNET 0x06
#define USB_WIRELESS_SUBCLASS_RF  0x01
#define USB_WIRELESS_PROTO_BT    0x01

#define USB_FEAT_ENDPOINT_HALT 0

static inline const void *usb_desc_next(const void *cur, const void *end)
{
    const uint8_t *p = (const uint8_t *)cur;
    if (p >= (const uint8_t *)end || p[0] < 2) return NULL;
    p += p[0];
    return p < (const uint8_t *)end ? p : NULL;
}

static inline const usb_iface_desc_t *
usb_find_iface(const void *cfg, uint16_t total, uint8_t num, uint8_t alt)
{
    const uint8_t *p = (const uint8_t *)cfg;
    const uint8_t *e = p + total;
    while (p < e && p[0] >= 2) {
        if (p[1] == USB_DESC_INTERFACE) {
            const usb_iface_desc_t *id = (const usb_iface_desc_t *)p;
            if (id->bInterfaceNumber == num && id->bAlternateSetting == alt)
                return id;
        }
        p += p[0];
    }
    return NULL;
}

static inline const usb_ep_desc_t *
usb_next_ep(const void *after, const void *end)
{
    const uint8_t *p = (const uint8_t *)after;
    p += p[0];
    while (p < (const uint8_t *)end && p[0] >= 2) {
        if (p[1] == USB_DESC_ENDPOINT) return (const usb_ep_desc_t *)p;
        if (p[1] == USB_DESC_INTERFACE) return NULL;
        p += p[0];
    }
    return NULL;
}

static inline void
usb_fill_setup(usb_setup_t *s, uint8_t bmrt, uint8_t req,
               uint16_t val, uint16_t idx, uint16_t len)
{
    s->bmRequestType = bmrt;
    s->bRequest = req;
    s->wValue   = val;
    s->wIndex   = idx;
    s->wLength  = len;
}

#endif
