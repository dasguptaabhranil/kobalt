/*
 * diskio.c — FatFS Disk I/O Bridge for the Kobalt Kernel
 *
 * Place this file at:  src/fs/fatfs/diskio.c
 *
 * Maps FatFS physical drive numbers (pdrv 0–3) to Kobalt blkdev entries
 * using the actual blkdev_t* pointer API:
 *
 *   blkdev_t *bd = blkdev_get(pdrv);
 *   blkdev_read (bd, lba, count, buf);
 *   blkdev_write(bd, lba, count, buf);
 *
 * ── NOTE ON SECTOR COUNT FIELD ───────────────────────────────────────────
 * This file accesses bd->num_sectors for GET_SECTOR_COUNT.
 * If your blkdev_t uses a different field name (e.g. bd->sector_count,
 * bd->nsectors, bd->total_lba), adjust the two references below.
 * Grep your blkdev.h for the uint64_t member that holds the total LBA count.
 * ─────────────────────────────────────────────────────────────────────────
 *
 * Copyright (C) 2026 Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ff.h"        /* FatFS types (BYTE, DWORD, LBA_t, DRESULT …) */
#include "diskio.h"    /* Disk-I/O function prototypes                 */
#include <blkdev.h>    /* Kobalt block-device abstraction layer        */

/* -------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

/*
 * get_bd() — resolve a FatFS pdrv to a Kobalt blkdev_t pointer.
 * Returns NULL if pdrv is out of range or device not present.
 */
static inline blkdev_t *get_bd(BYTE pdrv)
{
    if ((uint32_t)pdrv >= blkdev_count())
        return NULL;
    return blkdev_get((int)pdrv);
}

/* -------------------------------------------------------------------------
 * disk_status — Get drive status
 *
 * FatFS calls this before every I/O operation.
 * Return STA_NOINIT if the drive is absent; 0 if it is ready.
 * ------------------------------------------------------------------------- */
DSTATUS disk_status(BYTE pdrv)
{
    if (!get_bd(pdrv))
        return STA_NOINIT;
    return 0;
}

/* -------------------------------------------------------------------------
 * disk_initialize — Initialize drive
 *
 * All block devices are already initialised by the time FatFS is brought
 * up (Stage 6c in kmain).  Nothing extra to do here.
 * ------------------------------------------------------------------------- */
DSTATUS disk_initialize(BYTE pdrv)
{
    if (!get_bd(pdrv))
        return STA_NOINIT;
    return 0;
}

/* -------------------------------------------------------------------------
 * disk_read — Read sectors
 *
 *   pdrv   — physical drive number (0-based)
 *   buff   — data buffer (4-byte aligned; FatFS guarantees this)
 *   sector — starting LBA
 *   count  — number of sectors to read
 *
 * Returns RES_OK on success, RES_ERROR on I/O failure, RES_PARERR on bad args.
 * ------------------------------------------------------------------------- */
DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    if (!buff || count == 0)
        return RES_PARERR;

    blkdev_t *bd = get_bd(pdrv);
    if (!bd)
        return RES_PARERR;

    int rc = blkdev_read(bd, (uint64_t)sector, (uint32_t)count, buff);
    return (rc == 0) ? RES_OK : RES_ERROR;
}

/* -------------------------------------------------------------------------
 * disk_write — Write sectors
 * ------------------------------------------------------------------------- */
#if FF_FS_READONLY == 0
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    if (!buff || count == 0)
        return RES_PARERR;

    blkdev_t *bd = get_bd(pdrv);
    if (!bd)
        return RES_PARERR;

    /*
     * blkdev_write takes a non-const void* in some implementations.
     * The cast is safe: we never actually write through buff here,
     * the kernel's DMA layer does — and FatFS owns the buffer until
     * the function returns.
     */
    int rc = blkdev_write(bd, (uint64_t)sector, (uint32_t)count,
                          (void *)(uintptr_t)buff);
    return (rc == 0) ? RES_OK : RES_ERROR;
}
#endif /* FF_FS_READONLY */

/* -------------------------------------------------------------------------
 * disk_ioctl — Device control / query
 * ------------------------------------------------------------------------- */
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    blkdev_t *bd = get_bd(pdrv);
    if (!bd)
        return RES_PARERR;

    switch (cmd) {

    case CTRL_SYNC:
        /*
         * All blkdev_write calls complete synchronously (DMA with polling
         * wait).  Nothing to flush.
         */
        return RES_OK;

    case GET_SECTOR_COUNT:
        /*
         * Total sectors on the device.
         *
         * ADJUST bd->num_sectors to match your blkdev_t field name.
         * Common alternatives: bd->sector_count  bd->nsectors  bd->total_lba
         */
        if (!buff) return RES_PARERR;
        *(LBA_t *)buff = (LBA_t)bd->num_sectors;
        return RES_OK;

    case GET_SECTOR_SIZE:
        /*
         * ffconf.h fixes FF_MIN_SS == FF_MAX_SS == 512, so FatFS does not
         * call this at run-time, but we handle it for correctness.
         */
        if (!buff) return RES_PARERR;
        *(WORD *)buff = (WORD)bd->sector_size;
        return RES_OK;

    case GET_BLOCK_SIZE:
        /*
         * Erase-block granularity in sectors.  1 = unknown / fine-grained.
         * Safe for HDD, VirtIO-Blk, and most NVMe devices.
         */
        if (!buff) return RES_PARERR;
        *(DWORD *)buff = 1;
        return RES_OK;

#if FF_USE_TRIM
    case CTRL_TRIM:
        /* Forward to blkdev_trim() if your blkdev layer supports it. */
        (void)buff;
        return RES_OK;
#endif

    default:
        return RES_PARERR;
    }
}

/* -------------------------------------------------------------------------
 * get_fattime — Timestamp source
 *
 * Only called when FF_FS_NORTC = 0.  With FF_FS_NORTC = 1 (our default)
 * FatFS uses the compile-time constants in ffconf.h and never calls this.
 *
 * Implement once you add RTC support and set FF_FS_NORTC = 0.
 *
 * Return format (DWORD, packed):
 *   bits 31–25 : year offset from 1980   (0–127)
 *   bits 24–21 : month                   (1–12)
 *   bits 20–16 : day of month            (1–31)
 *   bits 15–11 : hour                    (0–23)
 *   bits 10– 5 : minute                  (0–59)
 *   bits  4– 0 : second / 2             (0–29)
 * ------------------------------------------------------------------------- */
#if FF_FS_NORTC == 0
DWORD get_fattime(void)
{
    /* TODO: replace with a real RTC read. */
    return ((DWORD)(2026 - 1980) << 25)
         | ((DWORD)1  << 21)   /* January */
         | ((DWORD)1  << 16)   /* 1st     */
         | ((DWORD)0  << 11)   /* 00:00:00 */
         | ((DWORD)0  <<  5)
         | ((DWORD)0);
}
#endif