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

#ifndef __TYKID_H__
#define __TYKID_H__

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(__GNUC__) && !defined(__clang__)
#  error "TYKID requires GCC >= 10 or Clang >= 12. MSVC is not supported."
#endif
#if defined(_WIN32) || defined(__APPLE__)
#  error "TYKID is Kobalt-exclusive. Foreign OS detected -- aborting."
#endif

#ifndef KOBALT_KERNEL_IDENT
#  error "TYKID: KOBALT_KERNEL_IDENT is not defined. " \
         "Include <kobalt/ident.h> before this header, or pass " \
         "-DKOBALT_KERNEL_IDENT=<token> during the kernel build. " \
         "This library will not build without it."
#endif

#define __TYKID_KOBALT_SEAL_RAW 0x4B4F424C54494459ULL
#define __TYKID_KOBALT_EXPECTED   (KOBALT_KERNEL_IDENT ^ __TYKID_KOBALT_SEAL_RAW)

typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;
typedef signed   char       s8;
typedef signed   short      s16;
typedef signed   int        s32;
typedef signed   long long  s64;
typedef __UINTPTR_TYPE__    uptr;
typedef __INTPTR_TYPE__     sptr;
typedef __SIZE_TYPE__       usz;
typedef unsigned char       bool8;

#ifndef NULL
#  define NULL ((void *)0)
#endif
#define TYKID_TRUE  ((bool8)1)
#define TYKID_FALSE ((bool8)0)

#define __ty_likely(x)      __builtin_expect(!!(x), 1)
#define __ty_unlikely(x)    __builtin_expect(!!(x), 0)
#define TYKID_API           __attribute__((visibility("default")))
#define TYKID_INTERNAL      __attribute__((visibility("hidden")))
#define TYKID_NORETURN      __attribute__((noreturn))
#define TYKID_PACKED        __attribute__((packed))
#define TYKID_ALIGNED(n)    __attribute__((aligned(n)))
#define TYKID_NOINLINE      __attribute__((noinline))
#define TYKID_ALWAYS_INL    __attribute__((always_inline)) static inline
#define TYKID_COLD          __attribute__((cold))
#define TYKID_HOT           __attribute__((hot))
#define TYKID_PURE          __attribute__((pure))
#define TYKID_CONSTFN       __attribute__((const))
#define TYKID_UNUSED        __attribute__((unused))
#define TYKID_SECTION(s)    __attribute__((section(s)))
#define TYKID_WARN_UNUSED   __attribute__((warn_unused_result))

#define TYKID_STATIC_ASSERT(cond, msg) _Static_assert((cond), "TYKID: " msg)
TYKID_STATIC_ASSERT(sizeof(u64) == 8,  "u64 must be 8 bytes");
TYKID_STATIC_ASSERT(sizeof(uptr) >= 4, "uptr must be at least 32-bit");

#define TYKID_VERSION_MAJOR  2
#define TYKID_VERSION_MINOR  0
#define TYKID_VERSION_PATCH  0
#define TYKID_ABI_EPOCH      0xA7C3U
#define TYKID_VERSION_STRING "TYKID/2.0.0-kobalt-abiepoch:A7C3-threat+selector"

typedef s32 tykid_status_t;
#define TYKID_OK                  ( (tykid_status_t)   0)
#define TYKID_ERR_GENERIC         ( (tykid_status_t)  -1)
#define TYKID_ERR_SEAL_BROKEN     ( (tykid_status_t)  -2)
#define TYKID_ERR_HW_ENUM         ( (tykid_status_t)  -3)
#define TYKID_ERR_NO_DRIVER       ( (tykid_status_t)  -4)
#define TYKID_ERR_DRIVER_CORRUPT  ( (tykid_status_t)  -5)
#define TYKID_ERR_ENTROPY_LOW     ( (tykid_status_t)  -6)
#define TYKID_ERR_ALLOC           ( (tykid_status_t)  -7)
#define TYKID_ERR_PATH            ( (tykid_status_t)  -8)
#define TYKID_ERR_PERM            ( (tykid_status_t)  -9)
#define TYKID_ERR_CYCLE           ( (tykid_status_t) -10)
#define TYKID_ERR_TIMEOUT         ( (tykid_status_t) -11)
#define TYKID_ERR_ALREADY_LOADED  ( (tykid_status_t) -12)
#define TYKID_ERR_VERSION         ( (tykid_status_t) -13)
#define TYKID_ERR_BLACKLIST       ( (tykid_status_t) -14)
#define TYKID_ERR_DEP_FAILED      ( (tykid_status_t) -15)
#define TYKID_ERR_INTERNAL        ( (tykid_status_t)-128)

#define TYKID_ERR_THREAT_BLOCKED ( (tykid_status_t) -16)
#define TYKID_ERR_NOT_ESSENTIAL ( (tykid_status_t) -17)
#define TYKID_ERR_CERT_INVALID ( (tykid_status_t) -18)
#define TYKID_ERR_ENTROPY_ANOMALY ( (tykid_status_t) -19)
#define TYKID_ERR_ELF_MALFORMED ( (tykid_status_t) -20)
#define TYKID_ERR_PATTERN_MATCH ( (tykid_status_t) -21)
#define TYKID_ERR_NULL_PTR ( (tykid_status_t) -22)
#define TYKID_ERR_NOT_INIT ( (tykid_status_t) -23)

typedef u32 tykid_hwclass_t;
#define TYKID_HW_UNKNOWN          ((tykid_hwclass_t)0x00000000)
#define TYKID_HW_GPU_UHD          ((tykid_hwclass_t)0x00010001)
#define TYKID_HW_GPU_DGPU         ((tykid_hwclass_t)0x00010002)
#define TYKID_HW_NIC_REALTEK      ((tykid_hwclass_t)0x00020001)
#define TYKID_HW_NIC_INTEL        ((tykid_hwclass_t)0x00020002)
#define TYKID_HW_NIC_GENERIC      ((tykid_hwclass_t)0x00020003)
#define TYKID_HW_AUDIO_HDA        ((tykid_hwclass_t)0x00030001)
#define TYKID_HW_AUDIO_USB        ((tykid_hwclass_t)0x00030002)
#define TYKID_HW_STORAGE_NVME     ((tykid_hwclass_t)0x00040001)
#define TYKID_HW_STORAGE_SATA     ((tykid_hwclass_t)0x00040002)
#define TYKID_HW_USB_XHCI         ((tykid_hwclass_t)0x00050001)
#define TYKID_HW_USB_EHCI         ((tykid_hwclass_t)0x00050002)
#define TYKID_HW_INPUT_KBD        ((tykid_hwclass_t)0x00060001)
#define TYKID_HW_INPUT_MOUSE      ((tykid_hwclass_t)0x00060002)
#define TYKID_HW_SMBUS            ((tykid_hwclass_t)0x00070001)
#define TYKID_HW_THERMAL          ((tykid_hwclass_t)0x00080001)

#define TYKID_HW_PCI_BUS          ((tykid_hwclass_t)0x00090001)
#define TYKID_HW_ACPI_BUS         ((tykid_hwclass_t)0x00090002)
#define TYKID_HW_ISA_BUS          ((tykid_hwclass_t)0x00090003)
#define TYKID_HW_TIMER            ((tykid_hwclass_t)0x000A0001)
#define TYKID_HW_INTERRUPT_CTRL   ((tykid_hwclass_t)0x000A0002)
#define TYKID_HW_BT               ((tykid_hwclass_t)0x000C0001)

#define TYKID_HW_CLASS_ANY ((tykid_hwclass_t)0x00000000)
#define TYKID_HW_CLASS_PCI_BRIDGE ((tykid_hwclass_t)0xFF000001)
#define TYKID_HW_CLASS_STORAGE_NVME   TYKID_HW_STORAGE_NVME
#define TYKID_HW_CLASS_STORAGE_SATA   TYKID_HW_STORAGE_SATA
#define TYKID_HW_CLASS_STORAGE_USB ((tykid_hwclass_t)0x00040003)
#define TYKID_HW_CLASS_NIC            TYKID_HW_NIC_GENERIC
#define TYKID_HW_CLASS_USB_HOST       TYKID_HW_USB_XHCI
#define TYKID_HW_CLASS_DISPLAY        TYKID_HW_GPU_UHD
#define TYKID_HW_CLASS_INPUT          TYKID_HW_INPUT_KBD
#define TYKID_HW_CLASS_SMBUS          TYKID_HW_SMBUS
#define TYKID_HW_CLASS_IOMMU ((tykid_hwclass_t)0x000B0001)
#define TYKID_HW_CLASS_AUDIO          TYKID_HW_AUDIO_HDA
#define TYKID_HW_CLASS_THERMAL        TYKID_HW_THERMAL
#define TYKID_HW_CLASS_BT             TYKID_HW_BT

typedef u8 tykid_drv_state_t;
#define TYKID_DRV_STATE_ABSENT 0x00
#define TYKID_DRV_STATE_PENDING 0x01
#define TYKID_DRV_STATE_LOADING 0x02
#define TYKID_DRV_STATE_ACTIVE 0x03
#define TYKID_DRV_STATE_FAILED 0x04
#define TYKID_DRV_STATE_SUSPENDED 0x05
#define TYKID_DRV_STATE_UNLOADED 0x06
#define TYKID_DRV_STATE_BLOCKED 0x07
#define TYKID_DRV_STATE_SKIPPED 0x08

#define TYKID_DRV_STATE_REGISTERED  TYKID_DRV_STATE_PENDING

typedef u8 ty_threat_class_t;
#define TY_THREAT_NONE ((ty_threat_class_t)0x00)
#define TY_THREAT_SUSPICIOUS ((ty_threat_class_t)0x01)
#define TY_THREAT_UNSIGNED ((ty_threat_class_t)0x02)
#define TY_THREAT_RUBBISH ((ty_threat_class_t)0x03)
#define TY_THREAT_TAMPERED ((ty_threat_class_t)0x04)
#define TY_THREAT_UNKNOWN ((ty_threat_class_t)0x05)
#define TY_THREAT_MALWARE ((ty_threat_class_t)0x06)
#define TY_THREAT_REVOKED ((ty_threat_class_t)0x07)

#define TY_THREAT_BLOCK_THRESHOLD  TY_THREAT_RUBBISH

#define TYKID_MAX_HW_DEVICES      64U
#define TYKID_MAX_DRIVERS         128U
#define TYKID_MAX_DEPS            16U
#define TYKID_MAX_PATH            256U
#define TYKID_MAX_NAME            64U
#define TYKID_ENTROPY_POOL_BYTES  64U
#define TYKID_HMAC_DIGEST_BYTES   32U
#define TYKID_GATE_MAGIC          0xDEAD4B42UL
#define TYKID_MAX_CERT_DER_BYTES 4096U

typedef struct TYKID_PACKED {
 char driver_name[TYKID_MAX_NAME];
 ty_threat_class_t threat_class;
 u8 stage_reached;
 u16 flags;
 u32 scan_duration_us;
 u32 entropy_x1000;
 tykid_status_t detail_code;
 char detail_msg[64];
 u8 sha256_digest[32];
 u8 computed_hmac[TYKID_HMAC_DIGEST_BYTES];
 bool8 cert_verified;
    u8                _pad[3];
 u32 pattern_offset;
 u32 pattern_index;
 u32 elf_anomalies;
} tykid_threat_report_t;

#define TY_STAGE_ELF_VALID      0x01U
#define TY_STAGE_ENTROPY_OK     0x02U
#define TY_STAGE_HMAC_OK        0x04U
#define TY_STAGE_BEARSSL_HASH   0x08U
#define TY_STAGE_BEARSSL_SIG    0x10U
#define TY_STAGE_PATTERN_SCAN   0x20U
#define TY_STAGE_ALL            0x3FU

#define TY_THREAT_FLAG_PACKED 0x0001U
#define TY_THREAT_FLAG_STRIPPED 0x0002U
#define TY_THREAT_FLAG_CERT_OK 0x0004U
#define TY_THREAT_FLAG_CERT_EXP 0x0008U
#define TY_THREAT_FLAG_ROOTKIT 0x0010U
#define TY_THREAT_FLAG_SELF_MOD 0x0020U
#define TY_THREAT_FLAG_SCAN_ERR 0x8000U

typedef u32 ty_hw_essential_mask_t;
#define TY_ESSENTIAL_NONE         0x00000000U
#define TY_ESSENTIAL_PCI_BUS 0x00000001U
#define TY_ESSENTIAL_ACPI 0x00000002U
#define TY_ESSENTIAL_INTERRUPT 0x00000004U
#define TY_ESSENTIAL_TIMER 0x00000008U
#define TY_ESSENTIAL_STORAGE_NVME 0x00000010U
#define TY_ESSENTIAL_STORAGE_SATA 0x00000020U
#define TY_ESSENTIAL_USB_HOST 0x00000040U
#define TY_ESSENTIAL_NIC 0x00000080U
#define TY_ESSENTIAL_FRAMEBUF 0x00000100U
#define TY_ESSENTIAL_KBD 0x00000200U
#define TY_ESSENTIAL_SMBUS 0x00000400U
#define TY_ESSENTIAL_IOMMU 0x00000800U

#define TY_ESSENTIAL_NEVER_MASK ( \
 0U  )

#define TYKID_DRV_FLAG_NONE        0x00000000U
#define TYKID_DRV_FLAG_CRITICAL 0x00000001U
#define TYKID_DRV_FLAG_LAZY 0x00000002U
#define TYKID_DRV_FLAG_SINGLETON 0x00000004U
#define TYKID_DRV_FLAG_VERIFIED 0x00000008U
#define TYKID_DRV_FLAG_BLACKLISTED 0x00000010U
#define TYKID_DRV_FLAG_FALLBACK 0x00000020U
#define TYKID_DRV_FLAG_EXCLUSIVE 0x00000040U
#define TYKID_DRV_FLAG_POWER_OPT 0x00000080U
#define TYKID_DRV_FLAG_THREAT_OK 0x00000100U
#define TYKID_DRV_FLAG_ESSENTIAL 0x00000200U
#define TYKID_DRV_FLAG_BUILTIN 0x00000400U
#define TYKID_DRV_FLAG_CERT_SIGNED 0x00000800U
#define TYKID_DRV_FLAG_THIRD_PARTY 0x00001000U
#define TYKID_DRV_FLAG_SCAN_DONE 0x00002000U

typedef struct TYKID_PACKED tykid_hw_device {
    u32              vendor_id;
    u32              device_id;
    u32              subsys_id;
 u32 class_code;
    tykid_hwclass_t  ty_class;
    u8               bus;
    u8               slot;
    u8               func;
    u8               irq;
    u64              mmio_base;
    u64              mmio_size;
 u64 hardware_fingerprint;
    char             name[TYKID_MAX_NAME];
} tykid_hw_device_t;

typedef struct TYKID_PACKED tykid_driver_desc {
    char                  name[TYKID_MAX_NAME];
    char                  path[TYKID_MAX_PATH];
    tykid_hwclass_t       hw_classes[8];
    u8                    hw_class_count;
    u32                   vendor_mask;
    u32                   device_mask;
    u32                   flags;
    u16                   version_major;
    u16                   version_minor;
 u8 deps[TYKID_MAX_DEPS];
    u8                    dep_count;
    u8                    priority;
    u8                    hmac[TYKID_HMAC_DIGEST_BYTES];
    tykid_drv_state_t     state;
 ty_threat_class_t threat_class;
    u64                   load_timestamp;
    uptr                  base_vaddr;
 tykid_threat_report_t threat_report;
 const u8 *cert_der;
    usz                   cert_der_len;
 const u8 *sig_der;
    usz                   sig_der_len;
} tykid_driver_desc_t;

typedef struct tykid_gate_ctx tykid_gate_ctx_t;

typedef struct {
    tykid_hw_device_t devices[TYKID_MAX_HW_DEVICES];
    u32               count;
    u64               enum_timestamp;
    u64               bus_topology_hash;
    ty_hw_essential_mask_t essential_mask;
    bool8             iommu_active;
} tykid_hw_enumset_t;

typedef struct {
    u32                   loaded_count;
 u32 skipped_count;
 u32 failed_count;
 u32 blocked_count;
    tykid_driver_desc_t  *loaded[TYKID_MAX_DRIVERS];
 tykid_driver_desc_t *blocked[TYKID_MAX_DRIVERS];
    tykid_status_t        status_per[TYKID_MAX_DRIVERS];
} tykid_load_result_t;

typedef void  (*tykid_log_fn_t)(u8 level, const char *fmt, ...);
typedef void *(*tykid_alloc_fn_t)(usz size);
typedef void  (*tykid_free_fn_t)(void *ptr, usz size);
typedef usz   (*tykid_entropy_fn_t)(void *buf, usz len);
typedef tykid_status_t (*tykid_ko_load_fn_t)(const char *path, uptr *base_out);
typedef tykid_status_t (*tykid_ko_unload_fn_t)(uptr base);
typedef tykid_status_t (*tykid_path_iter_fn_t)(const char *dir,
                            bool8 (*cb)(const char *entry, void *priv),
                            void *priv);
typedef void           (*tykid_iommu_block_fn_t)(uptr base_vaddr);

typedef tykid_status_t (*tykid_vfs_read_fn_t)(const char *path,
                                               u8 **buf_out, usz *len_out);

typedef struct {
 const char *drivers_dir;
    tykid_log_fn_t       log_fn;
    tykid_alloc_fn_t     alloc_fn;
    tykid_free_fn_t      free_fn;
    tykid_entropy_fn_t   entropy_fn;
    tykid_ko_load_fn_t   ko_load_fn;
    tykid_ko_unload_fn_t ko_unload_fn;
    tykid_path_iter_fn_t path_iter_fn;
 tykid_vfs_read_fn_t vfs_read_fn;
    tykid_iommu_block_fn_t iommu_block_fn;
    u64                  kobalt_boot_token;
    u8                   hmac_master_key[32];

    const u8            *trust_anchor_der;
    usz                  trust_anchor_der_len;
    u32                  flags;
    u8                   max_load_attempts;
    u32                  probe_timeout_ms;

 double entropy_block_threshold;
 bool8 block_unsigned;
 bool8 block_suspicious;
 bool8 load_non_essential;
} tykid_config_t;

TYKID_API TYKID_WARN_UNUSED
tykid_status_t  tykid_init(const tykid_config_t *cfg, tykid_gate_ctx_t **ctx_out);

TYKID_API void  tykid_shutdown(tykid_gate_ctx_t *ctx);

TYKID_API const char *tykid_version_string(void);

TYKID_API TYKID_WARN_UNUSED
tykid_status_t  tykid_verify_seal(const tykid_gate_ctx_t *ctx);

TYKID_API TYKID_WARN_UNUSED
tykid_status_t  tykid_enumerate_hardware(tykid_gate_ctx_t *ctx,
                                          tykid_hw_enumset_t *out);

TYKID_API TYKID_WARN_UNUSED
tykid_status_t  tykid_fingerprint_device(tykid_gate_ctx_t *ctx,
                                          tykid_hw_device_t *dev);

TYKID_API TYKID_WARN_UNUSED
tykid_status_t  tykid_scan_driver(tykid_gate_ctx_t *ctx,
                                   tykid_driver_desc_t *drv,
                                   tykid_threat_report_t *report_out);

TYKID_API TYKID_WARN_UNUSED
tykid_status_t  tykid_scan_all(tykid_gate_ctx_t *ctx,
                                u32 *blocked_count_out,
                                u32 *clean_count_out);

TYKID_API ty_hw_essential_mask_t
tykid_compute_essential_mask(tykid_gate_ctx_t *ctx,
                              const tykid_hw_enumset_t *hw);

TYKID_API bool8
tykid_driver_is_essential(const tykid_driver_desc_t *drv,
                           ty_hw_essential_mask_t mask);

TYKID_API TYKID_WARN_UNUSED
tykid_status_t  tykid_arbitrate(tykid_gate_ctx_t *ctx,
                                 const tykid_hw_enumset_t *hw,
                                 tykid_load_result_t *result);

TYKID_API TYKID_WARN_UNUSED
tykid_status_t  tykid_load_driver(tykid_gate_ctx_t *ctx, const char *name);

TYKID_API TYKID_WARN_UNUSED
tykid_status_t  tykid_unload_driver(tykid_gate_ctx_t *ctx, const char *name);

TYKID_API TYKID_WARN_UNUSED
tykid_status_t  tykid_blacklist_driver(tykid_gate_ctx_t *ctx, const char *name);

TYKID_API tykid_drv_state_t
                tykid_driver_state(const tykid_gate_ctx_t *ctx,
                                   const char *name);

TYKID_API u32   tykid_active_driver_count(const tykid_gate_ctx_t *ctx);
TYKID_API u32   tykid_blocked_driver_count(const tykid_gate_ctx_t *ctx);
TYKID_API u32   tykid_skipped_driver_count(const tykid_gate_ctx_t *ctx);

TYKID_API tykid_status_t
                tykid_dump_state(const tykid_gate_ctx_t *ctx,
                                 char *buf, usz buf_sz);

TYKID_API usz
                tykid_dump_threats(const tykid_gate_ctx_t *ctx,
                                   char *buf, usz buf_sz);

TYKID_API TYKID_WARN_UNUSED
tykid_status_t  tykid_verify_driver_integrity(tykid_gate_ctx_t *ctx,
                                               const tykid_driver_desc_t *drv);

TYKID_API TYKID_WARN_UNUSED
tykid_status_t  tykid_recheck_all(tykid_gate_ctx_t *ctx);

TYKID_API const char *tykid_strerror(tykid_status_t st);
TYKID_API const char *tykid_threat_name(ty_threat_class_t cls);

#ifdef __cplusplus
}
#endif
#endif
