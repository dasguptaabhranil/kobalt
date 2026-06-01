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

#ifndef _SYSFS_KOBALT_H
#define _SYSFS_KOBALT_H

#include <sysfs.h>

void sysfs_kobalt_init(void);

sysfs_obj_t *sysfs_blkdev_add(const char *devname, int removable,
                               uint64_t nsectors, const char *driver);
void         sysfs_blkdev_remove(const char *devname);

sysfs_obj_t *sysfs_pci_add_device(uint8_t bus, uint8_t dev, uint8_t fn,
                                   uint16_t vendor, uint16_t device,
                                   uint8_t cls, uint8_t sub, uint8_t irq);

sysfs_obj_t *sysfs_net_add(const char *ifname, const char *mac,
                            const char *driver);
void         sysfs_net_remove(const char *ifname);

#endif
