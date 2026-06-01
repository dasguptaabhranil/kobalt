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

#ifndef __TYKID_USB_H__
#define __TYKID_USB_H__

#include "tykid_internal.h"

TYKID_INTERNAL void ty_usb_notify_device(tykid_gate_ctx_t *ctx,
                                          u8  usb_class,
                                          u8  usb_subclass,
                                          u8  usb_protocol,
                                          u16 vendor_id,
                                          u16 product_id);

TYKID_INTERNAL tykid_hwclass_t
ty_usb_hwclass(u8 cls, u8 sub, u8 proto);

TYKID_INTERNAL ty_hw_essential_mask_t
ty_usb_essential_bits(u8 cls, u8 sub, u8 proto);

TYKID_INTERNAL ty_hw_essential_mask_t
ty_usb_accumulate_essential(void);

TYKID_INTERNAL tykid_status_t
ty_usb_scan_class_drv(tykid_gate_ctx_t      *ctx,
                       tykid_driver_desc_t   *drv,
                       tykid_threat_report_t *report);

#endif
