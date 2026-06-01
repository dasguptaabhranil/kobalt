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

#ifndef USB_MSC_INTERNAL_H
#define USB_MSC_INTERNAL_H

#include <stdint.h>
#include "../inc/usb_core.h"
#include "../../../../inc/blkdev.h"

typedef struct {
    usb_device_t *dev;
    uint8_t  bulk_in;
    uint8_t  bulk_out;
    uint8_t  lun;
    uint32_t tag;
    uint64_t num_sectors;
    uint32_t sector_size;
    blkdev_t blkdev;
    char     name[16];
    int      registered;
} msc_dev_t;

int msc_bot_command(msc_dev_t *msc, const uint8_t *cb, uint8_t cb_len,
                     void *data, uint32_t data_len, int dir_in);

int usb_scsi_inquiry(msc_dev_t *msc);
int usb_scsi_read_capacity(msc_dev_t *msc);
int usb_scsi_read10(msc_dev_t *msc, uint64_t lba, uint16_t blocks, void *buf);
int usb_scsi_write10(msc_dev_t *msc, uint64_t lba, uint16_t blocks, const void *buf);
int usb_scsi_sync_cache(msc_dev_t *msc);

#endif
