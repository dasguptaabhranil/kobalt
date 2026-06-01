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

#ifndef __TYKID_INTERNAL_H__
#define __TYKID_INTERNAL_H__

#include "tykid.h"

#include "../../bearssl/inc/bearssl_hash.h"
#include "../../bearssl/inc/bearssl_ec.h"
#include "../../bearssl/inc/bearssl_hmac.h"
#include "../../bearssl/port/br_kobalt.h"

#define TY_LOG_TRACE   0
#define TY_LOG_DEBUG   1
#define TY_LOG_INFO    2
#define TY_LOG_WARN    3
#define TY_LOG_ERROR   4
#define TY_LOG_FATAL   5

extern int ksnprintf(char *buf, size_t n, const char *fmt, ...);

#define TY_LOG_BUFSZ  192U

#define TY_LOG(ctx, lvl, fmt, ...) \
    do { if (__ty_likely((ctx)->cfg.log_fn)) { \
        char _ty_msg[TY_LOG_BUFSZ]; \
        ksnprintf(_ty_msg, TY_LOG_BUFSZ, \
                  "[TYKID/%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__); \
        (ctx)->cfg.log_fn((lvl), _ty_msg); \
    } } while(0)

#define TY_ALLOC(ctx, sz)     ((ctx)->cfg.alloc_fn((sz)))
#define TY_FREE(ctx, ptr, sz) ((ctx)->cfg.free_fn((ptr), (sz)))

typedef volatile u32 ty_spinlock_t;
#define TY_SPIN_INIT  0U

static TYKID_ALWAYS_INL void ty_spin_lock(ty_spinlock_t *l) {
    while (__sync_lock_test_and_set(l, 1U))
        __asm__ volatile("pause" ::: "memory");
}
static TYKID_ALWAYS_INL void ty_spin_unlock(ty_spinlock_t *l) {
    __sync_lock_release(l);
}

extern u64 kobalt_tsc_read(void);

extern void kobalt_log_critical(const char *tag, const void *rec, usz len);

#define TYKID_DRIVER_LOAD_TIMEOUT_MS  0U

static TYKID_ALWAYS_INL void ty_memzero_secure(void *ptr, usz len) {
    volatile u8 *p = (volatile u8 *)ptr;
    while (len--) *p++ = 0;
}

static TYKID_ALWAYS_INL void ty_memcpy(void *dst, const void *src, usz n) {
    u8 *d = (u8 *)dst; const u8 *s = (const u8 *)src;
    while (n--) *d++ = *s++;
}

static TYKID_ALWAYS_INL bool8 ty_memeq(const void *a, const void *b, usz n) {
    const u8 *p = (const u8 *)a, *q = (const u8 *)b;
    u8 diff = 0;
    for (usz i = 0; i < n; i++) diff |= p[i] ^ q[i];
    return diff == 0 ? TYKID_TRUE : TYKID_FALSE;
}

static TYKID_ALWAYS_INL usz ty_strlen(const char *s) {
    usz n = 0; while (s[n]) n++; return n;
}

static TYKID_ALWAYS_INL void ty_strncpy(char *dst, const char *src, usz n) {
    usz i = 0;
    for (; i < n - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

static TYKID_ALWAYS_INL s32 ty_strncmp(const char *a, const char *b, usz n) {
    for (usz i = 0; i < n; i++) {
        if (a[i] != b[i]) return (u8)a[i] - (u8)b[i];
        if (!a[i]) return 0;
    } return 0;
}

static TYKID_ALWAYS_INL u32 ty_rotl32(u32 v, u32 n) { return (v << n) | (v >> (32u - n)); }
static TYKID_ALWAYS_INL u32 ty_rotr32(u32 v, u32 n) { return (v >> n) | (v << (32u - n)); }
static TYKID_ALWAYS_INL u64 ty_rotl64(u64 v, u32 n) { return (v << n) | (v >> (64u - n)); }
static TYKID_ALWAYS_INL u64 ty_rotr64(u64 v, u32 n) { return (v >> n) | (v << (64u - n)); }

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#  define TY_LE16(x)  (x)
#  define TY_LE32(x)  (x)
#  define TY_LE64(x)  (x)
#  define TY_BE16(x)  __builtin_bswap16(x)
#  define TY_BE32(x)  __builtin_bswap32(x)
#  define TY_BE64(x)  __builtin_bswap64(x)
#else
#  define TY_LE16(x)  __builtin_bswap16(x)
#  define TY_LE32(x)  __builtin_bswap32(x)
#  define TY_LE64(x)  __builtin_bswap64(x)
#  define TY_BE16(x)  (x)
#  define TY_BE32(x)  (x)
#  define TY_BE64(x)  (x)
#endif

static TYKID_ALWAYS_INL u32 ty_get_u32le(const u8 *p) {
    return (u32)p[0] | ((u32)p[1]<<8) | ((u32)p[2]<<16) | ((u32)p[3]<<24);
}
static TYKID_ALWAYS_INL void ty_put_u32le(u8 *p, u32 v) {
    p[0]=(u8)v; p[1]=(u8)(v>>8); p[2]=(u8)(v>>16); p[3]=(u8)(v>>24);
}
static TYKID_ALWAYS_INL u64 ty_get_u64le(const u8 *p) {
    return (u64)ty_get_u32le(p) | ((u64)ty_get_u32le(p+4) << 32);
}
static TYKID_ALWAYS_INL void ty_put_u64le(u8 *p, u64 v) {
    ty_put_u32le(p,(u32)v); ty_put_u32le(p+4,(u32)(v>>32));
}

typedef struct TYKID_ALIGNED(64) {
    u64   state[8];
    u64   counter;
    u64   seed_epoch;
    bool8 seeded;
} ty_entropy_pool_t;

typedef struct { u64 v0, v1, v2, v3; } ty_siphash_ctx_t;

#define BLAKE2S_BLOCKBYTES  64U
#define BLAKE2S_OUTBYTES    32U
typedef struct TYKID_ALIGNED(16) {
    u32 h[8];
    u32 t[2];
    u32 f[2];
    u8  buf[BLAKE2S_BLOCKBYTES];
    u32 buflen;
    u8  outlen;
} ty_blake2s_ctx_t;

typedef struct {
    u32 state[16];
    u8  keystream[64];
    u32 keystream_pos;
} ty_chacha20_ctx_t;

typedef struct {
    br_sha256_context sha256;
    br_hmac_context   hmac;
    br_hmac_key_context hmac_key;
    br_ec_public_key  ec_pub;
    u8   ec_pub_buf[65];
    u8   sig_buf[72];
    usz  sig_len;
    u8   sha256_digest[32];
    bool8 initialised;
} ty_bearssl_scan_state_t;

#define TY_BAD_PATTERN_COUNT  52U
typedef struct {
    u64 hash;
    u32 flags;
} ty_bad_pattern_entry_t;

#define TY_ELF_MAGIC     0x464C457FU
#define TY_ET_REL        1U
#define TY_ET_EXEC       2U
#define TY_ET_DYN        3U
#define TY_EM_X86_64     62U
#define TY_ELF_CLASS64   2U

typedef struct TYKID_PACKED {
    u8  e_ident[16];
    u16 e_type;
    u16 e_machine;
    u32 e_version;
    u64 e_entry;
    u64 e_phoff;
    u64 e_shoff;
    u32 e_flags;
    u16 e_ehsize;
    u16 e_phentsize;
    u16 e_phnum;
    u16 e_shentsize;
    u16 e_shnum;
    u16 e_shstrndx;
} ty_elf64_hdr_t;

typedef struct TYKID_PACKED {
    u32 p_type;
    u32 p_flags;
    u64 p_offset;
    u64 p_vaddr;
    u64 p_paddr;
    u64 p_filesz;
    u64 p_memsz;
    u64 p_align;
} ty_elf64_phdr_t;

#define TY_PT_LOAD  1U

typedef struct {
    u32 counts[256];
    usz total;
    u32 frac_x1000;
} ty_entropy_hist_t;

#define TY_ENTROPY_FLAG_THRESHOLD_DEFAULT  7500U

#define TY_DEP_WORDS  ((TYKID_MAX_DRIVERS + 63) / 64)

typedef struct {
    u64 adj[TYKID_MAX_DRIVERS][TY_DEP_WORDS];
    u8  in_degree[TYKID_MAX_DRIVERS];
    u8  topo[TYKID_MAX_DRIVERS];
    u8  topo_count;
    u8  node_count;
} ty_dep_graph_t;

typedef struct {
    tykid_driver_desc_t entries[TYKID_MAX_DRIVERS];
    u32                 count;
    u64                 scan_timestamp;
    u64                 dir_hash;
    ty_dep_graph_t      deps;
    ty_spinlock_t       lock;
} ty_driver_registry_t;

typedef struct {
    tykid_hw_enumset_t  enumset;
    u64                 probe_hash;
    bool8               probed;
} ty_hw_probe_t;

typedef struct {
    u32   scanned;
    u32   clean;
    u32   blocked;
    u32   skipped;
    u32   suspicious;
    u32   unsigned_count;
    bool8 scan_complete;
} ty_scan_summary_t;

typedef struct {
    u64 mmio_access_count;
    u64 dma_bytes_issued;
    u64 irq_count;
    u64 policy_violations;
    u64 last_tsc;
    bool8 active;
} ty_sandbox_state_t;

#define TY_CTX_MAGIC_A  (0xDEAD4B42UL ^ (u32)(__TYKID_KOBALT_EXPECTED & 0xFFFFFFFFUL))
#define TY_CTX_MAGIC_B  (0xBEEF0000UL ^ (u32)((__TYKID_KOBALT_EXPECTED >> 32) & 0xFFFFFFFFUL))

struct TYKID_ALIGNED(4096) tykid_gate_ctx {
    u32  magic_a;
    u32  magic_b;
    u64  seal_check;
    u64  boot_token;

    tykid_config_t          cfg;

    ty_entropy_pool_t       entropy;
    ty_chacha20_ctx_t       csprng;
    u8  session_key[32];
    u8  hmac_key[32];

    ty_hw_probe_t           hw;
    ty_driver_registry_t    reg;

    ty_bearssl_scan_state_t threat;
    ty_scan_summary_t       scan_summary;

    ty_hw_essential_mask_t  essential_mask;

    ty_sandbox_state_t      sandbox[TYKID_MAX_DRIVERS];

    u32   total_loaded;
    u32   total_skipped;
    u32   total_failed;
    u32   total_blocked;
    u32   total_revoked;
    u32   recheck_count;
    u64   init_timestamp;

    bool8 safe_mode;
    u8    _pad[7];
};

TYKID_INTERNAL tykid_status_t ty_entropy_seed(tykid_gate_ctx_t *ctx);
TYKID_INTERNAL u64  ty_entropy_u64(tykid_gate_ctx_t *ctx);
TYKID_INTERNAL void ty_entropy_fill(tykid_gate_ctx_t *ctx, void *buf, usz len);

TYKID_INTERNAL void ty_blake2s_init(ty_blake2s_ctx_t *S, const u8 *key, u8 klen, u8 outlen);
TYKID_INTERNAL void ty_blake2s_update(ty_blake2s_ctx_t *S, const u8 *in, usz inlen);
TYKID_INTERNAL void ty_blake2s_final(ty_blake2s_ctx_t *S, u8 *out);
TYKID_INTERNAL void ty_hmac_blake2s(const u8 *key, usz klen, const u8 *msg, usz mlen,
                                     u8 out[BLAKE2S_OUTBYTES]);
TYKID_INTERNAL u64  ty_siphash24(const u8 *key16, const void *data, usz len);
TYKID_INTERNAL void ty_chacha20_init(ty_chacha20_ctx_t *ctx, const u8 key[32], const u8 nonce[8]);
TYKID_INTERNAL void ty_chacha20_fill(ty_chacha20_ctx_t *ctx, u8 *buf, usz len);
TYKID_INTERNAL tykid_status_t ty_derive_session_keys(tykid_gate_ctx_t *ctx);

TYKID_INTERNAL tykid_status_t ty_hw_pci_enumerate(tykid_gate_ctx_t *ctx, tykid_hw_enumset_t *out);
TYKID_INTERNAL tykid_hwclass_t ty_hw_classify(u32 vendor, u32 device, u32 class_code);
TYKID_INTERNAL u64 ty_hw_fingerprint_one(const tykid_hw_device_t *dev, const u8 *session_key);
TYKID_INTERNAL u64 ty_hw_topology_hash(const tykid_hw_enumset_t *hw, const u8 *session_key);

TYKID_INTERNAL tykid_status_t ty_registry_scan(tykid_gate_ctx_t *ctx);
TYKID_INTERNAL tykid_status_t ty_registry_parse_manifest(tykid_gate_ctx_t *ctx,
                                   const char *manifest_path, tykid_driver_desc_t *out);
TYKID_INTERNAL tykid_status_t ty_dep_graph_add(ty_dep_graph_t *g, u8 u, u8 v);
TYKID_INTERNAL tykid_status_t ty_dep_graph_toposort(ty_dep_graph_t *g);

TYKID_INTERNAL bool8 ty_driver_matches_hw(const tykid_driver_desc_t *drv,
                                           const tykid_hw_device_t *dev);
TYKID_INTERNAL tykid_status_t ty_gate_load_one(tykid_gate_ctx_t *ctx,
                                                tykid_driver_desc_t *drv);
TYKID_INTERNAL tykid_status_t ty_integrity_check_file(tykid_gate_ctx_t *ctx,
                                   const char *path,
                                   const u8 expected_hmac[TYKID_HMAC_DIGEST_BYTES]);
TYKID_INTERNAL s32 ty_find_driver_by_name(const tykid_gate_ctx_t *ctx, const char *name);

TYKID_INTERNAL tykid_status_t ty_bearssl_scan_init(tykid_gate_ctx_t *ctx);
TYKID_INTERNAL tykid_status_t ty_bearssl_scan_reset(tykid_gate_ctx_t *ctx);
TYKID_INTERNAL ty_threat_class_t ty_scan_stage1_elf(tykid_gate_ctx_t *ctx,
                                   const u8 *image, usz image_len, tykid_threat_report_t *r);
TYKID_INTERNAL ty_threat_class_t ty_scan_stage2_entropy(tykid_gate_ctx_t *ctx,
                                   const u8 *image, usz image_len, tykid_threat_report_t *r);
TYKID_INTERNAL ty_threat_class_t ty_scan_stage3_hmac(tykid_gate_ctx_t *ctx,
                                   tykid_driver_desc_t *drv, const u8 *image, usz image_len,
                                   tykid_threat_report_t *r);
TYKID_INTERNAL ty_threat_class_t ty_scan_stage4_sha256(tykid_gate_ctx_t *ctx,
                                   const u8 *image, usz image_len, tykid_threat_report_t *r);
TYKID_INTERNAL ty_threat_class_t ty_scan_stage5_cert_pattern(tykid_gate_ctx_t *ctx,
                                   tykid_driver_desc_t *drv, const u8 *image, usz image_len,
                                   tykid_threat_report_t *r);
TYKID_INTERNAL tykid_status_t ty_vfs_read_driver(tykid_gate_ctx_t *ctx,
                                   const char *path, u8 **buf_out, usz *len_out);
TYKID_INTERNAL void ty_vfs_free_driver(tykid_gate_ctx_t *ctx, u8 *buf, usz len);

TYKID_INTERNAL ty_hw_essential_mask_t ty_selector_compute_mask(tykid_gate_ctx_t *ctx,
                                   const tykid_hw_enumset_t *hw);
TYKID_INTERNAL bool8 ty_selector_driver_needed(const tykid_driver_desc_t *drv,
                                   ty_hw_essential_mask_t mask);
TYKID_INTERNAL const char *ty_selector_essential_flag_name(u32 flag);

TYKID_INTERNAL tykid_status_t ty_audit_init(tykid_gate_ctx_t *ctx);
TYKID_INTERNAL tykid_status_t ty_audit_verify_chain(tykid_gate_ctx_t *ctx);

TYKID_INTERNAL void ty_sandbox_record_access(tykid_gate_ctx_t *ctx, u32 drv_idx,
                                              u8 kind, u64 addr, usz len);
TYKID_INTERNAL u64  ty_sandbox_violation_count(const tykid_gate_ctx_t *ctx, u32 drv_idx);

TYKID_API tykid_status_t tykid_attest_export(tykid_gate_ctx_t *ctx,
                                              u8 *buf, usz buf_sz, usz *written);

TYKID_API tykid_status_t tykid_revoke_driver(tykid_gate_ctx_t *ctx, const char *name);

TYKID_INTERNAL void ty_ctx_poison(tykid_gate_ctx_t *ctx);
TYKID_INTERNAL TYKID_NORETURN void ty_panic(tykid_gate_ctx_t *ctx, const char *reason);

#endif
