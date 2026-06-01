/*
 * fatfs_kobalt.c — Kobalt-Kernel FatFS Mount / Unmount Helpers
 *
 * Place this file at:  src/fs/fatfs/fatfs_kobalt.c
 *
 * Copyright (C) 2026 Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ff.h"               /* FatFS public API  */
#include "fatfs_kobalt.h"     /* our own header    */
#include <blkdev.h>           /* blkdev_count()    */
#include <string.h>           /* memset            */

/* -------------------------------------------------------------------------
 * Internal state
 * ------------------------------------------------------------------------- */

/*
 * One FATFS work structure per logical volume.
 * These must remain valid for the entire lifetime of the mount.
 * We use a static array — no heap required.
 */
static FATFS s_fs[FATFS_KOBALT_MAX_VOLS];

/* Which volumes are currently mounted (1 = yes, 0 = no). */
static int   s_mounted[FATFS_KOBALT_MAX_VOLS];

/* -------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

/*
 * vol_path_for() — fill buf with the FatFS volume path string, e.g. "0:".
 * buf must be at least 3 bytes.
 */
static void vol_path_for(int vol, char *buf)
{
    buf[0] = (char)('0' + vol);
    buf[1] = ':';
    buf[2] = '\0';
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

int fatfs_kobalt_mount(int vol, int pdrv)
{
    if (vol < 0 || vol >= FATFS_KOBALT_MAX_VOLS)
        return -1;

    /*
     * FatFS does not expose a pdrv↔volume mapping function in the public API.
     * The default mapping is:  volume N  →  pdrv N.
     * If you need a custom mapping (e.g. volume 0 → pdrv 2), define
     * ff_get_ldnumber() or use the VolToPart[] table in ff.c (FF_MULTI_PARTITION).
     * For now, we require vol == pdrv, which matches the 1-to-1 blkdev layout.
     */
    if (vol != pdrv)
        return -1;   /* unsupported mapping — see comment above */

    char path[4];
    vol_path_for(vol, path);

    memset(&s_fs[vol], 0, sizeof(FATFS));

    /*
     * f_mount() with opt=1 performs an immediate mount attempt.
     * With opt=0 the mount is deferred until the first file operation —
     * we use 1 so we know right away whether a FAT filesystem is present.
     */
    FRESULT fr = f_mount(&s_fs[vol], path, /*opt=*/1);
    if (fr == FR_OK) {
        s_mounted[vol] = 1;
        return 0;
    }

    /* Mount failed — clear the work structure so it's not left half-initialised. */
    memset(&s_fs[vol], 0, sizeof(FATFS));
    s_mounted[vol] = 0;
    return -1;
}

void fatfs_kobalt_unmount(int vol)
{
    if (vol < 0 || vol >= FATFS_KOBALT_MAX_VOLS)
        return;
    if (!s_mounted[vol])
        return;

    char path[4];
    vol_path_for(vol, path);

    /* Passing NULL de-registers the FATFS object and flushes the cache. */
    f_mount(NULL, path, 0);
    memset(&s_fs[vol], 0, sizeof(FATFS));
    s_mounted[vol] = 0;
}

void fatfs_kobalt_init(void)
{
    uint32_t ndev = blkdev_count();
    if (ndev > (uint32_t)FATFS_KOBALT_MAX_VOLS)
        ndev = (uint32_t)FATFS_KOBALT_MAX_VOLS;

    for (uint32_t i = 0; i < ndev; i++)
        fatfs_kobalt_mount((int)i, (int)i);
        /* Return value intentionally ignored here; callers use
         * fatfs_kobalt_is_mounted() to check individual volumes. */
}

int fatfs_kobalt_is_mounted(int vol)
{
    if (vol < 0 || vol >= FATFS_KOBALT_MAX_VOLS)
        return 0;
    return s_mounted[vol];
}

int fatfs_kobalt_mounted_count(void)
{
    int n = 0;
    for (int i = 0; i < FATFS_KOBALT_MAX_VOLS; i++)
        if (s_mounted[i]) n++;
    return n;
}

/* -------------------------------------------------------------------------
 * fatfs_kobalt_strerror — human-readable FRESULT → string
 *
 * Use this anywhere you call an f_xxx() function and want to print the
 * error, e.g.:
 *
 *   FRESULT fr = f_open(&fil, "0:/boot.cfg", FA_READ);
 *   if (fr != FR_OK) klog_fail("fatfs", fatfs_kobalt_strerror(fr));
 * ------------------------------------------------------------------------- */
const char *fatfs_kobalt_strerror(int fresult)
{
    switch ((FRESULT)fresult) {
    case FR_OK:                  return "OK";
    case FR_DISK_ERR:            return "low-level I/O error";
    case FR_INT_ERR:             return "internal FatFS assertion failed";
    case FR_NOT_READY:           return "drive not ready";
    case FR_NO_FILE:             return "file not found";
    case FR_NO_PATH:             return "path not found";
    case FR_INVALID_NAME:        return "invalid path name";
    case FR_DENIED:              return "access denied / directory full";
    case FR_EXIST:               return "object already exists";
    case FR_INVALID_OBJECT:      return "invalid file or directory object";
    case FR_WRITE_PROTECTED:     return "drive is write-protected";
    case FR_INVALID_DRIVE:       return "invalid drive number";
    case FR_NOT_ENABLED:         return "volume not mounted";
    case FR_NO_FILESYSTEM:       return "no valid FAT/exFAT filesystem found";
    case FR_MKFS_ABORTED:        return "f_mkfs() aborted";
    case FR_TIMEOUT:             return "lock timeout";
    case FR_LOCKED:              return "file locked by another task";
    case FR_NOT_ENOUGH_CORE:     return "not enough memory for LFN buffer";
    case FR_TOO_MANY_OPEN_FILES: return "too many open files";
    case FR_INVALID_PARAMETER:   return "invalid parameter";
    default:                     return "unknown error";
    }
}
