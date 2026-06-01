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

#include "tykid_internal.h"
#include "tykid_usb.h"

#include "usb.h"

#define TYKID_USB_MAX_OBSERVED  32U

typedef struct TYKID_PACKED {
    u8               usb_class;
    u8               usb_subclass;
    u8               usb_protocol;
    u16              vendor_id;
    u16              product_id;
    tykid_hwclass_t  hw_class;
 u32 essential_bits;
} ty_usb_observed_dev_t;

static ty_usb_observed_dev_t g_usb_devs[TYKID_USB_MAX_OBSERVED];
static u32                   g_usb_dev_count = 0;
static ty_spinlock_t         g_usb_lock = TY_SPIN_INIT;

#define UAC_SUBCLASS_STREAMING  0x02
#define UAC_SUBCLASS_MIDI       0x03

static const struct {
    u8              usb_class;
 u8 usb_subclass;
 u8 usb_protocol;
    tykid_hwclass_t hw_class;
} g_usb_class_map[] = {

    { USB_CLASS_HID,  USB_HID_SUBCLASS_BOOT, USB_HID_PROTO_KEYBOARD,
                                                  TYKID_HW_INPUT_KBD   },
    { USB_CLASS_HID,  USB_HID_SUBCLASS_BOOT, USB_HID_PROTO_MOUSE,
                                                  TYKID_HW_INPUT_MOUSE  },
    { USB_CLASS_HID,  0, USB_HID_PROTO_KEYBOARD,  TYKID_HW_INPUT_KBD   },
    { USB_CLASS_HID,  0, USB_HID_PROTO_MOUSE,     TYKID_HW_INPUT_MOUSE  },
    { USB_CLASS_HID,  0, 0,                       TYKID_HW_INPUT_KBD   },

    { USB_CLASS_MASS_STORAGE, USB_MSC_SUBCLASS_SCSI,  USB_MSC_PROTOCOL_BOT,
                                                  TYKID_HW_CLASS_STORAGE_USB },
    { USB_CLASS_MASS_STORAGE, USB_MSC_SUBCLASS_ATAPI, USB_MSC_PROTOCOL_BOT,
                                                  TYKID_HW_CLASS_STORAGE_USB },
    { USB_CLASS_MASS_STORAGE, 0, USB_MSC_PROTOCOL_BOT,
                                                  TYKID_HW_CLASS_STORAGE_USB },

    { USB_CLASS_AUDIO, UAC_SUBCLASS_STREAMING, 0, TYKID_HW_AUDIO_USB  },
    { USB_CLASS_AUDIO, UAC_SUBCLASS_MIDI,      0, TYKID_HW_AUDIO_USB  },
    { USB_CLASS_AUDIO, 0,                      0, TYKID_HW_AUDIO_USB  },

    { USB_CLASS_CDC,  USB_CDC_SUBCLASS_ETHERNET, 0, TYKID_HW_NIC_GENERIC },
    { USB_CLASS_CDC,  USB_CDC_SUBCLASS_ACM,      0, TYKID_HW_UNKNOWN     },

    { USB_CLASS_MISC, 4, 1,                      TYKID_HW_NIC_GENERIC },

    { USB_CLASS_HUB,  0, 0,                      TYKID_HW_USB_XHCI    },

    { USB_CLASS_WIRELESS, USB_WIRELESS_SUBCLASS_RF, USB_WIRELESS_PROTO_BT,
                                                  TYKID_HW_UNKNOWN     },

    { USB_CLASS_VENDOR, 0, 0,                    TYKID_HW_UNKNOWN     },
};

#define USB_CLASS_MAP_COUNT \
    (sizeof(g_usb_class_map) / sizeof(g_usb_class_map[0]))

TYKID_INTERNAL tykid_hwclass_t
ty_usb_hwclass(u8 cls, u8 sub, u8 proto)
{
    for (usz i = 0; i < USB_CLASS_MAP_COUNT; i++) {
        if (g_usb_class_map[i].usb_class != cls) continue;
        if (g_usb_class_map[i].usb_subclass != 0 &&
            g_usb_class_map[i].usb_subclass != sub)  continue;
        if (g_usb_class_map[i].usb_protocol != 0 &&
            g_usb_class_map[i].usb_protocol != proto) continue;
        return g_usb_class_map[i].hw_class;
    }
    return TYKID_HW_UNKNOWN;
}

TYKID_INTERNAL ty_hw_essential_mask_t
ty_usb_essential_bits(u8 cls, u8 sub, u8 proto)
{

    if (cls == USB_CLASS_HID) {
        if (proto == USB_HID_PROTO_KEYBOARD) return TY_ESSENTIAL_KBD;
        if (sub  == USB_HID_SUBCLASS_BOOT &&
            proto == USB_HID_PROTO_KEYBOARD) return TY_ESSENTIAL_KBD;

        if (proto == 0 && sub == 0)          return TY_ESSENTIAL_KBD;
        return TY_ESSENTIAL_NONE;
    }

    if (cls == USB_CLASS_MASS_STORAGE && proto == USB_MSC_PROTOCOL_BOT)
        return TY_ESSENTIAL_STORAGE_SATA;

    if (cls == USB_CLASS_CDC && sub == USB_CDC_SUBCLASS_ETHERNET)
        return TY_ESSENTIAL_NIC;
 if (cls == USB_CLASS_MISC && sub == 4 && proto == 1)
        return TY_ESSENTIAL_NIC;

    if (cls == USB_CLASS_HUB)
        return TY_ESSENTIAL_USB_HOST;

    return TY_ESSENTIAL_NONE;
}

TYKID_INTERNAL void
ty_usb_notify_device(tykid_gate_ctx_t *ctx,
                     u8  usb_class,
                     u8  usb_subclass,
                     u8  usb_protocol,
                     u16 vendor_id,
                     u16 product_id)
{
    if (!ctx) return;

    ty_spin_lock(&g_usb_lock);

    if (g_usb_dev_count >= TYKID_USB_MAX_OBSERVED) {
        ty_spin_unlock(&g_usb_lock);
        TY_LOG(ctx, TY_LOG_WARN,
               "USB observed table full (%u entries) — device %04x:%04x dropped",
               TYKID_USB_MAX_OBSERVED, vendor_id, product_id);
        return;
    }

    for (u32 i = 0; i < g_usb_dev_count; i++) {
        if (g_usb_devs[i].usb_class    == usb_class    &&
            g_usb_devs[i].usb_subclass == usb_subclass &&
            g_usb_devs[i].usb_protocol == usb_protocol) {
            ty_spin_unlock(&g_usb_lock);
            return;
        }
    }

    ty_usb_observed_dev_t *d = &g_usb_devs[g_usb_dev_count++];
    d->usb_class      = usb_class;
    d->usb_subclass   = usb_subclass;
    d->usb_protocol   = usb_protocol;
    d->vendor_id      = vendor_id;
    d->product_id     = product_id;
    d->hw_class       = ty_usb_hwclass(usb_class, usb_subclass, usb_protocol);
    d->essential_bits = ty_usb_essential_bits(usb_class, usb_subclass, usb_protocol);

    ctx->essential_mask |= d->essential_bits;

    ty_spin_unlock(&g_usb_lock);

    TY_LOG(ctx, TY_LOG_INFO,
           "USB device: class=0x%02x sub=0x%02x proto=0x%02x "
           "vid=%04x pid=%04x hwclass=0x%08x essential=0x%08x",
           usb_class, usb_subclass, usb_protocol,
           vendor_id, product_id, d->hw_class, d->essential_bits);
}

TYKID_INTERNAL ty_hw_essential_mask_t
ty_usb_accumulate_essential(void)
{
    ty_hw_essential_mask_t mask = TY_ESSENTIAL_NONE;
    for (u32 i = 0; i < g_usb_dev_count; i++)
        mask |= g_usb_devs[i].essential_bits;
    return mask;
}

typedef struct {
 const char *name_prefix;
 u8 usb_class;
 u8 usb_subclass;
 u8 usb_protocol;
 u32 essential_gate;
} ty_usb_class_guard_t;

static const ty_usb_class_guard_t g_usb_guards[] = {

    { "xhci",        0,                     0, 0, TY_ESSENTIAL_USB_HOST  },
    { "usb_ep",      0,                     0, 0, TY_ESSENTIAL_USB_HOST  },
    { "usb_core",    0,                     0, 0, TY_ESSENTIAL_USB_HOST  },

    { "usb_hub",     USB_CLASS_HUB,         0, 0, TY_ESSENTIAL_USB_HOST  },

    { "hid_kbd",     USB_CLASS_HID, USB_HID_SUBCLASS_BOOT,
                                   USB_HID_PROTO_KEYBOARD, TY_ESSENTIAL_KBD },
    { "hid_mouse",   USB_CLASS_HID, USB_HID_SUBCLASS_BOOT,
                                   USB_HID_PROTO_MOUSE,    TY_ESSENTIAL_NONE },
    { "hid_tablet",  USB_CLASS_HID, 0, 0,                  TY_ESSENTIAL_NONE },
    { "hid",         USB_CLASS_HID, 0, 0,                  TY_ESSENTIAL_NONE },

    { "usb_msc",     USB_CLASS_MASS_STORAGE, 0,
                                   USB_MSC_PROTOCOL_BOT,  TY_ESSENTIAL_USB_HOST },
    { "usb_scsi",    USB_CLASS_MASS_STORAGE, 0,
                                   USB_MSC_PROTOCOL_BOT,  TY_ESSENTIAL_USB_HOST },

    { "usb_audio",   USB_CLASS_AUDIO, UAC_SUBCLASS_STREAMING,
                                   0,                     TY_ESSENTIAL_NONE },
    { "usb_midi",    USB_CLASS_AUDIO, UAC_SUBCLASS_MIDI,
                                   0,                     TY_ESSENTIAL_NONE },

    { "usb_cdc",     USB_CLASS_CDC, USB_CDC_SUBCLASS_ETHERNET,
                                   0,                     TY_ESSENTIAL_NIC  },
    { "usb_rndis",   USB_CLASS_MISC, 4, 1,                TY_ESSENTIAL_NIC  },

    { "usb_cdc_acm", USB_CLASS_CDC, USB_CDC_SUBCLASS_ACM,
                                   0,                     TY_ESSENTIAL_NONE },
    { "usb_ftdi",    USB_CLASS_VENDOR, 0, 0,              TY_ESSENTIAL_NONE },
    { "usb_cp210x",  USB_CLASS_VENDOR, 0, 0,              TY_ESSENTIAL_NONE },

    { "btusb",       USB_CLASS_WIRELESS, USB_WIRELESS_SUBCLASS_RF,
                                   USB_WIRELESS_PROTO_BT, TY_ESSENTIAL_NONE },
    { "bt_hci",      USB_CLASS_WIRELESS, USB_WIRELESS_SUBCLASS_RF,
                                   USB_WIRELESS_PROTO_BT, TY_ESSENTIAL_NONE },
};

#define USB_GUARD_COUNT \
    (sizeof(g_usb_guards) / sizeof(g_usb_guards[0]))

static TYKID_ALWAYS_INL bool8
ty_usb_name_starts_with(const char *name, const char *prefix)
{
    while (*prefix) {
        if (*name++ != *prefix++) return TYKID_FALSE;
    }

    return TYKID_TRUE;
}

static TYKID_NOINLINE bool8
ty_usb_class_observed(u8 cls, u8 sub, u8 proto)
{
    for (u32 i = 0; i < g_usb_dev_count; i++) {
        const ty_usb_observed_dev_t *d = &g_usb_devs[i];
        if (d->usb_class != cls)                      continue;
        if (sub   != 0 && d->usb_subclass  != sub)   continue;
        if (proto != 0 && d->usb_protocol  != proto)  continue;
        return TYKID_TRUE;
    }
    return TYKID_FALSE;
}

TYKID_INTERNAL tykid_status_t
ty_usb_scan_class_drv(tykid_gate_ctx_t      *ctx,
                       tykid_driver_desc_t   *drv,
                       tykid_threat_report_t *report)
{
    if (!ctx || !drv || !report) return TYKID_ERR_NULL_PTR;

    const ty_usb_class_guard_t *guard = NULL;
    for (usz i = 0; i < USB_GUARD_COUNT; i++) {
        if (ty_usb_name_starts_with(drv->name, g_usb_guards[i].name_prefix)) {
            guard = &g_usb_guards[i];
            break;
        }
    }

    if (!guard) return TYKID_OK;

    if (guard->usb_class == 0) {
        if (!(ctx->essential_mask & TY_ESSENTIAL_USB_HOST)) {
            TY_LOG(ctx, TY_LOG_WARN,
                   "USB infra driver '%s': TY_ESSENTIAL_USB_HOST not set "
                   "— no xHCI detected, marking suspicious",
                   drv->name);
            if (report->threat_class < TY_THREAT_SUSPICIOUS)
                report->threat_class = TY_THREAT_SUSPICIOUS;
            drv->state = TYKID_DRV_STATE_SKIPPED;
            return TYKID_ERR_NOT_ESSENTIAL;
        }
        TY_LOG(ctx, TY_LOG_DEBUG,
               "USB infra driver '%s': infrastructure check passed", drv->name);
        return TYKID_OK;
    }

    bool8 seen = ty_usb_class_observed(guard->usb_class,
                                        guard->usb_subclass,
                                        guard->usb_protocol);
    if (!seen) {

        TY_LOG(ctx, TY_LOG_INFO,
               "USB driver '%s': class 0x%02x sub=0x%02x proto=0x%02x "
               "not observed — driver skipped",
               drv->name,
               guard->usb_class, guard->usb_subclass, guard->usb_protocol);

        if (report->threat_class < TY_THREAT_SUSPICIOUS)
            report->threat_class = TY_THREAT_SUSPICIOUS;
        drv->state = TYKID_DRV_STATE_SKIPPED;
        return TYKID_ERR_NOT_ESSENTIAL;
    }

    if (guard->essential_gate != TY_ESSENTIAL_NONE &&
        !(ctx->essential_mask & guard->essential_gate)) {
        TY_LOG(ctx, TY_LOG_DEBUG,
               "USB driver '%s': essential gate 0x%08x not met "
               "(mask=0x%08x) — skipped",
               drv->name, guard->essential_gate, ctx->essential_mask);
        drv->state = TYKID_DRV_STATE_SKIPPED;
        return TYKID_ERR_NOT_ESSENTIAL;
    }

    TY_LOG(ctx, TY_LOG_DEBUG,
           "USB driver '%s': class validation passed "
           "(class=0x%02x observed, gate=0x%08x met)",
           drv->name, guard->usb_class, guard->essential_gate);
    return TYKID_OK;
}
