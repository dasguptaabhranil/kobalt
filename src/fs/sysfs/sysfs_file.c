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

#include "sysfs.h"

ssize_t sysfs_attr_read(sysfs_obj_t *obj, sysfs_attr_t *attr,
                        void *buf, size_t off, size_t len)
{
    if (!attr->show) return -1;

    char *page = kmalloc(SYSFS_PAGE_SIZE);
    if (!page) return -1;

    int n = attr->show(obj, page, SYSFS_PAGE_SIZE);
    if (n < 0) { kfree(page); return n; }

    if (off >= (size_t)n) { kfree(page); return 0; }

    size_t avail = (size_t)n - off;
    size_t copy  = len < avail ? len : avail;
    memcpy(buf, page + off, copy);
    kfree(page);
    return (ssize_t)copy;
}

ssize_t sysfs_attr_write(sysfs_obj_t *obj, sysfs_attr_t *attr,
                         const void *buf, size_t len)
{
    if (!attr->store || !(attr->mode & 0200)) return -1;
    int r = attr->store(obj, (const char *)buf, len);
    return r < 0 ? r : (ssize_t)len;
}
