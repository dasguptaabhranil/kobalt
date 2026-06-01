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
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>

#include "../src/fs/flatfs/flatfs.h"

static int opt_repair  = 0;
static int opt_verbose = 0;
static int opt_noact   = 0;
static uint64_t g_errors    = 0;
static uint64_t g_repaired  = 0;

static uint32_t tool_crc32(const void *data, size_t len)
{
    static uint32_t tab[256];
    static int ready;
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

static void set_crc_tool(void *blk, size_t sz)
{
    uint32_t c = tool_crc32(blk, sz - 4);
    memcpy((uint8_t *)blk + sz - 4, &c, 4);
}

#define ERR(fmt, ...) do { \
    fprintf(stderr, "fsck.flatfs: ERROR: " fmt "\n", ##__VA_ARGS__); \
    g_errors++; \
} while(0)

#define WARN(fmt, ...) do { \
    if (opt_verbose) printf("fsck.flatfs: WARN:  " fmt "\n", ##__VA_ARGS__); \
} while(0)

#define INFO(fmt, ...) do { \
    if (opt_verbose) printf("fsck.flatfs: " fmt "\n", ##__VA_ARGS__); \
} while(0)

static void check_superblock(uint8_t *buf, uint64_t cap)
{
    flatfs_super_t *sb  = (flatfs_super_t *)buf;
    flatfs_super_t *sb2 = (flatfs_super_t *)(buf + FLATFS_BLOCK_SIZE);

    if (sb->magic != FLATFS_MAGIC) {
        ERR("superblock: bad magic 0x%08x (expected 0x%08x)",
            sb->magic, FLATFS_MAGIC);
        if (opt_repair && !opt_noact && sb2->magic == FLATFS_MAGIC) {
            memcpy(sb, sb2, sizeof(*sb));
            printf("  repaired: restored primary superblock from backup\n");
            g_repaired++;
        }
        return;
    }
    if (sb->version != FLATFS_VERSION)
        ERR("superblock: version %u (expected %u)", sb->version, FLATFS_VERSION);
    if (sb->block_size != FLATFS_BLOCK_SIZE)
        ERR("superblock: block_size %u (expected %u)", sb->block_size, FLATFS_BLOCK_SIZE);
    if (sb->capacity_bytes != (uint64_t)cap)
        WARN("superblock: capacity_bytes %llu vs device size %llu",
             (unsigned long long)sb->capacity_bytes, (unsigned long long)cap);

    uint32_t sv = sb->crc32;
    sb->crc32 = 0;
    uint32_t got = tool_crc32(sb, sizeof(*sb));
    sb->crc32 = sv;
    if (got != sv) {
        ERR("superblock: CRC32 mismatch (stored=0x%08x computed=0x%08x)",
            sv, got);
        if (opt_repair && !opt_noact) {
            set_crc_tool(sb, sizeof(*sb));
            g_repaired++;
        }
    }

    if (memcmp(sb, sb2, sizeof(*sb)) != 0) {
        WARN("superblock backup differs from primary");
        if (opt_repair && !opt_noact) {
            memcpy(sb2, sb, sizeof(*sb));
            g_repaired++;
        }
    }

    if (sb->state & FLATFS_STATE_DIRTY)
        WARN("filesystem was not cleanly unmounted (journal replay may be needed)");
    if (sb->state & FLATFS_STATE_ERROR)
        ERR("superblock reports prior error condition");

    INFO("superblock OK: %llu blocks, %llu inodes, label='%s'",
         (unsigned long long)sb->total_blocks,
         (unsigned long long)sb->total_inodes,
         (char *)sb->label);
}

static void check_inodes(uint8_t *buf, flatfs_super_t *sb)
{
    uint64_t free_count = 0;
    uint64_t live_count = 0;
    uint64_t errs       = 0;

    for (uint64_t ino = 1; ino < sb->total_inodes; ino++) {
        uint64_t blk = sb->inode_table_start + ino / FLATFS_INODES_PER_BLK;
        uint32_t off = (uint32_t)(ino % FLATFS_INODES_PER_BLK) * FLATFS_INODE_SIZE;
        flatfs_inode_t *in = (flatfs_inode_t *)(buf + blk * FLATFS_BLOCK_SIZE + off);

        if (in->inode_magic == 0) { free_count++; continue; }

        if (in->inode_magic != FLATFS_INODE_MAGIC) {
            ERR("inode %llu: bad magic 0x%08x", (unsigned long long)ino,
                in->inode_magic);
            errs++;
            continue;
        }
        if (in->ino != ino) {
            ERR("inode %llu: self-ino mismatch (stored %llu)",
                (unsigned long long)ino, (unsigned long long)in->ino);
            if (opt_repair && !opt_noact) { in->ino = ino; g_repaired++; }
            errs++;
        }

        uint32_t sv = in->crc32;
        in->crc32 = 0;
        uint32_t got = tool_crc32(in, sizeof(*in));
        in->crc32 = sv;
        if (got != sv) {
            ERR("inode %llu: CRC32 mismatch", (unsigned long long)ino);
            errs++;
        }

        if (in->nlink == 0 && (in->mode & FLATFS_S_IFMT)) {
            WARN("inode %llu: nlink=0 but not free", (unsigned long long)ino);
        }
        live_count++;
    }

    if (sb->free_inodes != free_count) {
        ERR("inode accounting: super says %llu free, counted %llu",
            (unsigned long long)sb->free_inodes,
            (unsigned long long)free_count);
        if (opt_repair && !opt_noact) {
            sb->free_inodes = free_count;
            g_repaired++;
        }
    }

    INFO("inodes: %llu live, %llu free, %llu errors",
         (unsigned long long)live_count,
         (unsigned long long)free_count,
         (unsigned long long)errs);
}

static void check_freelist(uint8_t *buf, flatfs_super_t *sb)
{
    uint64_t counted_free = 0;

    for (uint64_t gi = 0; gi < sb->freelist_blocks; gi++) {
        uint8_t *gblk = buf + (sb->freelist_start + gi) * FLATFS_BLOCK_SIZE;
        uint32_t magic = *(uint32_t *)gblk;

        if (magic != FLATFS_FREELIST_MAGIC) {
            ERR("freelist group %llu: bad magic 0x%08x",
                (unsigned long long)gi, magic);
            continue;
        }

        uint32_t sv = *(uint32_t *)(gblk + FLATFS_BLOCK_SIZE - 4);
        *(uint32_t *)(gblk + FLATFS_BLOCK_SIZE - 4) = 0;
        uint32_t got = tool_crc32(gblk, FLATFS_BLOCK_SIZE - 4);
        *(uint32_t *)(gblk + FLATFS_BLOCK_SIZE - 4) = sv;
        if (got != sv) {
            ERR("freelist group %llu: CRC32 mismatch", (unsigned long long)gi);
            if (opt_repair && !opt_noact) {
                set_crc_tool(gblk, FLATFS_BLOCK_SIZE);
                g_repaired++;
            }
        }

        uint8_t *bm = gblk + 20 + (FLATFS_ALLOC_MAXORDER + 1) * 4;
        for (uint32_t bit = 0; bit < FLATFS_GROUP_BLOCKS; bit++)
            if (!((bm[bit >> 3] >> (bit & 7)) & 1)) counted_free++;
    }

    if (sb->free_blocks != counted_free) {
        ERR("freelist accounting: super says %llu free, bitmap has %llu",
            (unsigned long long)sb->free_blocks,
            (unsigned long long)counted_free);
        if (opt_repair && !opt_noact) {
            sb->free_blocks = counted_free;
            g_repaired++;
        }
    }
    INFO("freelist: %llu free blocks", (unsigned long long)counted_free);
}

static void check_btree_root(uint8_t *buf, flatfs_super_t *sb)
{
    uint64_t rblk = sb->btree_root_blk;
    if (!rblk || rblk >= sb->total_blocks) {
        ERR("btree root block %llu out of range", (unsigned long long)rblk);
        return;
    }
    uint8_t *nb = buf + rblk * FLATFS_BLOCK_SIZE;
    uint32_t magic = *(uint32_t *)nb;
    if (magic != FLATFS_BTREE_MAGIC) {
        ERR("btree root: bad magic 0x%08x", magic);
        return;
    }
    INFO("btree root at block %llu: OK", (unsigned long long)rblk);
}

static void check_journal(uint8_t *buf, flatfs_super_t *sb)
{
    uint64_t jbase = sb->journal_start - 2;
    uint8_t *jblk  = buf + jbase * FLATFS_BLOCK_SIZE;
    uint32_t magic = *(uint32_t *)jblk;
    if (magic != FLATFS_JOURNAL_MAGIC) {
        ERR("journal: bad magic 0x%08x", magic);
        return;
    }
    uint32_t sv  = *(uint32_t *)(jblk + FLATFS_BLOCK_SIZE - 4);
    *(uint32_t *)(jblk + FLATFS_BLOCK_SIZE - 4) = 0;
    uint32_t got = tool_crc32(jblk, FLATFS_BLOCK_SIZE - 4);
    *(uint32_t *)(jblk + FLATFS_BLOCK_SIZE - 4) = sv;
    if (got != sv) {
        ERR("journal superblock: CRC32 mismatch");
        return;
    }
    if (sb->state & FLATFS_STATE_DIRTY)
        printf("  NOTE: dirty flag set — journal replay required at next mount\n");
    INFO("journal: OK (%llu blocks)", (unsigned long long)sb->journal_blocks);
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage: %s [-r] [-v] [-n] <device>\n"
            "  -r  repair errors\n"
            "  -v  verbose output\n"
            "  -n  no-act: report only, do not write\n",
            prog);
    exit(4);
}

int main(int argc, char **argv)
{
    char *dev = NULL;

    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "-r")) opt_repair  = 1;
        else if (!strcmp(argv[i], "-v")) opt_verbose = 1;
        else if (!strcmp(argv[i], "-n")) opt_noact   = 1;
        else if (argv[i][0] != '-')      dev = argv[i];
        else usage(argv[0]);
    }
    if (!dev) usage(argv[0]);

    int flags = (opt_repair && !opt_noact) ? O_RDWR : O_RDONLY;
    int fd = open(dev, flags);
    if (fd < 0) { perror("open"); return 4; }

    struct stat st;
    if (fstat(fd, &st) < 0) { perror("fstat"); close(fd); return 4; }
    uint64_t cap = S_ISREG(st.st_mode) ? (uint64_t)st.st_size
                                        : (uint64_t)lseek(fd, 0, SEEK_END);
    if (cap < FLATFS_BLOCK_SIZE * 8) {
        fprintf(stderr, "fsck.flatfs: device too small\n");
        close(fd);
        return 4;
    }

    int mprot = PROT_READ | ((opt_repair && !opt_noact) ? PROT_WRITE : 0);
    int mflags = (opt_repair && !opt_noact) ? MAP_SHARED : MAP_PRIVATE;
    uint8_t *buf = mmap(NULL, (size_t)cap, mprot, mflags, fd, 0);
    if (buf == MAP_FAILED) { perror("mmap"); close(fd); return 4; }

    printf("fsck.flatfs: checking %s\n", dev);

    check_superblock(buf, cap);

    flatfs_super_t *sb = (flatfs_super_t *)buf;
    if (sb->magic == FLATFS_MAGIC) {
        check_journal(buf, sb);
        check_inodes(buf, sb);
        check_freelist(buf, sb);
        check_btree_root(buf, sb);

        if (opt_repair && !opt_noact && g_repaired > 0) {
            sb->error_count += (uint32_t)g_errors;
            if (g_errors == g_repaired)
                sb->state = FLATFS_STATE_CLEAN;

            sb->crc32 = 0;
            sb->crc32 = tool_crc32(sb, sizeof(*sb));
            memcpy(buf + FLATFS_BLOCK_SIZE, sb, sizeof(*sb));
            msync(buf, (size_t)cap, MS_SYNC);
        }
    }

    munmap(buf, (size_t)cap);
    close(fd);

    printf("fsck.flatfs: %llu error(s), %llu repaired\n",
           (unsigned long long)g_errors,
           (unsigned long long)g_repaired);

    if (g_errors == 0)              return 0;
    if (g_repaired == g_errors)     return 2;
    return 1;
}
