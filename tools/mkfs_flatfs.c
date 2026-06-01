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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <time.h>

#define FLATFS_TOOL_BUILD 1
#include "../src/fs/flatfs/flatfs.h"
#include "../src/fs/flatfs/flatfs_super.h"

#define DEFAULT_INODES        65536u
#define DEFAULT_JOURNAL_BLKS  FLATFS_JOURNAL_BLOCKS
#define MIN_SIZE_BYTES        (4u * 1024u * 1024u)

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage: %s [-L label] [-i inodes] [-j journal_blocks] <device>\n"
            "  -L  volume label (max 63 chars)\n"
            "  -i  number of inodes (default %u)\n"
            "  -j  journal ring size in blocks (default %u)\n",
            prog, DEFAULT_INODES, DEFAULT_JOURNAL_BLKS);
    exit(1);
}

static uint64_t get_device_size(int fd)
{
    struct stat st;
    if (fstat(fd, &st) < 0) return 0;
    if (S_ISREG(st.st_mode)) return (uint64_t)st.st_size;

    off_t sz = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    return (sz > 0) ? (uint64_t)sz : 0;
}

static uint32_t tool_crc32(const void *data, size_t len)
{
    static uint32_t tab[256];
    static int      ready;
    if (!ready) {
        for (int i = 0; i < 256; i++) {
            uint32_t c = (uint32_t)i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            tab[i] = c;
        }
        ready = 1;
    }
    const uint8_t *p = data;
    uint32_t crc = 0xFFFFFFFFu;
    while (len--) crc = tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

static void set_crc(void *blk, size_t sz)
{
    uint32_t c = tool_crc32(blk, sz - 4);
    memcpy((uint8_t *)blk + sz - 4, &c, 4);
}

int main(int argc, char **argv)
{
    char     *label  = "flatfs";
    uint64_t  ninodes= DEFAULT_INODES;
    uint64_t  jblks  = DEFAULT_JOURNAL_BLKS;
    char     *dev    = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-L") && i+1 < argc) { label   = argv[++i]; }
        else if (!strcmp(argv[i], "-i") && i+1 < argc) { ninodes = (uint64_t)atoll(argv[++i]); }
        else if (!strcmp(argv[i], "-j") && i+1 < argc) { jblks   = (uint64_t)atoll(argv[++i]); }
        else if (argv[i][0] != '-') { dev = argv[i]; }
        else { usage(argv[0]); }
    }
    if (!dev) usage(argv[0]);
    if (ninodes < 16)   { fprintf(stderr, "mkfs: need at least 16 inodes\n"); return 1; }
    if (jblks  < 16)    { fprintf(stderr, "mkfs: journal too small\n"); return 1; }

    int fd = open(dev, O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    uint64_t cap = get_device_size(fd);
    if (cap < MIN_SIZE_BYTES) {
        fprintf(stderr, "mkfs: device too small (%llu bytes, need %u)\n",
                (unsigned long long)cap, MIN_SIZE_BYTES);
        close(fd);
        return 1;
    }

    printf("mkfs.flatfs: formatting %s\n", dev);
    printf("  capacity:  %llu bytes (%.1f MiB)\n",
           (unsigned long long)cap, (double)cap / (1024*1024));
    printf("  inodes:    %llu\n", (unsigned long long)ninodes);
    printf("  journal:   %llu blocks (%llu KiB)\n",
           (unsigned long long)jblks,
           (unsigned long long)(jblks * FLATFS_BLOCK_SIZE / 1024));
    printf("  label:     %s\n", label);

    void *buf = mmap(NULL, (size_t)cap, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (buf == MAP_FAILED) { perror("mmap"); close(fd); return 1; }

    uint64_t zero_bytes = cap < (16 * 1024 * 1024ULL) ? cap : (16 * 1024 * 1024ULL);
    memset(buf, 0, (size_t)zero_bytes);

    flatfs_super_t *sb = (flatfs_super_t *)buf;
    sb->magic              = FLATFS_MAGIC;
    sb->version            = FLATFS_VERSION;
    sb->block_size         = FLATFS_BLOCK_SIZE;
    sb->inode_size         = FLATFS_INODE_SIZE;
    sb->capacity_bytes     = cap;

    uint64_t total_blks    = cap >> FLATFS_BLOCK_SHIFT;
    sb->total_blocks       = total_blks;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    sb->uuid[0] = (uint64_t)ts.tv_sec ^ (uint64_t)ts.tv_nsec;
    sb->uuid[1] = cap ^ (uint64_t)(uintptr_t)buf;
    sb->mkfs_time = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;

    uint64_t off = 2;

    sb->journal_start  = off + 2;
    sb->journal_blocks = jblks;
    off = sb->journal_start + jblks;

    uint64_t iblks = (ninodes + FLATFS_INODES_PER_BLK - 1)
                     / FLATFS_INODES_PER_BLK;
    sb->inode_table_start  = off;
    sb->inode_table_blocks = iblks;
    sb->total_inodes       = ninodes;
    sb->free_inodes        = ninodes - 2;
    off += iblks;

    sb->btree_root_blk = off++;

    uint64_t data_blks = total_blks - off - 1;
    uint64_t ngroups   = (data_blks + FLATFS_GROUP_BLOCKS - 1)
                         / FLATFS_GROUP_BLOCKS;
    sb->freelist_start  = off;
    sb->freelist_blocks = ngroups;
    off += ngroups;

    sb->data_start      = off;
    sb->free_blocks     = total_blks - off;
    sb->root_ino        = 1;
    sb->features        = FLATFS_FEAT_JOURNAL | FLATFS_FEAT_HTREE |
                          FLATFS_FEAT_BTREE   | FLATFS_FEAT_INLINE |
                          FLATFS_FEAT_CRC32   | FLATFS_FEAT_64BIT  |
                          FLATFS_FEAT_MONITOR | FLATFS_FEAT_ARBITER;
    sb->state           = FLATFS_STATE_CLEAN;
    sb->max_mounts      = 30;
    strncpy((char *)sb->label, label, 63);

    sb->crc32 = 0;
    sb->crc32 = tool_crc32(sb, sizeof(*sb));
    memcpy((uint8_t *)buf + FLATFS_BLOCK_SIZE, sb, sizeof(*sb));

    printf("  layout:    super(2) + journal_hdr(2) + journal(%llu) + "
           "inode_table(%llu) + btree(1) + freelist(%llu) + data(%llu)\n",
           (unsigned long long)jblks,
           (unsigned long long)iblks,
           (unsigned long long)ngroups,
           (unsigned long long)(total_blks - sb->data_start));

    {
        uint8_t *jblk = (uint8_t *)buf + (sb->journal_start - 2) *
                         FLATFS_BLOCK_SIZE;

        uint32_t *magic  = (uint32_t *)jblk;
        uint64_t *fields = (uint64_t *)(jblk + 8);
        *magic   = FLATFS_JOURNAL_MAGIC;
        *(uint32_t *)(jblk + 4) = 1;
        fields[0] = 1;
        fields[1] = 0;
        fields[2] = 0;
        fields[3] = sb->journal_start;
        fields[4] = jblks - 2;
        *(uint32_t *)(jblk + FLATFS_BLOCK_SIZE - 4) = 0;
        uint32_t jcrc = tool_crc32(jblk, FLATFS_BLOCK_SIZE - 4);
        memcpy(jblk + FLATFS_BLOCK_SIZE - 4, &jcrc, 4);

        memcpy(jblk + FLATFS_BLOCK_SIZE, jblk, FLATFS_BLOCK_SIZE);
    }

    for (uint64_t gi = 0; gi < ngroups; gi++) {
        uint8_t *gblk = (uint8_t *)buf +
                         (sb->freelist_start + gi) * FLATFS_BLOCK_SIZE;
        memset(gblk, 0, FLATFS_BLOCK_SIZE);
        *(uint32_t *)gblk = FLATFS_FREELIST_MAGIC;
        *(uint32_t *)(gblk + 4)  = (uint32_t)gi;
        uint64_t base = sb->data_start + gi * FLATFS_GROUP_BLOCKS;
        memcpy(gblk + 8, &base, 8);
        uint32_t fc = FLATFS_GROUP_BLOCKS;
        memcpy(gblk + 16, &fc, 4);

        set_crc(gblk, FLATFS_BLOCK_SIZE);
    }

    {
        uint8_t *rblk = (uint8_t *)buf + sb->btree_root_blk * FLATFS_BLOCK_SIZE;
        memset(rblk, 0, FLATFS_BLOCK_SIZE);
        *(uint32_t *)rblk         = FLATFS_BTREE_MAGIC;
        *(uint16_t *)(rblk + 4)   = 1;
        *(uint16_t *)(rblk + 6)   = 0;
        uint64_t self = sb->btree_root_blk;
        memcpy(rblk + 16, &self, 8);
        set_crc(rblk, FLATFS_BLOCK_SIZE);
    }

    {
        uint64_t iblk_addr = sb->inode_table_start
                             + 1 / FLATFS_INODES_PER_BLK;
        uint32_t ioff = (1 % FLATFS_INODES_PER_BLK) * FLATFS_INODE_SIZE;
        uint8_t *iblk = (uint8_t *)buf + iblk_addr * FLATFS_BLOCK_SIZE + ioff;
        memset(iblk, 0, FLATFS_INODE_SIZE);

        flatfs_inode_t *in = (flatfs_inode_t *)iblk;
        in->ino         = 1;
        in->mode        = FLATFS_S_IFDIR | 0755u;
        in->uid         = 0;
        in->gid         = 0;
        in->nlink       = 2;
        in->crtime_ns   = sb->mkfs_time;
        in->atime_ns    = sb->mkfs_time;
        in->mtime_ns    = sb->mkfs_time;
        in->ctime_ns    = sb->mkfs_time;
        in->flags       = FLATFS_FL_HTREE;
        in->inode_magic = FLATFS_INODE_MAGIC;

        uint64_t ht_blk = sb->data_start;
        sb->free_blocks--;
        uint8_t *htblk = (uint8_t *)buf + ht_blk * FLATFS_BLOCK_SIZE;
        memset(htblk, 0, FLATFS_BLOCK_SIZE);
        *(uint32_t *)htblk = FLATFS_HTREE_MAGIC;
        *(uint32_t *)(htblk + 4) = 2;
        *(uint32_t *)(htblk + 8) = 1;
        set_crc(htblk, FLATFS_BLOCK_SIZE);
        in->dir_htree_blk = ht_blk;
        in->htree_depth   = 1;

        uint8_t *g0 = (uint8_t *)buf + sb->freelist_start * FLATFS_BLOCK_SIZE;
        uint32_t bit = (uint32_t)(ht_blk - sb->data_start);
        g0[20 + (bit >> 3)] |= (uint8_t)(1u << (bit & 7));
        uint32_t *fc = (uint32_t *)(g0 + 16);
        (*fc)--;
        set_crc(g0, FLATFS_BLOCK_SIZE);

        in->size = 2;
        in->crc32 = 0;
        in->crc32 = tool_crc32(in, sizeof(*in));
    }

    sb->crc32 = 0;
    sb->crc32 = tool_crc32(sb, sizeof(*sb));
    memcpy((uint8_t *)buf + FLATFS_BLOCK_SIZE, sb, sizeof(*sb));

    msync(buf, (size_t)cap, MS_SYNC);
    munmap(buf, (size_t)cap);
    close(fd);

    printf("mkfs.flatfs: done. %llu blocks, %llu inodes.\n",
           (unsigned long long)total_blks,
           (unsigned long long)ninodes);
    return 0;
}
