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

#if 0

#include "../inc/blkdev.h"

#include "../drivers/ahci/ahci.h"
#include "../drivers/nvme/nvme.h"
#include "../drivers/virtio/virtio_blk.h"

#endif

#if 0

    {
        int n_blk = 0;
        int blk_rc;

        blk_rc = ahci_init();
        if (blk_rc > 0) {
            char ahci_msg[48];
            ksnprintf(ahci_msg, sizeof(ahci_msg),
                      "%d SATA drive(s) registered", blk_rc);
            klog_ok("ahci", ahci_msg);
            n_blk += blk_rc;
        } else if (blk_rc == 0) {
            klog_info("ahci", "controller present, no drives attached");
        } else {
            klog_info("ahci", "no AHCI controller");
        }

        blk_rc = nvme_init();
        if (blk_rc > 0) {
            char nvme_msg[48];
            ksnprintf(nvme_msg, sizeof(nvme_msg),
                      "%d NVMe namespace(s) registered", blk_rc);
            klog_ok("nvme", nvme_msg);
            n_blk += blk_rc;
        } else {
            klog_info("nvme", "no NVMe controller");
        }

        blk_rc = virtio_blk_init();
        if (blk_rc == 0) {
            klog_ok("virtio-blk", "block device registered");
            n_blk++;
        } else {
            klog_info("virtio-blk", "no VirtIO block device");
        }

        if (n_blk > 0) {
            char blksum[48];
            ksnprintf(blksum, sizeof(blksum),
                      "%u block device(s) ready", blkdev_count());
            klog_ok("blkdev", blksum);
        } else {
            klog_warn("blkdev", "no block storage available");
        }
    }

#endif

#if 0

    } else if (strcmp(cmd, "blkls") == 0) {
        unsigned int n = blkdev_count();
        if (n == 0) {
            kputs("  No block devices registered.\n");
        } else {
            kputs("  IDX  NAME        SECTORS          SECTSZ  CAPACITY\n");
            kputs("  ─────────────────────────────────────────────────────\n");
            for (unsigned int i = 0; i < n; i++) {
                blkdev_t *d = blkdev_get(i);
                if (!d) continue;

                uint64_t mib = (d->num_sectors / 2048u);
                if (d->sector_size != 512u)
                    mib = (d->num_sectors * d->sector_size) / (1024u * 1024u);

                char row[80];
                ksnprintf(row, sizeof(row),
                          "  [%u]  %-10s  %-16llu  %-6u  %llu MiB\n",
                          i, d->name,
                          (unsigned long long)d->num_sectors,
                          d->sector_size,
                          (unsigned long long)mib);
                kputs(row);
            }
        }

    } else if (strncmp(cmd, "blkread ", 8) == 0) {
        const char *args = cmd + 8;
        unsigned int dev_idx = 0;
        unsigned long long lba = 0;
        unsigned int count = 1;

        int parsed = 0;
        {
            const char *p = args;

            while (*p == ' ') p++;
            while (*p >= '0' && *p <= '9') { dev_idx = dev_idx * 10u + (unsigned)(*p - '0'); p++; parsed++; }

            while (*p == ' ') p++;
            while (*p >= '0' && *p <= '9') { lba = lba * 10ULL + (unsigned long long)(*p - '0'); p++; parsed++; }

            while (*p == ' ') p++;
            if (*p >= '1' && *p <= '9') {
                count = 0;
                while (*p >= '0' && *p <= '9') { count = count * 10u + (unsigned)(*p - '0'); p++; }
            }
        }

        if (parsed < 2) {
            kputs("  usage: blkread <dev_idx> <lba> [count=1]\n");
            goto dispatch_done;
        }
        if (count == 0 || count > 8u) {
            kputs("  count must be 1-8\n");
            goto dispatch_done;
        }

        blkdev_t *bd = blkdev_get(dev_idx);
        if (!bd) {
            kputs("  invalid device index\n");
            goto dispatch_done;
        }

        static uint8_t blkread_buf[8 * 4096]
            __attribute__((aligned(4096)));

        uint32_t byte_count = count * bd->sector_size;
        if (byte_count > sizeof(blkread_buf)) {
            kputs("  request too large for read buffer\n");
            goto dispatch_done;
        }

        int rc = blkdev_read(bd, lba, count, blkread_buf);
        if (rc != 0) {
            kputs("  read failed\n");
            goto dispatch_done;
        }

        static const char hextab[] = "0123456789abcdef";
        char line[80];
        uint32_t offset;
        for (offset = 0; offset < byte_count; offset += 16u) {
            uint32_t pos = 0;

            line[pos++] = hextab[(offset >> 12) & 0xF];
            line[pos++] = hextab[(offset >>  8) & 0xF];
            line[pos++] = hextab[(offset >>  4) & 0xF];
            line[pos++] = hextab[(offset >>  0) & 0xF];
            line[pos++] = ':'; line[pos++] = ' ';

            for (uint32_t k = 0; k < 16u && offset + k < byte_count; k++) {
                uint8_t b = blkread_buf[offset + k];
                line[pos++] = hextab[b >> 4];
                line[pos++] = hextab[b & 0xF];
                line[pos++] = ' ';
            }
            line[pos++] = '\n';
            line[pos]   = '\0';
            kputs(line);
        }

        dispatch_done:;

    } else if (strncmp(cmd, "blkwrite ", 9) == 0) {

        const char *p = cmd + 9;
        unsigned int dev_idx = 0;
        unsigned long long lba = 0;

        while (*p == ' ') p++;
        while (*p >= '0' && *p <= '9') { dev_idx = dev_idx * 10u + (unsigned)(*p - '0'); p++; }
        while (*p == ' ') p++;
        while (*p >= '0' && *p <= '9') { lba = lba * 10ULL + (unsigned long long)(*p - '0'); p++; }

        blkdev_t *bd = blkdev_get(dev_idx);
        if (!bd) {
            kputs("  invalid device index\n");
        } else {
            static uint8_t blkwrite_buf[4096] __attribute__((aligned(4096)));
            uint32_t i;
            for (i = 0; i < bd->sector_size; i++)
                blkwrite_buf[i] = (uint8_t)(i & 0xFFu);

            int wrc = blkdev_write(bd, lba, 1, blkwrite_buf);
            if (wrc == 0)
                klog_ok("blkdev", "sector written");
            else
                klog_fail("blkdev", "write failed");
        }

#endif

#if 0

        kputs("  blkls                    — list registered block devices\n");
        kputs("  blkread <dev> <lba> [n]  — hex dump n sectors from LBA\n");
        kputs("  blkwrite <dev> <lba>     — write test pattern to sector\n");

#endif
