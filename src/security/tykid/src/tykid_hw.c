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

#define PCI_MAX_BUS     256U
#define PCI_MAX_SLOT     32U
#define PCI_MAX_FUNC      8U

#define PCI_CFG_VENDOR   0x00U
#define PCI_CFG_DEVICE   0x02U
#define PCI_CFG_COMMAND  0x04U
#define PCI_CFG_CLASS    0x08U
#define PCI_CFG_HDR_TYPE 0x0EU
#define PCI_CFG_BAR0     0x10U
#define PCI_CFG_SUBSYS_V 0x2CU
#define PCI_CFG_SUBSYS_D 0x2EU
#define PCI_CFG_IRQ_LINE 0x3CU
#define PCI_CFG_IRQ_PIN  0x3DU

#define PCI_VENDOR_INVALID  0xFFFFU
#define PCI_HDR_MULTIFUNC   0x80U

typedef struct {
    u8              pci_class;
    u8              pci_subclass;
    u8              pci_progif;
    tykid_hwclass_t ty_class;
} ty_pci_class_entry_t;

static const ty_pci_class_entry_t TY_PCI_CLASS_TABLE[] = {
    { 0x03, 0x00, 0xFF, TYKID_HW_GPU_UHD     },
    { 0x03, 0x02, 0xFF, TYKID_HW_GPU_DGPU    },
    { 0x03, 0x80, 0xFF, TYKID_HW_GPU_DGPU    },
    { 0x02, 0x00, 0xFF, TYKID_HW_NIC_GENERIC },
    { 0x02, 0x80, 0xFF, TYKID_HW_NIC_GENERIC },
    { 0x04, 0x01, 0xFF, TYKID_HW_AUDIO_HDA   },
    { 0x04, 0x03, 0x00, TYKID_HW_AUDIO_HDA   },
    { 0x01, 0x08, 0x02, TYKID_HW_STORAGE_NVME},
    { 0x01, 0x01, 0xFF, TYKID_HW_STORAGE_SATA},
    { 0x01, 0x06, 0x01, TYKID_HW_STORAGE_SATA},
    { 0x0C, 0x03, 0x30, TYKID_HW_USB_XHCI    },
    { 0x0C, 0x03, 0x20, TYKID_HW_USB_EHCI    },
    { 0x0C, 0x03, 0x10, TYKID_HW_USB_EHCI    },
    { 0x0C, 0x05, 0xFF, TYKID_HW_SMBUS       },
    { 0x11, 0x80, 0xFF, TYKID_HW_THERMAL     },
};
#define TY_PCI_CLASS_TABLE_SIZE (sizeof(TY_PCI_CLASS_TABLE)/sizeof(TY_PCI_CLASS_TABLE[0]))

typedef struct {
    u16             vendor_id;
    u16             device_id;
    tykid_hwclass_t ty_class;
    const char     *name;
} ty_pci_id_entry_t;

static const ty_pci_id_entry_t TY_PCI_ID_TABLE[] = {
    { 0x8086, 0x9B41, TYKID_HW_GPU_UHD,      "Intel UHD 630 (Comet Lake)"      },
    { 0x8086, 0x9BC8, TYKID_HW_GPU_UHD,      "Intel UHD 630 (Coffee Lake)"     },
    { 0x8086, 0x46A6, TYKID_HW_GPU_UHD,      "Intel Arc A370M"                 },
    { 0x8086, 0x4905, TYKID_HW_GPU_UHD,      "Intel Iris Xe (Tiger Lake)"      },
    { 0x8086, 0xFFFF, TYKID_HW_GPU_UHD,      "Intel GPU (generic)"             },
    { 0x1002, 0x73BF, TYKID_HW_GPU_DGPU,     "AMD Radeon RX 6900 XT"           },
    { 0x1002, 0x73DF, TYKID_HW_GPU_DGPU,     "AMD Radeon RX 6700 XT"           },
    { 0x1002, 0xFFFF, TYKID_HW_GPU_DGPU,     "AMD GPU (generic)"               },
    { 0x10DE, 0x2204, TYKID_HW_GPU_DGPU,     "NVIDIA RTX 3090"                 },
    { 0x10DE, 0x2482, TYKID_HW_GPU_DGPU,     "NVIDIA RTX 3070 Ti"              },
    { 0x10DE, 0xFFFF, TYKID_HW_GPU_DGPU,     "NVIDIA GPU (generic)"            },
    { 0x10EC, 0x8168, TYKID_HW_NIC_REALTEK,  "Realtek RTL8111/8168"            },
    { 0x10EC, 0x8125, TYKID_HW_NIC_REALTEK,  "Realtek RTL8125 2.5GbE"          },
    { 0x10EC, 0x8136, TYKID_HW_NIC_REALTEK,  "Realtek RTL8101E Fast Ethernet"  },
    { 0x10EC, 0xFFFF, TYKID_HW_NIC_REALTEK,  "Realtek NIC (generic)"           },
    { 0x8086, 0x15B7, TYKID_HW_NIC_INTEL,    "Intel I219-LM"                   },
    { 0x8086, 0x15B8, TYKID_HW_NIC_INTEL,    "Intel I219-V"                    },
    { 0x8086, 0x10D3, TYKID_HW_NIC_INTEL,    "Intel 82574L Gigabit"            },
    { 0x8086, 0x100E, TYKID_HW_NIC_INTEL,    "Intel 82540EM (QEMU e1000)"      },
    { 0x8086, 0x1015, TYKID_HW_NIC_INTEL,    "Intel 82540EM LOM"               },
    { 0x8086, 0x1016, TYKID_HW_NIC_INTEL,    "Intel 82540EP LOM"               },
    { 0x8086, 0x1017, TYKID_HW_NIC_INTEL,    "Intel 82540EP"                   },
    { 0x8086, 0x101E, TYKID_HW_NIC_INTEL,    "Intel 82540EP LP"                },
    { 0x8086, 0x100F, TYKID_HW_NIC_INTEL,    "Intel 82545EM Copper"            },
    { 0x8086, 0x1011, TYKID_HW_NIC_INTEL,    "Intel 82545EM Fiber"             },
    { 0x8086, 0x1026, TYKID_HW_NIC_INTEL,    "Intel 82545GM Copper"            },
    { 0x8086, 0x1027, TYKID_HW_NIC_INTEL,    "Intel 82545GM Fiber"             },
    { 0x8086, 0x1028, TYKID_HW_NIC_INTEL,    "Intel 82545GM Serdes"            },
    { 0x8086, 0x1010, TYKID_HW_NIC_INTEL,    "Intel 82546EB Copper"            },
    { 0x8086, 0x1012, TYKID_HW_NIC_INTEL,    "Intel 82546EB Fiber"             },
    { 0x8086, 0x107D, TYKID_HW_NIC_INTEL,    "Intel 82572EI Copper"            },
    { 0x8086, 0x107E, TYKID_HW_NIC_INTEL,    "Intel 82572EI Fiber"             },
    { 0x8086, 0x108B, TYKID_HW_NIC_INTEL,    "Intel 82573V"                    },
    { 0x8086, 0x108C, TYKID_HW_NIC_INTEL,    "Intel 82573E"                    },
    { 0x8086, 0x109A, TYKID_HW_NIC_INTEL,    "Intel 82573L"                    },
    { 0x8086, 0x150C, TYKID_HW_NIC_INTEL,    "Intel 82583V"                    },
    { 0x8086, 0x10EA, TYKID_HW_NIC_INTEL,    "Intel 82577LM"                   },
    { 0x8086, 0x10EB, TYKID_HW_NIC_INTEL,    "Intel 82577LC"                   },
    { 0x8086, 0x10EF, TYKID_HW_NIC_INTEL,    "Intel 82578DM"                   },
    { 0x8086, 0x10F0, TYKID_HW_NIC_INTEL,    "Intel 82578DC"                   },
    { 0x8086, 0x1502, TYKID_HW_NIC_INTEL,    "Intel 82579LM"                   },
    { 0x8086, 0x1503, TYKID_HW_NIC_INTEL,    "Intel 82579V"                    },
    { 0x8086, 0x9DC8, TYKID_HW_AUDIO_HDA,    "Intel Cannon Point HDA"          },
    { 0x8086, 0xA348, TYKID_HW_AUDIO_HDA,    "Intel Cannon Lake HDA"           },
    { 0x144D, 0xA808, TYKID_HW_STORAGE_NVME, "Samsung 970 EVO NVMe"            },
    { 0x1C5C, 0x1339, TYKID_HW_STORAGE_NVME, "SK Hynix P31 NVMe"               },
    { 0x1987, 0x5016, TYKID_HW_STORAGE_NVME, "Phison E16 NVMe"                 },

    { 0x1AF4, 0x1042, TYKID_HW_STORAGE_SATA, "VirtIO Block 1.0"                },
    { 0x1AF4, 0x1001, TYKID_HW_STORAGE_SATA, "VirtIO Block 0.9"                },
    { 0x1AF4, 0x1045, TYKID_HW_STORAGE_SATA, "VirtIO SCSI"                     },
    { 0x1AF4, 0x1041, TYKID_HW_NIC_GENERIC,  "VirtIO Net 1.0"                  },
    { 0x1AF4, 0x1000, TYKID_HW_NIC_GENERIC,  "VirtIO Net 0.9"                  },
};
#define TY_PCI_ID_TABLE_SIZE (sizeof(TY_PCI_ID_TABLE)/sizeof(TY_PCI_ID_TABLE[0]))

TYKID_INTERNAL tykid_hwclass_t
ty_hw_classify(u32 vendor, u32 device, u32 class_code)
{
    u8 cls = (u8)((class_code >> 16) & 0xFF);
    u8 sub = (u8)((class_code >>  8) & 0xFF);
    u8 pif = (u8)((class_code      ) & 0xFF);

    for (usz i = 0; i < TY_PCI_ID_TABLE_SIZE; i++) {
        const ty_pci_id_entry_t *e = &TY_PCI_ID_TABLE[i];
        if ((u16)vendor != e->vendor_id) continue;
        if (e->device_id != 0xFFFFU && (u16)device != e->device_id) continue;
        return e->ty_class;
    }
    for (usz i = 0; i < TY_PCI_CLASS_TABLE_SIZE; i++) {
        const ty_pci_class_entry_t *e = &TY_PCI_CLASS_TABLE[i];
        if (e->pci_class    != cls) continue;
        if (e->pci_subclass != sub) continue;
        if (e->pci_progif   != 0xFF && e->pci_progif != pif) continue;
        return e->ty_class;
    }
    return TYKID_HW_UNKNOWN;
}

static const char *ty_hw_device_name(u32 vendor, u32 device)
{
    for (usz i = 0; i < TY_PCI_ID_TABLE_SIZE; i++) {
        const ty_pci_id_entry_t *e = &TY_PCI_ID_TABLE[i];
        if ((u16)vendor == e->vendor_id &&
            (e->device_id == 0xFFFFU || (u16)device == e->device_id))
            return e->name;
    }
    return "Unknown Device";
}

#if defined(__i386__) || defined(__x86_64__)

static TYKID_ALWAYS_INL void ty_outl(u16 port, u32 val) {
    __asm__ volatile("outl %0, %w1" :: "a"(val), "Nd"(port) : "memory");
}
static TYKID_ALWAYS_INL u32 ty_inl(u16 port) {
    u32 v;
    __asm__ volatile("inl %w1, %0" : "=a"(v) : "Nd"(port) : "memory");
    return v;
}

static TYKID_ALWAYS_INL u32 ty_pci_cfg_read32(u8 bus, u8 slot, u8 fn, u8 off)
{
    u32 addr = (1u << 31) | ((u32)bus << 16) | ((u32)slot << 11)
             | ((u32)fn << 8) | ((u32)(off & 0xFC));
    ty_outl(0xCF8, addr);
    return ty_inl(0xCFC);
}

static TYKID_ALWAYS_INL void ty_pci_cfg_write32(u8 bus, u8 slot, u8 fn, u8 off, u32 val)
{
    u32 addr = (1u << 31) | ((u32)bus << 16) | ((u32)slot << 11)
             | ((u32)fn << 8) | ((u32)(off & 0xFC));
    ty_outl(0xCF8, addr);
    ty_outl(0xCFC, val);
}

#elif defined(__aarch64__)

extern volatile u32 *kobalt_ecam_base;
extern usz           kobalt_ecam_size;

static TYKID_ALWAYS_INL u32 ty_pci_cfg_read32(u8 bus, u8 slot, u8 fn, u8 off)
{
    u32 bdf = ((u32)bus << 20) | ((u32)slot << 15) | ((u32)fn << 12) | (off & 0xFFC);
    if (!kobalt_ecam_base || (usz)bdf >= kobalt_ecam_size) return 0xFFFFFFFFU;
    volatile u32 *p = kobalt_ecam_base + (bdf >> 2);
    u32 val;
    __asm__ volatile("ldr %w0, [%1]" : "=r"(val) : "r"(p) : "memory");
    return val;
}

static TYKID_ALWAYS_INL void ty_pci_cfg_write32(u8 bus, u8 slot, u8 fn, u8 off, u32 val)
{
    u32 bdf = ((u32)bus << 20) | ((u32)slot << 15) | ((u32)fn << 12) | (off & 0xFFC);
    if (!kobalt_ecam_base || (usz)bdf >= kobalt_ecam_size) return;
    volatile u32 *p = kobalt_ecam_base + (bdf >> 2);
    __asm__ volatile("str %w0, [%1]" :: "r"(val), "r"(p) : "memory");
}

#else
#   error "ty_pci_cfg_read32: unsupported architecture"
#endif

static TYKID_ALWAYS_INL u16 ty_pci_cfg_read16(u8 bus, u8 slot, u8 fn, u8 off)
{
    u32 v = ty_pci_cfg_read32(bus, slot, fn, off & ~3u);
    return (u16)((v >> ((off & 2u) * 8u)) & 0xFFFFU);
}

static TYKID_ALWAYS_INL u8 ty_pci_cfg_read8(u8 bus, u8 slot, u8 fn, u8 off)
{
    u32 v = ty_pci_cfg_read32(bus, slot, fn, off & ~3u);
    return (u8)((v >> ((off & 3u) * 8u)) & 0xFFU);
}

static u64 ty_pci_bar_size(u8 bus, u8 slot, u8 fn, u8 bar_off)
{
    u32 orig = ty_pci_cfg_read32(bus, slot, fn, bar_off);
    if (orig == 0xFFFFFFFFU || orig == 0) return 0;

    u16 cmd = ty_pci_cfg_read16(bus, slot, fn, PCI_CFG_COMMAND);
    ty_pci_cfg_write32(bus, slot, fn, PCI_CFG_COMMAND, cmd & ~0x2U);

    ty_pci_cfg_write32(bus, slot, fn, bar_off, 0xFFFFFFFFU);
    u32 raw = ty_pci_cfg_read32(bus, slot, fn, bar_off);
    ty_pci_cfg_write32(bus, slot, fn, bar_off, orig);
    ty_pci_cfg_write32(bus, slot, fn, PCI_CFG_COMMAND, cmd);

    if (raw == 0 || raw == 0xFFFFFFFFU) return 0;

    u32 mask = (orig & 0x1U) ? ~0x3U : ~0xFU;
    u32 sz32 = ~(raw & mask) + 1U;
    return (u64)sz32;
}

static u64 ty_pci_bar64_size(u8 bus, u8 slot, u8 fn, u8 bar_off)
{
    u64 lo = ty_pci_bar_size(bus, slot, fn, bar_off);

    u32 orig_hi = ty_pci_cfg_read32(bus, slot, fn, bar_off + 4);
    u16 cmd = ty_pci_cfg_read16(bus, slot, fn, PCI_CFG_COMMAND);
    ty_pci_cfg_write32(bus, slot, fn, PCI_CFG_COMMAND, cmd & ~0x2U);

    ty_pci_cfg_write32(bus, slot, fn, bar_off + 4, 0xFFFFFFFFU);
    u32 raw_hi = ty_pci_cfg_read32(bus, slot, fn, bar_off + 4);
    ty_pci_cfg_write32(bus, slot, fn, bar_off + 4, orig_hi);
    ty_pci_cfg_write32(bus, slot, fn, PCI_CFG_COMMAND, cmd);

    if (raw_hi == 0) return lo;
    u64 sz = ~((u64)raw_hi << 32 | (u32)(~0xFU & ~lo)) + 1ULL;
    return sz ? sz : lo;
}

TYKID_INTERNAL u64
ty_hw_fingerprint_one(const tykid_hw_device_t *dev, const u8 *session_key)
{
    u8 blob[48];
    ty_put_u32le(blob +  0, dev->vendor_id);
    ty_put_u32le(blob +  4, dev->device_id);
    ty_put_u32le(blob +  8, dev->subsys_id);
    ty_put_u32le(blob + 12, dev->class_code);
    ty_put_u64le(blob + 16, dev->mmio_base);
    ty_put_u64le(blob + 24, dev->mmio_size);
    blob[32] = dev->bus;
    blob[33] = dev->slot;
    blob[34] = dev->func;
    blob[35] = dev->irq;
    ty_put_u32le(blob + 36, dev->ty_class);
    ty_memzero_secure(blob + 40, 8);

    u64 h = ty_siphash24(session_key, blob, sizeof(blob));
    h ^= ty_rotl64(ty_siphash24(session_key + 16, blob, sizeof(blob)), 32);
    ty_memzero_secure(blob, sizeof(blob));
    return h;
}

TYKID_INTERNAL u64
ty_hw_topology_hash(const tykid_hw_enumset_t *hw, const u8 *session_key)
{
    u64 acc = (u64)hw->count * 0x9E3779B97F4A7C15ULL;
    for (u32 i = 0; i < hw->count; i++) {
        acc ^= hw->devices[i].hardware_fingerprint;
        acc  = ty_rotl64(acc, 27);
        acc *= 0x94D049BB133111EBULL;
    }
    acc ^= ty_siphash24(session_key, hw->devices,
                        hw->count * sizeof(tykid_hw_device_t));
    return acc;
}

TYKID_INTERNAL tykid_status_t
ty_hw_pci_enumerate(tykid_gate_ctx_t *ctx, tykid_hw_enumset_t *out)
{
    out->count = 0;
    ty_memzero_secure(out->devices, sizeof(out->devices));

    for (u32 bus = 0; bus < PCI_MAX_BUS && out->count < TYKID_MAX_HW_DEVICES; bus++) {
        for (u32 slot = 0; slot < PCI_MAX_SLOT && out->count < TYKID_MAX_HW_DEVICES; slot++) {
            u16 v0 = ty_pci_cfg_read16((u8)bus, (u8)slot, 0, PCI_CFG_VENDOR);
            if (v0 == PCI_VENDOR_INVALID || v0 == 0) continue;

            u8 hdr = ty_pci_cfg_read8((u8)bus, (u8)slot, 0, PCI_CFG_HDR_TYPE);
            u8 maxfn = (hdr & PCI_HDR_MULTIFUNC) ? PCI_MAX_FUNC : 1;

            for (u8 fn = 0; fn < maxfn && out->count < TYKID_MAX_HW_DEVICES; fn++) {
                u16 vendor = ty_pci_cfg_read16((u8)bus, (u8)slot, fn, PCI_CFG_VENDOR);
                if (vendor == PCI_VENDOR_INVALID) continue;

                u16 device     = ty_pci_cfg_read16((u8)bus, (u8)slot, fn, PCI_CFG_DEVICE);
                u32 class_dw   = ty_pci_cfg_read32((u8)bus, (u8)slot, fn, PCI_CFG_CLASS);
                u32 class_code = (class_dw >> 8) & 0xFFFFFFU;
                u16 subsys_v   = ty_pci_cfg_read16((u8)bus, (u8)slot, fn, PCI_CFG_SUBSYS_V);
                u16 subsys_d   = ty_pci_cfg_read16((u8)bus, (u8)slot, fn, PCI_CFG_SUBSYS_D);
                u32 bar0       = ty_pci_cfg_read32((u8)bus, (u8)slot, fn, PCI_CFG_BAR0);
                u8  irq        = ty_pci_cfg_read8 ((u8)bus, (u8)slot, fn, PCI_CFG_IRQ_LINE);

                if (((class_code >> 16) & 0xFF) == 0x06) continue;

                tykid_hw_device_t *dev = &out->devices[out->count++];
                dev->vendor_id  = vendor;
                dev->device_id  = device;
                dev->subsys_id  = ((u32)subsys_v << 16) | subsys_d;
                dev->class_code = class_code;
                dev->bus        = (u8)bus;
                dev->slot       = (u8)slot;
                dev->func       = fn;
                dev->irq        = irq;

                u8 bar_type = bar0 & 0x6U;
                if (bar_type == 0x4) {

                    dev->mmio_base = ((u64)ty_pci_cfg_read32((u8)bus,(u8)slot,fn,PCI_CFG_BAR0+4) << 32)
                                   | (bar0 & ~0xFULL);
                    dev->mmio_size = ty_pci_bar64_size((u8)bus, (u8)slot, fn, PCI_CFG_BAR0);
                } else {
                    dev->mmio_base = bar0 & ~0xFULL;
                    dev->mmio_size = ty_pci_bar_size((u8)bus, (u8)slot, fn, PCI_CFG_BAR0);
                }

                dev->ty_class = ty_hw_classify(vendor, device, class_code);
                ty_strncpy(dev->name, ty_hw_device_name(vendor, device), TYKID_MAX_NAME);
                dev->hardware_fingerprint = ty_hw_fingerprint_one(dev, ctx->session_key);

                TY_LOG(ctx, TY_LOG_DEBUG,
                    "PCI %02x:%02x.%x vendor=%04x device=%04x class=%06x "
                    "mmio=%016llx size=%016llx ty=%08x name='%s'",
                    bus, slot, fn, vendor, device, class_code,
                    (unsigned long long)dev->mmio_base,
                    (unsigned long long)dev->mmio_size,
                    dev->ty_class, dev->name);
            }
        }
    }

    out->enum_timestamp    = kobalt_tsc_read();
    out->bus_topology_hash = ty_hw_topology_hash(out, ctx->session_key);

    TY_LOG(ctx, TY_LOG_INFO,
           "PCI enumeration: %u devices, topology=%016llx",
           out->count, (unsigned long long)out->bus_topology_hash);

    return out->count > 0 ? TYKID_OK : TYKID_ERR_HW_ENUM;
}

TYKID_API tykid_status_t
tykid_enumerate_hardware(tykid_gate_ctx_t *ctx, tykid_hw_enumset_t *out)
{
    if (__ty_unlikely(!ctx || !out)) return TYKID_ERR_GENERIC;
    tykid_status_t s = tykid_verify_seal(ctx);
    if (__ty_unlikely(s != TYKID_OK)) return s;
    s = ty_hw_pci_enumerate(ctx, out);
    if (s == TYKID_OK) {
        ctx->hw.enumset    = *out;
        ctx->hw.probed     = TYKID_TRUE;
        ctx->hw.probe_hash = out->bus_topology_hash;
    }
    return s;
}

TYKID_API tykid_status_t
tykid_fingerprint_device(tykid_gate_ctx_t *ctx, tykid_hw_device_t *dev)
{
    if (__ty_unlikely(!ctx || !dev)) return TYKID_ERR_GENERIC;
    dev->hardware_fingerprint = ty_hw_fingerprint_one(dev, ctx->session_key);
    return TYKID_OK;
}
