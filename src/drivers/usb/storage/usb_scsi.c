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

struct msc_shadow {
    void    *dev;
    uint8_t  bulk_in;
    uint8_t  bulk_out;
    uint8_t  lun;
    uint32_t tag;
    uint64_t num_sectors;
    uint32_t sector_size;
};

extern int msc_bot_command(void *msc, const uint8_t *cb, uint8_t cblen,
                             void *data, uint32_t dlen, int dir_in);

#define BOTI(m,cb,cl,buf,bl)  msc_bot_command(m, cb, cl, buf, bl, 1)
#define BOTO(m,cb,cl,buf,bl)  msc_bot_command(m, cb, cl, (void*)(buf), bl, 0)

#define OP_TUR    0x00
#define OP_SENSE  0x03
#define OP_INQ    0x12
#define OP_CAP10  0x25
#define OP_READ10 0x28
#define OP_WR10   0x2A
#define OP_SVC_IN 0x9E
#define SA_CAP16  0x10
#define OP_SYNC   0x35

typedef struct __attribute__((packed)) {
    uint8_t  peripheral, rmb, version, response_fmt, addl_len;
    uint8_t  flags[3];
    char     vendor[8], product[16], rev[4];
} inq_t;

typedef struct __attribute__((packed)) {
    uint32_t last_lba;
    uint32_t block_size;
} cap10_t;

typedef struct __attribute__((packed)) {
    uint64_t last_lba;
    uint32_t block_size;
    uint8_t  _r[20];
} cap16_t;

typedef struct __attribute__((packed)) {
    uint8_t resp_code, _r;
    uint8_t sense_key;
    uint8_t info[4];
    uint8_t addl_len;
    uint8_t cmd_info[4];
    uint8_t asc, ascq;
    uint8_t _r2[4];
} sense_t;

static int do_sense(void *m, sense_t *s)
{
    uint8_t cb[6] = { OP_SENSE, 0, 0, 0, sizeof(*s), 0 };
    return BOTI(m, cb, 6, s, sizeof(*s));
}

int usb_scsi_inquiry(void *m)
{
    inq_t inq;
    memset(&inq, 0, sizeof(inq));
    uint8_t cb[6] = { OP_INQ, 0, 0, 0, sizeof(inq), 0 };
    if (BOTI(m, cb, 6, &inq, sizeof(inq)) < 0) return -1;

    char v[9], p[17];
    memcpy(v, inq.vendor,  8); v[8]  = '\0';
    memcpy(p, inq.product, 16); p[16] = '\0';
    for (int i = 7;  i >= 0  && v[i] == ' '; i--) v[i] = '\0';
    for (int i = 15; i >= 0  && p[i] == ' '; i--) p[i] = '\0';

    char msg[64];
    ksnprintf(msg, sizeof(msg), "SCSI: '%s' '%s' type=%02x", v, p, inq.peripheral & 0x1F);
    klog_info("usb_scsi", msg);
    return 0;
}

int usb_scsi_read_capacity(void *m)
{
    struct msc_shadow *ms = m;

    for (int i = 0; i < 3; i++) {
        uint8_t cb[6] = { OP_TUR, 0, 0, 0, 0, 0 };
        if (BOTO(m, cb, 6, NULL, 0) == 0) break;
        sense_t s; do_sense(m, &s);
        for (volatile int d = 0; d < 1000000; d++);
    }

    cap10_t c10;
    memset(&c10, 0, sizeof(c10));
    uint8_t cb10[10] = { OP_CAP10, 0,0,0,0,0,0,0,0,0 };
    if (BOTI(m, cb10, 10, &c10, sizeof(c10)) < 0) return -1;

    uint32_t llba = __builtin_bswap32(c10.last_lba);
    uint32_t bsz  = __builtin_bswap32(c10.block_size);

    if (llba == 0xFFFFFFFFu) {
        cap16_t c16;
        memset(&c16, 0, sizeof(c16));
        uint8_t cb16[16] = {
            OP_SVC_IN, SA_CAP16, 0,0,0,0,0,0,0,0,0,0,
            (uint8_t)(sizeof(c16)>>8), (uint8_t)sizeof(c16), 0,0
        };
        if (BOTI(m, cb16, 16, &c16, sizeof(c16)) < 0) return -1;
        ms->num_sectors = __builtin_bswap64(c16.last_lba) + 1ULL;
        bsz = __builtin_bswap32(c16.block_size);
    } else {
        ms->num_sectors = (uint64_t)llba + 1ULL;
    }
    ms->sector_size = bsz ? bsz : 512u;
    return 0;
}

int usb_scsi_read10(void *m, uint64_t lba, uint16_t blks, void *buf)
{
    struct msc_shadow *ms = m;
    uint32_t l = (uint32_t)lba;
    uint8_t cb[10] = {
        OP_READ10, 0,
        (uint8_t)(l>>24), (uint8_t)(l>>16), (uint8_t)(l>>8), (uint8_t)l,
        0,
        (uint8_t)(blks>>8), (uint8_t)blks,
        0
    };
    return BOTI(m, cb, 10, buf, (uint32_t)blks * ms->sector_size);
}

int usb_scsi_write10(void *m, uint64_t lba, uint16_t blks, const void *buf)
{
    struct msc_shadow *ms = m;
    uint32_t l = (uint32_t)lba;
    uint8_t cb[10] = {
        OP_WR10, 0,
        (uint8_t)(l>>24), (uint8_t)(l>>16), (uint8_t)(l>>8), (uint8_t)l,
        0,
        (uint8_t)(blks>>8), (uint8_t)blks,
        0
    };
    return BOTO(m, cb, 10, buf, (uint32_t)blks * ms->sector_size);
}

int usb_scsi_sync_cache(void *m)
{
    uint8_t cb[10] = { OP_SYNC, 0,0,0,0,0,0,0,0,0 };
    return BOTO(m, cb, 10, NULL, 0);
}
