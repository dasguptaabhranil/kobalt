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

static const s32 ty_log2_lut[256] = {
  0,
  -2048,

  -1024,

 0
};

static u32 ty_compute_shannon(const u32 counts[256], usz total) {
    if (total == 0) return 0;

    u64 acc = 0;
    for (u32 i = 0; i < 256; i++) {
        if (counts[i] == 0) continue;

        u32 c   = counts[i];
        u32 k   = 0;
        u32 tmp = c;
 while (tmp > 1) { tmp >>= 1; k++; }
 u32 frac = ((c - (1u << k)) << 16) >> k;
 u32 lg2c = (k << 16) + frac;

        u32 lt   = 0;
        tmp = (u32)total;
        while (tmp > 1) { tmp >>= 1; lt++; }
        u32 lfrac = (((u32)total - (1u << lt)) << 16) >> lt;
        u32 lg2t  = (lt << 16) + lfrac;

        s32 log2p_q16 = (s32)lg2c - (s32)lg2t;

        acc += (u64)c * (u64)(u32)(-log2p_q16);
    }

    u64 h_q16 = (acc * 1000u) / ((u64)total);
    u32 h_x1000 = (u32)(h_q16 >> 16);

    if (h_x1000 > 8000u) h_x1000 = 8000u;
    return h_x1000;
}

static const ty_bad_pattern_entry_t ty_bad_patterns[TY_BAD_PATTERN_COUNT] = {

    { 0x7AE8B4C2F1093D56ULL, 0 }, { 0x3F91A0E64D28C7B5ULL, 0 },
    { 0xC8052B7E39F4A16DULL, 0 }, { 0x514D98076BE3F2CAULL, 0 },
    { 0xA27F3C85E140B96EULL, 0 }, { 0x6B0E47D9F28135ACULL, 0 },

    { 0xD319C4B7820E5F61ULL, 0 }, { 0x4E8A2F0593D76CB1ULL, 0 },
    { 0x97B65014FCA38E2DULL, 0 }, { 0x2C0F83A1E74B59D6ULL, 0 },
    { 0x5819D3E7A406BC92ULL, 0 }, { 0xF20C614BE8973A05ULL, 0 },
    { 0x3A7E50D94F182C6BULL, 0 }, { 0x8D24A7F630B95E1CULL, 0 },
    { 0x61FC08B5729D3A4EULL, 0 }, { 0xB493E12D508F67A0ULL, 0 },

    { 0x07A5D4E8F26C91B3ULL, 0 }, { 0xE4B83162C097F5DAULL, 0 },
    { 0x9C271EAD504683BFULL, 0 }, { 0x35D9C6028B4FA71EULL, 0 },
    { 0x1F487B3CE509A62DULL, 0 }, { 0xAD60F295381CE740ULL, 0 },
    { 0x74B1083F9D52A6C8ULL, 0 }, { 0xC9E27A4B061FD835ULL, 0 },
    { 0x5382FD91B40E7A6CULL, 0 }, { 0xE751C04A8F239BDEULL, 0 },
    { 0x281D96B7C4F50E3AULL, 0 }, { 0x6FA34E80D21795CBULL, 0 },
    { 0xB0D857129EC3F46AULL, 0 }, { 0x4C71A25FD3089EB6ULL, 0 },

    { 0x3E0B94D5A7F26C18ULL, 0 }, { 0x927C4138E5BD06F4ULL, 0 },
    { 0xDA3F7861B20C94E5ULL, 0 }, { 0x5B8E034C6F91A27DULL, 0 },
    { 0x109F256DCB84E3A7ULL, 0 }, { 0xF3A78B02E461D59CULL, 0 },

    { 0x6C5D30E9F71AB284ULL, 0 }, { 0xB82F14A7CE60935DULL, 0 },
    { 0x4A913D72BF08E56CULL, 0 }, { 0xD50C86F4E392A17BULL, 0 },
    { 0x73E14B9D208FC65AULL, 0 }, { 0x1B6F28D5A94703ECULL, 0 },

    { 0xC0748EAF1D5392B6ULL, 0 }, { 0x8A21F07B43CE985DULL, 0 },
    { 0x56B93CAE28F1047DULL, 0 }, { 0xE07D514FA8923CB6ULL, 0 },

    { 0x29E40B76F83D51ACULL, 0 }, { 0x7C1A93E5D04F628BULL, 0 },
    { 0xB5D28C0F7E14A369ULL, 0 }, { 0x03E5A17C9F482DB6ULL, 0 },
    { 0x91F7D304B65EC82AULL, 0 }, { 0xA48B2E50F71D3C96ULL, 0 },
};

static const u8 ty_pattern_scan_key[16] = {0};

tykid_status_t ty_bearssl_scan_init(tykid_gate_ctx_t *ctx) {
    ty_bearssl_scan_state_t *st = &ctx->threat;
    ty_memzero_secure(st, sizeof(*st));

    br_hmac_key_init(&st->hmac_key, &br_sha256_vtable,
                     ctx->session_key, sizeof(ctx->session_key));

    st->initialised = TYKID_TRUE;
    return TYKID_OK;
}

tykid_status_t ty_bearssl_scan_reset(tykid_gate_ctx_t *ctx) {

    ty_bearssl_scan_state_t *st = &ctx->threat;
    br_sha256_init(&st->sha256);
    br_hmac_init(&st->hmac, &st->hmac_key, 0);
    ty_memzero_secure(st->sha256_digest, sizeof(st->sha256_digest));
    ty_memzero_secure(st->sig_buf, sizeof(st->sig_buf));
    st->sig_len = 0;
    return TYKID_OK;
}

tykid_status_t ty_vfs_read_driver(tykid_gate_ctx_t *ctx,
                                   const char *path,
                                   u8 **buf_out, usz *len_out) {
    if (!ctx->cfg.vfs_read_fn) {
        TY_LOG(ctx, TY_LOG_ERROR, "vfs_read_fn not configured");
        return TYKID_ERR_NOT_INIT;
    }
    return ctx->cfg.vfs_read_fn(path, buf_out, len_out);
}

void ty_vfs_free_driver(tykid_gate_ctx_t *ctx, u8 *buf, usz len) {
    if (buf && ctx->cfg.free_fn)
        ctx->cfg.free_fn(buf, len);
}

ty_threat_class_t ty_scan_stage1_elf(tykid_gate_ctx_t *ctx,
                                      const u8 *image, usz image_len,
                                      tykid_threat_report_t *report) {
    (void)ctx;

    report->stage_reached = 1;

    if (image_len < sizeof(ty_elf64_hdr_t)) {
        report->detail_code = TYKID_ERR_ELF_MALFORMED;
        ty_strncpy(report->detail_msg, "image too small for ELF64 header",
                   sizeof(report->detail_msg));
        return TY_THREAT_RUBBISH;
    }

    const ty_elf64_hdr_t *ehdr = (const ty_elf64_hdr_t *)image;

    u32 magic;
    ty_memcpy(&magic, ehdr->e_ident, 4);
    if (TY_LE32(magic) != TY_ELF_MAGIC) {
        report->detail_code = TYKID_ERR_ELF_MALFORMED;
        ty_strncpy(report->detail_msg, "ELF magic mismatch",
                   sizeof(report->detail_msg));
        return TY_THREAT_RUBBISH;
    }

    if (ehdr->e_ident[4] != TY_ELF_CLASS64) {
        report->detail_code = TYKID_ERR_ELF_MALFORMED;
        ty_strncpy(report->detail_msg, "not ELFCLASS64",
                   sizeof(report->detail_msg));
        return TY_THREAT_RUBBISH;
    }

    if (TY_LE16(ehdr->e_machine) != TY_EM_X86_64) {
        report->detail_code = TYKID_ERR_ELF_MALFORMED;
        ty_strncpy(report->detail_msg, "not EM_X86_64",
                   sizeof(report->detail_msg));
        return TY_THREAT_RUBBISH;
    }

    u16 etype = TY_LE16(ehdr->e_type);
    if (etype != TY_ET_REL && etype != TY_ET_EXEC && etype != TY_ET_DYN) {
        report->detail_code = TYKID_ERR_ELF_MALFORMED;
        ty_strncpy(report->detail_msg, "unrecognised ELF e_type",
                   sizeof(report->detail_msg));
        return TY_THREAT_RUBBISH;
    }

    u64 shoff     = TY_LE64(ehdr->e_shoff);
    u16 shentsize = TY_LE16(ehdr->e_shentsize);
    u16 shnum     = TY_LE16(ehdr->e_shnum);
    if (shoff != 0) {
        u64 sh_end = shoff + (u64)shentsize * (u64)shnum;
        if (sh_end > (u64)image_len) {
            report->detail_code = TYKID_ERR_ELF_MALFORMED;
            ty_strncpy(report->detail_msg, "section header table out of bounds",
                       sizeof(report->detail_msg));
            return TY_THREAT_RUBBISH;
        }
    }

    u64 phoff     = TY_LE64(ehdr->e_phoff);
    u16 phentsize = TY_LE16(ehdr->e_phentsize);
    u16 phnum     = TY_LE16(ehdr->e_phnum);

    if (phoff != 0 && phnum > 0) {
        if (phoff + (u64)phentsize * (u64)phnum > (u64)image_len) {
            report->detail_code = TYKID_ERR_ELF_MALFORMED;
            ty_strncpy(report->detail_msg, "program header table out of bounds",
                       sizeof(report->detail_msg));
            return TY_THREAT_RUBBISH;
        }

        for (u16 i = 0; i < phnum; i++) {
            const ty_elf64_phdr_t *ph =
                (const ty_elf64_phdr_t *)(image + phoff + (u64)phentsize * i);
            if (TY_LE32(ph->p_type) != TY_PT_LOAD) continue;

            u64 seg_off = TY_LE64(ph->p_offset);
            u64 seg_fsz = TY_LE64(ph->p_filesz);
            if (seg_fsz > 0 && seg_off + seg_fsz > (u64)image_len) {
                report->detail_code = TYKID_ERR_ELF_MALFORMED;
                ty_strncpy(report->detail_msg,
                           "PT_LOAD segment exceeds file size",
                           sizeof(report->detail_msg));
                return TY_THREAT_RUBBISH;
            }
        }
    }

    report->stage_reached = 1;
    return TY_THREAT_NONE;
}

ty_threat_class_t ty_scan_stage2_entropy(tykid_gate_ctx_t *ctx,
                                          const u8 *image, usz image_len,
                                          tykid_threat_report_t *report) {
    report->stage_reached = 2;

    u32 counts[256];
    ty_memzero_secure(counts, sizeof(counts));
    for (usz i = 0; i < image_len; i++) counts[image[i]]++;

    u32 threshold = (ctx->cfg.entropy_block_threshold > 0.0)
                    ? (u32)(ctx->cfg.entropy_block_threshold * 1000.0)
                    : TY_ENTROPY_FLAG_THRESHOLD_DEFAULT;

    u32 h = ty_compute_shannon(counts, image_len);
    report->entropy_x1000 = h;

    if (h > threshold) {
        report->detail_code = TYKID_ERR_ENTROPY_ANOMALY;
        ty_strncpy(report->detail_msg,
                   "high entropy — likely packed or encrypted",
                   sizeof(report->detail_msg));
        return TY_THREAT_SUSPICIOUS;
    }

    return TY_THREAT_NONE;
}

ty_threat_class_t ty_scan_stage3_hmac(tykid_gate_ctx_t *ctx,
                                       tykid_driver_desc_t *drv,
                                       const u8 *image, usz image_len,
                                       tykid_threat_report_t *report) {
    report->stage_reached = 3;

    u8 computed[BLAKE2S_OUTBYTES];
    ty_hmac_blake2s(ctx->hmac_key, sizeof(ctx->hmac_key),
                    image, image_len, computed);

    bool8 stored_is_zero = TYKID_TRUE;
    for (u32 i = 0; i < TYKID_HMAC_DIGEST_BYTES; i++) {
        if (drv->hmac[i] != 0) { stored_is_zero = TYKID_FALSE; break; }
    }

    if (!stored_is_zero) {
        if (!ty_memeq(computed, drv->hmac, TYKID_HMAC_DIGEST_BYTES)) {
 report->detail_code = TYKID_ERR_ELF_MALFORMED;
            ty_strncpy(report->detail_msg,
                       "HMAC-BLAKE2s mismatch — driver binary tampered",
                       sizeof(report->detail_msg));

            ty_memcpy(report->computed_hmac, computed, TYKID_HMAC_DIGEST_BYTES);
            return TY_THREAT_TAMPERED;
        }
    }

    ty_memcpy(report->computed_hmac, computed, TYKID_HMAC_DIGEST_BYTES);
    return TY_THREAT_NONE;
}

ty_threat_class_t ty_scan_stage4_sha256(tykid_gate_ctx_t *ctx,
                                         const u8 *image, usz image_len,
                                         tykid_threat_report_t *report) {
    report->stage_reached = 4;

    ty_bearssl_scan_state_t *st = &ctx->threat;

    br_sha256_init(&st->sha256);

    const usz CHUNK = 4096;
    usz remaining   = image_len;
    const u8 *ptr   = image;
    while (remaining > 0) {
        usz sz = remaining > CHUNK ? CHUNK : remaining;
        br_sha256_update(&st->sha256, ptr, sz);
        ptr       += sz;
        remaining -= sz;
    }

    br_sha256_out(&st->sha256, st->sha256_digest);
    ty_memcpy(report->sha256_digest, st->sha256_digest, 32);

    return TY_THREAT_NONE;
}

ty_threat_class_t ty_scan_stage5_cert_pattern(tykid_gate_ctx_t *ctx,
                                               tykid_driver_desc_t *drv,
                                               const u8 *image, usz image_len,
                                               tykid_threat_report_t *report) {
    report->stage_reached = 5;
    ty_bearssl_scan_state_t *st = &ctx->threat;

    bool8 sig_ok     = TYKID_FALSE;
    bool8 has_cert   = (drv->cert_der != NULL && drv->cert_der_len > 0 &&
                        drv->sig_der  != NULL && drv->sig_der_len  > 0);

    if (has_cert) {

        const u8 *cert  = drv->cert_der;
        usz       clen  = drv->cert_der_len;
        const u8 *point = NULL;

        for (usz i = 0; i + 67 <= clen; i++) {
            if (cert[i] == 0x03 && cert[i+1] == 0x42 && cert[i+2] == 0x00 &&
                cert[i+3] == 0x04) {

                point = &cert[i + 3];
                break;
            }
        }

        if (point != NULL) {
            st->ec_pub.curve = BR_EC_secp256r1;
            st->ec_pub.q     = st->ec_pub_buf;
            st->ec_pub.qlen  = 65;
            ty_memcpy(st->ec_pub_buf, point, 65);

            usz siglen = drv->sig_der_len < sizeof(st->sig_buf)
                         ? drv->sig_der_len : sizeof(st->sig_buf);
            ty_memcpy(st->sig_buf, drv->sig_der, siglen);
            st->sig_len = siglen;

            u32 vok = br_ecdsa_i31_vrfy_asn1(
                          br_ec_get_default(),
                          st->sha256_digest, 32,
                          &st->ec_pub,
                          st->sig_buf, st->sig_len);

            sig_ok = (vok == 1) ? TYKID_TRUE : TYKID_FALSE;

            if (!sig_ok) {
                report->detail_code = TYKID_ERR_CERT_INVALID;
                ty_strncpy(report->detail_msg,
                           "ECDSA-P256 signature verification failed",
                           sizeof(report->detail_msg));

                return TY_THREAT_TAMPERED;
            }

            report->cert_verified = TYKID_TRUE;
        } else {

            TY_LOG(ctx, TY_LOG_WARN,
                   "driver '%s': cert DER present but EC point not found",
                   drv->name);
            has_cert = TYKID_FALSE;
        }
    }

    if (!has_cert) {
        if (ctx->cfg.block_unsigned) {
            report->detail_code = TYKID_ERR_CERT_INVALID;
            ty_strncpy(report->detail_msg,
                       "driver has no certificate and block_unsigned is set",
                       sizeof(report->detail_msg));
            return TY_THREAT_UNSIGNED;
        }

        TY_LOG(ctx, TY_LOG_WARN,
               "driver '%s': unsigned (no cert) — allowed by policy", drv->name);
    }

    if (image_len >= 8) {
        for (usz i = 0; i + 8 <= image_len; i++) {
            u64 h = ty_siphash24(ty_pattern_scan_key, image + i, 8);
            for (u32 j = 0; j < TY_BAD_PATTERN_COUNT; j++) {
                if (ty_bad_patterns[j].hash == h) {
                    report->detail_code = TYKID_ERR_PATTERN_MATCH;
                    ty_strncpy(report->detail_msg,
                               "known malware byte pattern detected",
                               sizeof(report->detail_msg));
                    report->pattern_offset = (u32)i;
                    report->pattern_index  = j;
                    return TY_THREAT_MALWARE;
                }
            }
        }
    }

    return TY_THREAT_NONE;
}

tykid_status_t tykid_scan_driver(tykid_gate_ctx_t  *ctx,
                                  tykid_driver_desc_t *drv,
                                  tykid_threat_report_t *report_out) {
    if (!ctx || !drv) return TYKID_ERR_NULL_PTR;

    tykid_threat_report_t report;
    ty_memzero_secure(&report, sizeof(report));
    ty_strncpy(report.driver_name, drv->name, sizeof(report.driver_name));

    u8 *image   = NULL;
    usz img_len = 0;
    tykid_status_t rc = ty_vfs_read_driver(ctx, drv->path, &image, &img_len);
    if (rc != TYKID_OK || !image || img_len == 0) {
        TY_LOG(ctx, TY_LOG_ERROR, "scan_driver: failed to read '%s' (rc=%d)",
               drv->path, rc);
        report.threat_class = TY_THREAT_UNKNOWN;
        report.detail_code  = rc != TYKID_OK ? rc : TYKID_ERR_NULL_PTR;
        ty_strncpy(report.detail_msg, "could not read driver binary",
                   sizeof(report.detail_msg));
        goto finish;
    }

    if (!ctx->threat.initialised) {
        rc = ty_bearssl_scan_init(ctx);
        if (rc != TYKID_OK) {
            TY_LOG(ctx, TY_LOG_ERROR, "scan_driver: BearSSL init failed");
            report.threat_class = TY_THREAT_UNKNOWN;
            goto cleanup;
        }
    }
    ty_bearssl_scan_reset(ctx);

    ty_threat_class_t cls1 = ty_scan_stage1_elf(ctx, image, img_len, &report);
    if (cls1 >= TY_THREAT_BLOCK_THRESHOLD) {

        report.threat_class = cls1;
        TY_LOG(ctx, TY_LOG_WARN,
               "driver '%s' blocked at stage 1 (ELF malformed)", drv->name);
        goto cleanup;
    }

    ty_threat_class_t cls2 = ty_scan_stage2_entropy(ctx, image, img_len, &report);

    ty_threat_class_t cls3 = ty_scan_stage3_hmac(ctx, drv, image, img_len, &report);
    if (cls3 >= TY_THREAT_BLOCK_THRESHOLD) {
        report.threat_class = cls3;
        TY_LOG(ctx, TY_LOG_WARN,
               "driver '%s' blocked at stage 3 (HMAC tampered)", drv->name);
        goto cleanup;
    }

    ty_threat_class_t cls4 = ty_scan_stage4_sha256(ctx, image, img_len, &report);

    (void)cls4;

    ty_threat_class_t cls5 = ty_scan_stage5_cert_pattern(ctx, drv,
                                                          image, img_len, &report);

    ty_threat_class_t final_class = TY_THREAT_NONE;
#define TY_MAX_CLASS(a, b)  ((a) > (b) ? (a) : (b))
    final_class = TY_MAX_CLASS(final_class, cls1);
    final_class = TY_MAX_CLASS(final_class, cls2);
    final_class = TY_MAX_CLASS(final_class, cls3);
    final_class = TY_MAX_CLASS(final_class, cls5);
#undef TY_MAX_CLASS
    report.threat_class = final_class;

    if (final_class == TY_THREAT_SUSPICIOUS && ctx->cfg.block_suspicious) {
        TY_LOG(ctx, TY_LOG_WARN,
               "driver '%s' blocked (suspicious + block_suspicious policy)",
               drv->name);
    }

cleanup:
    ty_vfs_free_driver(ctx, image, img_len);

finish:

    drv->threat_class = report.threat_class;
    ty_memcpy(&drv->threat_report, &report, sizeof(report));

    if (report.threat_class >= TY_THREAT_BLOCK_THRESHOLD ||
        (report.threat_class == TY_THREAT_SUSPICIOUS && ctx->cfg.block_suspicious) ||
        (report.threat_class == TY_THREAT_UNSIGNED    && ctx->cfg.block_unsigned)) {
        drv->state = TYKID_DRV_STATE_BLOCKED;
    }

    if (report_out)
        ty_memcpy(report_out, &report, sizeof(report));

    TY_LOG(ctx, TY_LOG_INFO,
           "driver '%s' scan complete: class=%s state=%s",
           drv->name,
           tykid_threat_name(report.threat_class),
           drv->state == TYKID_DRV_STATE_BLOCKED ? "BLOCKED" : "PASS");

    return TYKID_OK;
}

tykid_status_t tykid_scan_all(tykid_gate_ctx_t *ctx,
                               u32 *blocked_count_out,
                               u32 *clean_count_out) {
    if (!ctx) return TYKID_ERR_NULL_PTR;

    ty_scan_summary_t *s = &ctx->scan_summary;
    ty_memzero_secure(s, sizeof(*s));

    for (u32 i = 0; i < ctx->reg.count; i++) {
        tykid_driver_desc_t *drv = &ctx->reg.entries[i];

        if (drv->state != TYKID_DRV_STATE_REGISTERED) continue;

        s->scanned++;
        tykid_threat_report_t rep;
        tykid_status_t rc = tykid_scan_driver(ctx, drv, &rep);
        if (rc != TYKID_OK) {

            TY_LOG(ctx, TY_LOG_ERROR,
                   "scan_all: tykid_scan_driver returned %d for '%s'",
                   rc, drv->name);
            drv->state      = TYKID_DRV_STATE_BLOCKED;
            drv->threat_class = TY_THREAT_UNKNOWN;
            s->blocked++;
            continue;
        }

        switch (rep.threat_class) {
        case TY_THREAT_NONE:
            s->clean++;
            break;
        case TY_THREAT_SUSPICIOUS:
            s->suspicious++;
            if (ctx->cfg.block_suspicious) s->blocked++;
            else                           s->clean++;
            break;
        case TY_THREAT_UNSIGNED:
            s->unsigned_count++;
            if (ctx->cfg.block_unsigned) s->blocked++;
            else                         s->clean++;
            break;
        default:
            s->blocked++;
            break;
        }
    }

    ctx->total_blocked += s->blocked;
    s->scan_complete    = TYKID_TRUE;

    TY_LOG(ctx, TY_LOG_INFO,
           "scan_all complete: %u scanned, %u clean, %u blocked, "
           "%u suspicious, %u unsigned",
           s->scanned, s->clean, s->blocked, s->suspicious, s->unsigned_count);

    if (blocked_count_out) *blocked_count_out = s->blocked;
    if (clean_count_out)   *clean_count_out   = s->clean;

    return TYKID_OK;
}

u32 tykid_blocked_driver_count(const tykid_gate_ctx_t *ctx) {
    if (!ctx) return 0;
    return ctx->total_blocked;
}

u32 tykid_skipped_driver_count(const tykid_gate_ctx_t *ctx) {
    if (!ctx) return 0;
    return ctx->total_skipped;
}

const char *tykid_threat_name(ty_threat_class_t cls) {
    switch (cls) {
    case TY_THREAT_NONE:       return "NONE";
    case TY_THREAT_SUSPICIOUS: return "SUSPICIOUS";
    case TY_THREAT_UNSIGNED:   return "UNSIGNED";
    case TY_THREAT_RUBBISH:    return "RUBBISH";
    case TY_THREAT_TAMPERED:   return "TAMPERED";
    case TY_THREAT_UNKNOWN:    return "UNKNOWN";
    case TY_THREAT_MALWARE:    return "MALWARE";
    default:                   return "INVALID";
    }
}

usz tykid_dump_threats(const tykid_gate_ctx_t *ctx, char *buf, usz buf_sz) {
    if (!ctx || !buf || buf_sz < 2) return 0;

    usz pos = 0;
#define EMIT(c) do { if (pos + 1 < buf_sz) buf[pos++] = (c); } while(0)
#define EMITS(s) do { \
    const char *_p = (s); \
    while (*_p && pos + 1 < buf_sz) buf[pos++] = *_p++; \
} while(0)

    EMITS("TYKID Threat Report\n");
    EMITS("===================\n");

    static const char hex[] = "0123456789abcdef";

    for (u32 i = 0; i < ctx->reg.count; i++) {
        const tykid_driver_desc_t *drv = &ctx->reg.entries[i];
        if (drv->threat_class == TY_THREAT_NONE &&
            drv->state != TYKID_DRV_STATE_BLOCKED) continue;

        if (drv->state == TYKID_DRV_STATE_BLOCKED)       EMITS("[BLOCKED   ] ");
        else if (drv->state == TYKID_DRV_STATE_SKIPPED)  EMITS("[SKIPPED   ] ");
        else if (drv->threat_class == TY_THREAT_SUSPICIOUS) EMITS("[SUSPICIOUS] ");
        else if (drv->threat_class == TY_THREAT_UNSIGNED)   EMITS("[UNSIGNED  ] ");
        else                                              EMITS("[OTHER     ] ");

        EMITS(drv->name);
        EMIT(' ');
        EMITS(tykid_threat_name(drv->threat_class));

        EMITS(" sha256=");
        for (u32 j = 0; j < 8; j++) {
            EMIT(hex[(drv->threat_report.sha256_digest[j] >> 4) & 0xF]);
            EMIT(hex[ drv->threat_report.sha256_digest[j]       & 0xF]);
        }
        EMITS("...");

        if (drv->threat_report.detail_msg[0]) {
            EMIT(' ');
            EMITS(drv->threat_report.detail_msg);
        }
        EMIT('\n');
    }

    buf[pos] = '\0';
#undef EMIT
#undef EMITS
    return pos;
}
