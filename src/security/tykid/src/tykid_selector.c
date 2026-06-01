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

#include "../inc/tykid_internal.h"

typedef struct {
    u32 pci_class_code;
    u32 pci_mask;
    u32 essential_flag;
} ty_class_map_entry_t;

static const ty_class_map_entry_t ty_class_map[] = {
    { 0x010802, 0xFFFFFF, TY_ESSENTIAL_STORAGE_NVME  },
    { 0x010601, 0xFFFFFF, TY_ESSENTIAL_STORAGE_SATA  },
    { 0x010100, 0xFFFF00, TY_ESSENTIAL_STORAGE_SATA  },
    { 0x010000, 0xFFFF00, TY_ESSENTIAL_STORAGE_SATA  },
    { 0x018000, 0xFFFF00, TY_ESSENTIAL_STORAGE_SATA  },
    { 0x020000, 0xFF0000, TY_ESSENTIAL_NIC            },
    { 0x030000, 0xFFFF00, TY_ESSENTIAL_FRAMEBUF       },
    { 0x060400, 0xFFFF00, TY_ESSENTIAL_PCI_BUS        },
    { 0x060000, 0xFFFF00, TY_ESSENTIAL_PCI_BUS        },
    { 0x0C0330, 0xFFFFFF, TY_ESSENTIAL_USB_HOST       },
    { 0x0C0320, 0xFFFFFF, TY_ESSENTIAL_USB_HOST       },
    { 0x0C0310, 0xFFFFFF, TY_ESSENTIAL_USB_HOST       },
    { 0x0C0500, 0xFFFF00, TY_ESSENTIAL_SMBUS          },
    { 0x080600, 0xFFFF00, TY_ESSENTIAL_IOMMU          },
    { 0x080000, 0xFFFF00, TY_ESSENTIAL_INTERRUPT      },
    { 0x080100, 0xFFFF00, TY_ESSENTIAL_INTERRUPT      },
    { 0x080300, 0xFFFF00, TY_ESSENTIAL_TIMER          },
    { 0x080400, 0xFFFF00, TY_ESSENTIAL_TIMER          },
};
#define TY_CLASS_MAP_COUNT (sizeof(ty_class_map)/sizeof(ty_class_map[0]))

typedef struct {
    u16 vendor_id;
    u16 device_id;
    u32 essential_flag;
} ty_vid_did_map_t;

static const ty_vid_did_map_t ty_vid_did_map[] = {
    { 0x8086, 0x0F18, TY_ESSENTIAL_KBD          },
    { 0x8086, 0x9D2B, TY_ESSENTIAL_KBD          },
    { 0x1022, 0x1481, TY_ESSENTIAL_IOMMU        },
    { 0x1022, 0x1577, TY_ESSENTIAL_IOMMU        },
    { 0x10EC, 0x5765, TY_ESSENTIAL_STORAGE_NVME },
    { 0x106B, 0x2003, TY_ESSENTIAL_STORAGE_NVME },
    { 0x1AF4, 0x1042, TY_ESSENTIAL_STORAGE_SATA },
    { 0x1AF4, 0x1001, TY_ESSENTIAL_STORAGE_SATA },
    { 0x1AF4, 0x1045, TY_ESSENTIAL_STORAGE_SATA },
    { 0x1AF4, 0x105A, TY_ESSENTIAL_STORAGE_SATA },
    { 0x1AF4, 0x1041, TY_ESSENTIAL_NIC          },
    { 0x1AF4, 0x1000, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x100E, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x1015, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x1016, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x1017, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x101E, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x100F, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x1011, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x1026, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x1027, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x1028, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x1010, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x1012, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x107D, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x107E, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x108B, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x108C, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x109A, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x150C, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x10EA, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x10EB, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x10EF, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x10F0, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x1502, TY_ESSENTIAL_NIC          },
    { 0x8086, 0x1503, TY_ESSENTIAL_NIC          },
};
#define TY_VID_DID_MAP_COUNT (sizeof(ty_vid_did_map)/sizeof(ty_vid_did_map[0]))

typedef struct {
    tykid_hwclass_t hw_class;
    u32             essential_flag;
} ty_drv_class_map_t;

static const ty_drv_class_map_t ty_drv_class_map[] = {
    { TYKID_HW_CLASS_ANY,          0             },
    { TYKID_HW_CLASS_PCI_BRIDGE,   TY_ESSENTIAL_PCI_BUS      },
    { TYKID_HW_CLASS_STORAGE_NVME, TY_ESSENTIAL_STORAGE_NVME },
    { TYKID_HW_CLASS_STORAGE_SATA, TY_ESSENTIAL_STORAGE_SATA },
    { TYKID_HW_CLASS_STORAGE_USB,  TY_ESSENTIAL_USB_HOST     },
    { TYKID_HW_CLASS_NIC,          TY_ESSENTIAL_NIC          },
    { TYKID_HW_CLASS_USB_HOST,     TY_ESSENTIAL_USB_HOST     },
    { TYKID_HW_CLASS_DISPLAY,      TY_ESSENTIAL_FRAMEBUF     },
    { TYKID_HW_CLASS_INPUT,        TY_ESSENTIAL_KBD          },
    { TYKID_HW_CLASS_SMBUS,        TY_ESSENTIAL_SMBUS        },
    { TYKID_HW_CLASS_IOMMU,        TY_ESSENTIAL_IOMMU        },
    { TYKID_HW_CLASS_AUDIO,        0xFFFFFFFFU               },
    { TYKID_HW_CLASS_THERMAL,      0xFFFFFFFFU               },
    { TYKID_HW_CLASS_BT,           0xFFFFFFFFU               },
};
#define TY_DRV_CLASS_MAP_COUNT (sizeof(ty_drv_class_map)/sizeof(ty_drv_class_map[0]))

static TYKID_ALWAYS_INL u32 ty_pci_class_of(const tykid_hw_device_t *dev) {
    return (u32)(dev->class_code & 0x00FFFFFFu);
}

ty_hw_essential_mask_t
ty_selector_compute_mask(tykid_gate_ctx_t *ctx, const tykid_hw_enumset_t *hw)
{
    ty_hw_essential_mask_t mask = 0;
    mask |= TY_ESSENTIAL_ACPI;
    mask |= TY_ESSENTIAL_INTERRUPT;
    mask |= TY_ESSENTIAL_TIMER;
    mask |= TY_ESSENTIAL_KBD;

    if (!hw || hw->count == 0) {
        TY_LOG(ctx, TY_LOG_WARN, "selector: hw set empty, using arch-minimum mask");
        return mask;
    }

    for (u32 i = 0; i < hw->count; i++) {
        const tykid_hw_device_t *dev = &hw->devices[i];
        u32 code = ty_pci_class_of(dev);

        for (u32 j = 0; j < (u32)TY_CLASS_MAP_COUNT; j++) {
            u32 mc = code        & ty_class_map[j].pci_mask;
            u32 me = ty_class_map[j].pci_class_code & ty_class_map[j].pci_mask;
            if (mc == me) mask |= ty_class_map[j].essential_flag;
        }
        for (u32 j = 0; j < (u32)TY_VID_DID_MAP_COUNT; j++) {
            if ((u16)dev->vendor_id == ty_vid_did_map[j].vendor_id &&
                (u16)dev->device_id == ty_vid_did_map[j].device_id)
                mask |= ty_vid_did_map[j].essential_flag;
        }
    }

    if (hw->iommu_active)
        mask |= TY_ESSENTIAL_IOMMU;

    if (!(mask & TY_ESSENTIAL_IOMMU)) {
        if (mask & TY_ESSENTIAL_NIC) {
            TY_LOG(ctx, TY_LOG_WARN,
                   "selector: no IOMMU — suppressing NIC drivers (uncontained DMA)");
            mask &= ~TY_ESSENTIAL_NIC;
        }
        if (mask & TY_ESSENTIAL_USB_HOST) {
            TY_LOG(ctx, TY_LOG_WARN,
                   "selector: no IOMMU — suppressing USB host drivers (uncontained DMA)");
            mask &= ~TY_ESSENTIAL_USB_HOST;
        }
    }

    TY_LOG(ctx, TY_LOG_INFO,
           "selector: essential mask=0x%08x (%u devices)", (u32)mask, hw->count);
    return mask;
}

bool8 ty_selector_driver_needed(const tykid_driver_desc_t *drv,
                                 ty_hw_essential_mask_t mask)
{
    if (!drv) return TYKID_FALSE;

    for (u32 i = 0; i < (u32)TY_DRV_CLASS_MAP_COUNT; i++) {
        bool8 match = TYKID_FALSE;
        for (u8 j = 0; j < drv->hw_class_count && j < 8; j++) {
            if (ty_drv_class_map[i].hw_class == drv->hw_classes[j]) {
                match = TYKID_TRUE; break;
            }
        }
        if (!match && ty_drv_class_map[i].hw_class == TYKID_HW_CLASS_ANY
                   && drv->hw_class_count == 0)
            match = TYKID_TRUE;
        if (!match) continue;

        u32 flag = ty_drv_class_map[i].essential_flag;
        if (flag == 0)          return TYKID_TRUE;
        if (flag == 0xFFFFFFFFU) return TYKID_FALSE;
        return (mask & flag) ? TYKID_TRUE : TYKID_FALSE;
    }
    return TYKID_TRUE;
}

ty_hw_essential_mask_t
tykid_compute_essential_mask(tykid_gate_ctx_t *ctx, const tykid_hw_enumset_t *hw)
{
    if (!ctx) return 0;
    ty_hw_essential_mask_t mask = ty_selector_compute_mask(ctx, hw);
    ctx->essential_mask = mask;
    return mask;
}

bool8 tykid_driver_is_essential(const tykid_driver_desc_t *drv,
                                  ty_hw_essential_mask_t mask)
{
    return ty_selector_driver_needed(drv, mask);
}

const char *ty_selector_essential_flag_name(u32 flag)
{
    switch (flag) {
    case TY_ESSENTIAL_PCI_BUS:      return "PCI_BUS";
    case TY_ESSENTIAL_ACPI:         return "ACPI";
    case TY_ESSENTIAL_INTERRUPT:    return "INTERRUPT";
    case TY_ESSENTIAL_TIMER:        return "TIMER";
    case TY_ESSENTIAL_STORAGE_NVME: return "STORAGE_NVME";
    case TY_ESSENTIAL_STORAGE_SATA: return "STORAGE_SATA";
    case TY_ESSENTIAL_USB_HOST:     return "USB_HOST";
    case TY_ESSENTIAL_NIC:          return "NIC";
    case TY_ESSENTIAL_FRAMEBUF:     return "FRAMEBUF";
    case TY_ESSENTIAL_KBD:          return "KBD";
    case TY_ESSENTIAL_SMBUS:        return "SMBUS";
    case TY_ESSENTIAL_IOMMU:        return "IOMMU";
    default:                        return "UNKNOWN";
    }
}

void ty_selector_apply(tykid_gate_ctx_t *ctx)
{
    if (ctx->cfg.load_non_essential) return;

    u32 skipped = 0;
    for (u32 i = 0; i < ctx->reg.count; i++) {
        tykid_driver_desc_t *drv = &ctx->reg.entries[i];
        if (drv->state == TYKID_DRV_STATE_ACTIVE)     continue;
        if (drv->state != TYKID_DRV_STATE_REGISTERED) continue;

        if (!ty_selector_driver_needed(drv, ctx->essential_mask)) {
            drv->state = TYKID_DRV_STATE_SKIPPED;
            skipped++;
        }
    }
    ctx->total_skipped += skipped;
    TY_LOG(ctx, TY_LOG_INFO, "selector: %u driver(s) skipped", skipped);
}
