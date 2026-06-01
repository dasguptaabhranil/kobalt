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
#include "../inc/tykid_usb.h"
#include <pci.h>
#include <kobalt_ident.h>
#include <kfmt.h>
#include <kmalloc.h>
#include <vfs.h>
#include <stdarg.h>

extern uint32_t sys_now(void);

static tykid_gate_ctx_t *g_ty_ctx;

void tykid_kobalt_set_ctx(tykid_gate_ctx_t *ctx) { g_ty_ctx = ctx; }
tykid_gate_ctx_t *tykid_kobalt_get_ctx(void) { return g_ty_ctx; }

void kobalt_usb_notify(u8 cls, u8 sub, u8 proto, u16 vid, u16 pid)
{
    if (g_ty_ctx)
        ty_usb_notify_device(g_ty_ctx, cls, sub, proto, vid, pid);
}

void kobalt_kernel_panic(const char *msg)
{
    klog_fail("tykid", msg ? msg : "unspecified panic");
    __asm__ volatile("cli" ::: "memory");
    for (;;)
        __asm__ volatile("hlt");
    __builtin_unreachable();
}

static tykid_status_t ty_vfs_alloc_read_cb(const char  *path,
                                             u8         **buf_out,
                                             usz         *len_out)
{
    vfs_stat_t st;
    if (vfs_stat(path, &st) < 0) return TYKID_ERR_PATH;
    size_t sz = (size_t)st.size;

    uint8_t *buf = (uint8_t *)kmalloc(sz ? sz : 1);
    if (!buf) return TYKID_ERR_ALLOC;

    int fd = vfs_open(path, VFS_O_RDONLY, 0);
    if (fd < 0) { kfree(buf); return TYKID_ERR_PATH; }

    size_t done = 0;
    while (done < sz) {
        int n = vfs_read(fd, buf + done, sz - done);
        if (n <= 0) break;
        done += (size_t)n;
    }
    vfs_close(fd);

    *buf_out = buf;
    *len_out = done;
    return TYKID_OK;
}

#ifndef KY_KFMT_HAS_VSNPRINTF
#  define KY_KFMT_HAS_VSNPRINTF 0
#endif

static void ty_log_cb(u8 level, const char *fmt, ...)
{
#if KY_KFMT_HAS_VSNPRINTF
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    kfmt_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    const char *msg = buf;
#else
    const char *msg = fmt;
#endif

    if      (level >= TY_LOG_FATAL) klog_fail("tykid", msg);
    else if (level >= TY_LOG_ERROR) klog_fail("tykid", msg);
    else if (level >= TY_LOG_WARN)  klog_warn("tykid", msg);
    else                            klog_info("tykid", msg);
}

static void *ty_alloc_cb(usz size)           { return kmalloc((size_t)size); }
static void  ty_free_cb(void *ptr, usz size) { (void)size; kfree(ptr); }

static usz ty_entropy_cb(void *buf, usz len)
{
    u8 *p = (u8 *)buf;
    for (usz i = 0; i < len; i++) {
        uint32_t lo, hi;
        __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
        p[i] = (u8)(lo ^ hi
                  ^ (uint8_t)(KOBALT_KERNEL_IDENT >> (i % 64))
                  ^ (uint8_t)(sys_now() >> (i & 7)));
    }
    return len;
}

u32  kobalt_cpuid_microcode_rev(void) { return 0; }

void kobalt_smbios_uuid(u8 out[16])
{
    ty_memzero_secure(out, 16);
}

u64  kobalt_platform_security_version(void) { return 0; }

void kobalt_kernel_image_hash(u8 out[32])
{
    ty_memzero_secure(out, 32);
}

static tykid_status_t ty_kobalt_path_iter(const char *dir,
                                           bool8 (*cb)(const char *, void *),
                                           void *priv)
{
    vfs_dirent_t de;
    for (uint64_t i = 0; ; i++) {
        if (vfs_readdir(dir, i, &de) < 0) break;
        if (!cb(de.name, priv)) break;
    }
    return TYKID_OK;
}

tykid_status_t kobalt_vfs_read_text(const char *path,
                                     char *buf, usz buf_sz,
                                     usz *out_len)
{
    vfs_stat_t st;
    if (vfs_stat(path, &st) < 0) return TYKID_ERR_PATH;
    size_t sz = (size_t)st.size;

    int fd = vfs_open(path, VFS_O_RDONLY, 0);
    if (fd < 0) return TYKID_ERR_PATH;

    usz cap = (sz < buf_sz) ? sz : buf_sz;
    usz done = 0;
    while (done < cap) {
        int n = vfs_read(fd, buf + done, cap - done);
        if (n <= 0) break;
        done += (size_t)n;
    }
    vfs_close(fd);

    *out_len = done;
    return TYKID_OK;
}

static struct {
    int  fd;
    char path[256];
} s_bin;

tykid_status_t kobalt_vfs_read_binary(const char *path,
                                       u8 *buf, usz buf_sz,
                                       usz *out_len)
{
    if (!s_bin.fd || ty_strncmp(s_bin.path, path, 256) != 0) {
        if (s_bin.fd > 0) vfs_close(s_bin.fd);
        s_bin.fd = vfs_open(path, VFS_O_RDONLY, 0);
        ty_strncpy(s_bin.path, path, 256);
        if (s_bin.fd < 0) { s_bin.fd = 0; return TYKID_ERR_PATH; }
    }

    usz done = 0;
    while (done < buf_sz) {
        int n = vfs_read(s_bin.fd, buf + done, buf_sz - done);
        if (n <= 0) break;
        done += (size_t)n;
    }

    *out_len = done;
    if (done == 0) {
        vfs_close(s_bin.fd);
        s_bin.fd      = 0;
        s_bin.path[0] = '\0';
        return TYKID_ERR_GENERIC;
    }
    return TYKID_OK;
}

void tykid_kobalt_config_init(tykid_config_t *cfg, u64 boot_token)
{
    ty_memzero_secure(cfg, sizeof(*cfg));

    cfg->log_fn             = ty_log_cb;
    cfg->alloc_fn           = ty_alloc_cb;
    cfg->free_fn            = ty_free_cb;
    cfg->entropy_fn         = ty_entropy_cb;
    cfg->kobalt_boot_token  = boot_token;
    cfg->max_load_attempts  = 3;
    cfg->probe_timeout_ms   = 500;
    cfg->vfs_read_fn        = ty_vfs_alloc_read_cb;

    cfg->path_iter_fn           = ty_kobalt_path_iter;
    cfg->trust_anchor_der       = NULL;
    cfg->trust_anchor_der_len   = 0;
    cfg->entropy_block_threshold = 7.5;
    cfg->block_unsigned         = TYKID_FALSE;
    cfg->block_suspicious       = TYKID_FALSE;
    cfg->load_non_essential     = TYKID_FALSE;

    u64 seed = KOBALT_KERNEL_IDENT ^ boot_token;
    for (u32 i = 0; i < 32; i++) {
        cfg->hmac_master_key[i] =
            (u8)(seed                >> ( i        % 8 * 8)) ^
            (u8)(boot_token          >> ((i *  7)  % 64))    ^
            (u8)(KOBALT_KERNEL_IDENT >> ((i * 13)  % 64));
    }
}

typedef struct {
    const char      *name;
    tykid_hwclass_t  hw_class;
    u32              flags;
    bool8            is_critical;
} ty_builtin_t;

static const ty_builtin_t TY_BUILTINS[] = {
    { "uart",
      TYKID_HW_UNKNOWN,
      TYKID_DRV_FLAG_CRITICAL | TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED,
      TYKID_TRUE },

    { "vga",
      TYKID_HW_GPU_UHD,
      TYKID_DRV_FLAG_CRITICAL | TYKID_DRV_FLAG_SINGLETON |
      TYKID_DRV_FLAG_VERIFIED | TYKID_DRV_FLAG_FALLBACK,
      TYKID_TRUE },

    { "pci",
      TYKID_HW_UNKNOWN,
      TYKID_DRV_FLAG_CRITICAL | TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED,
      TYKID_TRUE },

    { "ps2kbd",
      TYKID_HW_INPUT_KBD,
      TYKID_DRV_FLAG_CRITICAL | TYKID_DRV_FLAG_SINGLETON |
      TYKID_DRV_FLAG_VERIFIED | TYKID_DRV_FLAG_FALLBACK,
      TYKID_TRUE },

    { "blkdev",
      TYKID_HW_UNKNOWN,
      TYKID_DRV_FLAG_CRITICAL | TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED,
      TYKID_TRUE },

    { "virtio-net",
      TYKID_HW_NIC_GENERIC,
      TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED | TYKID_DRV_FLAG_FALLBACK,
      TYKID_FALSE },

    { "e1000",
      TYKID_HW_NIC_INTEL,
      TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED | TYKID_DRV_FLAG_FALLBACK,
      TYKID_FALSE },

    { "igc",
      TYKID_HW_NIC_INTEL,
      TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED | TYKID_DRV_FLAG_FALLBACK,
      TYKID_FALSE },

    { "ixgbe",
      TYKID_HW_NIC_INTEL,
      TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED | TYKID_DRV_FLAG_FALLBACK,
      TYKID_FALSE },

    { "usb_cdc",   TYKID_HW_NIC_GENERIC, TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED, TYKID_FALSE },
    { "usb_rndis", TYKID_HW_NIC_GENERIC, TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED, TYKID_FALSE },

    { "ahci",
      TYKID_HW_STORAGE_SATA,
      TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED | TYKID_DRV_FLAG_FALLBACK,
      TYKID_FALSE },

    { "nvme",
      TYKID_HW_STORAGE_NVME,
      TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED | TYKID_DRV_FLAG_FALLBACK,
      TYKID_FALSE },

    { "virtio-blk",
      TYKID_HW_STORAGE_SATA,
      TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED | TYKID_DRV_FLAG_FALLBACK,
      TYKID_FALSE },

    { "usb_msc",  TYKID_HW_CLASS_STORAGE_USB, TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED, TYKID_FALSE },
    { "usb_scsi", TYKID_HW_CLASS_STORAGE_USB, TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED, TYKID_FALSE },

    { "xhci",
      TYKID_HW_USB_XHCI,
      TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED | TYKID_DRV_FLAG_CRITICAL,
      TYKID_FALSE },

    { "ehci",
      TYKID_HW_USB_EHCI,
      TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED,
      TYKID_FALSE },

    { "usb_core", TYKID_HW_USB_XHCI,
      TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED | TYKID_DRV_FLAG_CRITICAL,
      TYKID_FALSE },

    { "usb_ep",   TYKID_HW_USB_XHCI, TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED, TYKID_FALSE },
    { "usb_hub",  TYKID_HW_USB_XHCI, TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED, TYKID_FALSE },

    { "hid",         TYKID_HW_INPUT_KBD,   TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED, TYKID_FALSE },
    { "hid_kbd",     TYKID_HW_INPUT_KBD,   TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED | TYKID_DRV_FLAG_FALLBACK, TYKID_FALSE },
    { "hid_mouse",   TYKID_HW_INPUT_MOUSE, TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED, TYKID_FALSE },
    { "hid_tablet",  TYKID_HW_INPUT_MOUSE, TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED, TYKID_FALSE },

    { "usb_cdc_acm", TYKID_HW_UNKNOWN, TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED, TYKID_FALSE },
    { "usb_ftdi",    TYKID_HW_UNKNOWN, TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED, TYKID_FALSE },
    { "usb_cp210x",  TYKID_HW_UNKNOWN, TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED, TYKID_FALSE },

    { "intel_hda",
      TYKID_HW_AUDIO_HDA,
      TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED | TYKID_DRV_FLAG_FALLBACK,
      TYKID_FALSE },

    { "usb_audio", TYKID_HW_AUDIO_USB, TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED, TYKID_FALSE },
    { "usb_midi",  TYKID_HW_AUDIO_USB, TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED, TYKID_FALSE },

    { "btusb",  TYKID_HW_BT, TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED, TYKID_FALSE },
    { "bt_hci", TYKID_HW_BT, TYKID_DRV_FLAG_SINGLETON | TYKID_DRV_FLAG_VERIFIED, TYKID_FALSE },
};

#define TY_BUILTIN_COUNT (sizeof(TY_BUILTINS) / sizeof(TY_BUILTINS[0]))

void tykid_kobalt_register_builtins(tykid_gate_ctx_t *ctx)
{
    if (!ctx) return;
    ty_driver_registry_t *reg = &ctx->reg;

    for (u32 i = 0; i < TY_BUILTIN_COUNT; i++) {
        if (reg->count >= TYKID_MAX_DRIVERS) break;
        const ty_builtin_t *b = &TY_BUILTINS[i];

        if (ty_find_driver_by_name(ctx, b->name) >= 0) continue;

        tykid_driver_desc_t *e = &reg->entries[reg->count++];
        ty_memzero_secure(e, sizeof(*e));

        ty_strncpy(e->name,       b->name, TYKID_MAX_NAME);
        e->hw_classes[0]  = b->hw_class;
        e->hw_class_count = 1;
        e->flags          = b->flags;
        e->threat_class   = TY_THREAT_NONE;
        e->priority       = 255;
        e->version_major  = 1;
        e->version_minor  = 0;
        e->base_vaddr     = 0;

        e->state = b->is_critical ? TYKID_DRV_STATE_ACTIVE
                                  : TYKID_DRV_STATE_REGISTERED;
    }
}

void tykid_kobalt_gate_builtins(tykid_gate_ctx_t        *ctx,
                                 const tykid_hw_enumset_t *hw)
{
    if (!ctx) return;

    tykid_compute_essential_mask(ctx, hw);

    ty_driver_registry_t *reg = &ctx->reg;
    u32 approved = 0, skipped = 0;

    for (u32 i = 0; i < reg->count; i++) {
        tykid_driver_desc_t *drv = &reg->entries[i];

        if (drv->state != TYKID_DRV_STATE_REGISTERED) continue;

        if (tykid_driver_is_essential(drv, ctx->essential_mask)) {
            drv->state = TYKID_DRV_STATE_ACTIVE;
            approved++;
        } else {
            drv->state = TYKID_DRV_STATE_SKIPPED;
            skipped++;
            TY_LOG(ctx, TY_LOG_INFO,
                   "gate_builtins: '%s' skipped (no matching hardware)",
                   drv->name);
        }
    }

    static const char *const AUDIO_HDA_DRVS[] = { "intel_hda" };
    static const char *const AUDIO_USB_DRVS[] = { "usb_audio", "usb_midi" };

    bool8 has_hda_pci   = TYKID_FALSE;
    bool8 has_usb_audio = TYKID_FALSE;

    u32 npci = (u32)pci_count();
    for (u32 p = 0; p < npci; p++) {
        pci_device_t *d = pci_get_device(p);
        if (!d) continue;
        if (d->class_code == 0x04u && d->subclass == 0x03u) has_hda_pci   = TYKID_TRUE;
        if (d->class_code == 0x0Cu && d->subclass == 0x03u) has_usb_audio = TYKID_TRUE;
    }

    for (u32 i = 0; has_hda_pci && i < sizeof(AUDIO_HDA_DRVS)/sizeof(*AUDIO_HDA_DRVS); i++) {
        int idx = ty_find_driver_by_name(ctx, AUDIO_HDA_DRVS[i]);
        if (idx < 0) continue;
        tykid_driver_desc_t *drv = &reg->entries[idx];
        if (drv->state == TYKID_DRV_STATE_SKIPPED) {
            drv->state = TYKID_DRV_STATE_ACTIVE;
            skipped--; approved++;
        }
    }

    for (u32 i = 0; has_usb_audio && i < sizeof(AUDIO_USB_DRVS)/sizeof(*AUDIO_USB_DRVS); i++) {
        int idx = ty_find_driver_by_name(ctx, AUDIO_USB_DRVS[i]);
        if (idx < 0) continue;
        tykid_driver_desc_t *drv = &reg->entries[idx];
        if (drv->state == TYKID_DRV_STATE_SKIPPED) {
            drv->state = TYKID_DRV_STATE_ACTIVE;
            skipped--; approved++;
        }
    }

    TY_LOG(ctx, TY_LOG_INFO,
           "gate_builtins: %u builtin(s) approved, %u skipped",
           approved, skipped);
}

int tykid_kobalt_builtin_approved(tykid_gate_ctx_t *ctx, const char *name)
{
    if (!ctx || !name) return 0;
    int idx = ty_find_driver_by_name(ctx, name);
    if (idx < 0) return 0;
    return ctx->reg.entries[idx].state == TYKID_DRV_STATE_ACTIVE ? 1 : 0;
}
