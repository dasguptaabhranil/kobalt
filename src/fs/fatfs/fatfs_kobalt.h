/*
 * fatfs_kobalt.h — Kobalt-Kernel FatFS Mount / Unmount API
 *
 * Place this file at:  src/fs/fatfs/fatfs_kobalt.h
 *
 * This is the thin glue layer between kmain() and FatFS.
 * It mirrors the pattern used by flatfs_kobalt.h in this kernel.
 *
 * Typical usage in kmain() after Stage 6c (block devices up):
 *
 *   #include "../fs/fatfs/fatfs_kobalt.h"
 *
 *   fatfs_kobalt_init();        // try to mount all blkdevs
 *
 *   if (fatfs_kobalt_is_mounted(0))
 *       klog_ok("fatfs", "FAT volume 0 mounted");
 *
 * Copyright (C) 2026 Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef FATFS_KOBALT_H
#define FATFS_KOBALT_H

#include <stdint.h>

/*
 * Maximum logical volumes we will try to mount automatically.
 * Must be ≤ FF_VOLUMES (defined in ffconf.h).
 */
#define FATFS_KOBALT_MAX_VOLS   4

/* -------------------------------------------------------------------------
 * Initialisation
 * ------------------------------------------------------------------------- */

/*
 * fatfs_kobalt_init()
 *
 * Iterates over all registered blkdevs (up to FATFS_KOBALT_MAX_VOLS) and
 * calls f_mount() on each one.  Volumes whose blkdev does not contain a
 * recognisable FAT/exFAT filesystem are silently skipped.
 *
 * Call this once, after all block-device drivers have been initialised
 * (i.e. after Stage 6c in kmain).
 */
void fatfs_kobalt_init(void);

/*
 * fatfs_kobalt_mount(vol, pdrv)
 *
 * Mount a single logical volume.
 *
 *   vol  — FatFS volume number  (0 = "0:", 1 = "1:", …)
 *   pdrv — blkdev index to use for this volume (usually same as vol)
 *
 * Returns 0 on success, -1 on failure.
 */
int fatfs_kobalt_mount(int vol, int pdrv);

/*
 * fatfs_kobalt_unmount(vol)
 *
 * Flush pending writes and release the FATFS object for volume vol.
 */
void fatfs_kobalt_unmount(int vol);

/* -------------------------------------------------------------------------
 * Status Queries
 * ------------------------------------------------------------------------- */

/*
 * fatfs_kobalt_is_mounted(vol) — returns non-zero if vol is mounted.
 */
int fatfs_kobalt_is_mounted(int vol);

/*
 * fatfs_kobalt_mounted_count() — returns the number of successfully
 * mounted FAT volumes.
 */
int fatfs_kobalt_mounted_count(void);

/*
 * fatfs_kobalt_strerror(fresult) — human-readable string for a FRESULT
 * error code returned by any f_xxx() FatFS API call.
 */
const char *fatfs_kobalt_strerror(int fresult);

#endif /* FATFS_KOBALT_H */
