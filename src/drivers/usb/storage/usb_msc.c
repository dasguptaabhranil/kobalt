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
#include <string.h>
#include "../inc/usb.h"
#include "../inc/usb_core.h"
#include "../inc/kernel.h"
#include "../inc/kmalloc.h"
#include "../inc/blkdev.h"

#define CBW_SIG  0x43425355u
#define CSW_SIG  0x53425355u

typedef struct __attribute__((packed)) {
    uint32_t dCBWSignature;
    uint32_t dCBWTag;
    uint32_t dCBWDataTransferLength;
    uint8_t  bmCBWFlags;
    uint8_t  bCBWLUN;
    uint8_t  bCBWCBLength;
    uint8_t  CBWCB[16];
} bot_cbw_t;

typedef struct __attribute__((packed)) {
    uint32_t dCSWSignature;
    uint32_t dCSWTag;
    uint32_t dCSWDataResidue;
    uint8_t  bCSWStatus;
} bot_csw_t;

#define MSC_RESET   0xFF
#define MSC_MAX_LUN 0xFE

typedef struct {
    usb_device_t *dev;
    uint8_t  bulk_in, bulk_out, lun;
    uint32_t tag;
    uint64_t num_sectors;
    uint32_t sector_size;
    char     name[16];
    int      registered;
} msc_dev_t;

static int g_nmsc;

extern int usb_scsi_inquiry(void *msc);
extern int usb_scsi_read_capacity(void *msc);
extern int usb_scsi_read10(void *msc, uint64_t lba, uint16_t blks, void *buf);
extern int usb_scsi_write10(void *msc, uint64_t lba, uint16_t blks, const void *buf);

static void bot_reset(msc_dev_t *m)
{
    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_IFACE,
                   MSC_RESET, 0, m->dev->iface, 0);
    usb_control_out(m->dev, &s, NULL, 0);
    usb_clear_halt(m->dev, m->bulk_in);
    usb_clear_halt(m->dev, m->bulk_out);
}

int msc_bot_command(msc_dev_t *m, const uint8_t *cb, uint8_t cblen,
                     void *data, uint32_t dlen, int dir_in)
{
    bot_cbw_t cbw;
    memset(&cbw, 0, sizeof(cbw));
    cbw.dCBWSignature          = CBW_SIG;
    cbw.dCBWTag                = ++m->tag;
    cbw.dCBWDataTransferLength = dlen;
    cbw.bmCBWFlags             = dir_in ? 0x80 : 0x00;
    cbw.bCBWLUN                = m->lun;
    cbw.bCBWCBLength           = cblen;
    memcpy(cbw.CBWCB, cb, cblen);

    if (usb_bulk_out(m->dev, m->bulk_out, &cbw, sizeof(cbw)) < 0) {
        bot_reset(m); return -1;
    }

    if (dlen && data) {
        int r = dir_in ? usb_bulk_in(m->dev, m->bulk_in, data, dlen)
                       : usb_bulk_out(m->dev, m->bulk_out, data, dlen);
        if (r < 0)
            usb_clear_halt(m->dev, dir_in ? m->bulk_in : m->bulk_out);
    }

    bot_csw_t csw;
    memset(&csw, 0, sizeof(csw));
    if (usb_bulk_in(m->dev, m->bulk_in, &csw, sizeof(csw)) < 0) {
        bot_reset(m); return -1;
    }
    if (csw.dCSWSignature != CSW_SIG || csw.dCSWTag != cbw.dCBWTag) {
        bot_reset(m); return -1;
    }
    if (csw.bCSWStatus == 2) { bot_reset(m); return -1; }
    return csw.bCSWStatus == 0 ? 0 : -1;
}

static int msc_read(void *ctx, uint64_t lba, uint32_t cnt, void *buf)
{
    msc_dev_t *m = (msc_dev_t *)ctx;
    uint8_t *p = buf;
    while (cnt) {
        uint16_t n = cnt > 65535u ? 65535u : (uint16_t)cnt;
        if (usb_scsi_read10(m, lba, n, p) < 0) return -1;
        p += (uint64_t)n * m->sector_size;
        lba += n; cnt -= n;
    }
    return 0;
}

static int msc_write(void *ctx, uint64_t lba, uint32_t cnt, const void *buf)
{
    msc_dev_t *m = (msc_dev_t *)ctx;
    const uint8_t *p = buf;
    while (cnt) {
        uint16_t n = cnt > 65535u ? 65535u : (uint16_t)cnt;
        if (usb_scsi_write10(m, lba, n, p) < 0) return -1;
        p += (uint64_t)n * m->sector_size;
        lba += n; cnt -= n;
    }
    return 0;
}

static int msc_probe(usb_device_t *d, const usb_iface_desc_t *iface,
                      const void *cfg, uint16_t clen)
{
    if (iface->bInterfaceClass    != USB_CLASS_MASS_STORAGE ||
        iface->bInterfaceProtocol != USB_MSC_PROTOCOL_BOT)
        return 0;
    if (iface->bInterfaceSubClass != USB_MSC_SUBCLASS_SCSI &&
        iface->bInterfaceSubClass != USB_MSC_SUBCLASS_ATAPI)
        return 0;

    msc_dev_t *m = kmalloc(sizeof(*m));
    if (!m) return 0;
    memset(m, 0, sizeof(*m));
    m->dev = d;

    const uint8_t *end = (const uint8_t *)cfg + clen;
    const usb_ep_desc_t *ep = (const usb_ep_desc_t *)iface;
    for (uint8_t i = 0; i < iface->bNumEndpoints; i++) {
        ep = usb_next_ep(ep, end);
        if (!ep || USB_EP_TYPE(ep->bmAttributes) != USB_EP_TYPE_BULK) continue;
        if (USB_EP_IS_IN(ep->bEndpointAddress)) m->bulk_in  = ep->bEndpointAddress;
        else                                     m->bulk_out = ep->bEndpointAddress;
    }
    if (!m->bulk_in || !m->bulk_out) { kfree(m); return 0; }

    usb_setup_t s;
    usb_fill_setup(&s, USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_IFACE,
                   MSC_MAX_LUN, 0, iface->bInterfaceNumber, 1);
    uint8_t max_lun = 0;
    usb_control_in(d, &s, &max_lun, 1);
    m->lun = 0;

    d->iface = iface->bInterfaceNumber;
    d->driver_data = m;

    if (usb_scsi_inquiry(m)       < 0) klog_warn("usb_msc", "SCSI inquiry failed");
    if (usb_scsi_read_capacity(m) < 0) {
        klog_warn("usb_msc", "read capacity failed");
        m->sector_size = 512;
    }

    ksnprintf(m->name, sizeof(m->name), "usb%d", g_nmsc++);

    int r = blkdev_register(m->name, m, msc_read, msc_write,
                             m->num_sectors, m->sector_size);
    if (r < 0) { klog_warn("usb_msc", "blkdev_register failed"); }
    else { m->registered = 1; }

    char msg[64];
    ksnprintf(msg, sizeof(msg), "drive '%s': %llu sectors x %u = %llu MiB",
              m->name,
              (unsigned long long)m->num_sectors,
              m->sector_size,
              (unsigned long long)(m->num_sectors * m->sector_size / (1024u*1024u)));
    klog_ok("usb_msc", msg);
    return 1;
}

static void msc_disc(usb_device_t *d)
{
    if (d->driver_data) { kfree(d->driver_data); d->driver_data = NULL; }
}

static usb_driver_t msc_drv = {
    .name = "usb_msc", .probe = msc_probe, .disconnect = msc_disc, .poll = NULL,
};

void usb_msc_init(void)
{
    usb_driver_register(&msc_drv);
    klog_ok("usb_msc", "Mass Storage driver registered");
}
