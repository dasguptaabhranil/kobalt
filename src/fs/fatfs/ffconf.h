/* Copyright (C) 2026 Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef FFCONF_H
#define FFCONF_H

#define FFCONF_DEF	80386		/* Must match FF_DEFINED in ff.h (R0.16) */
#define FF_FS_READONLY      0

#define FF_USE_MKFS         1

#define FF_USE_FIND         1

#define FF_USE_FASTSEEK     0

#define FF_USE_EXPAND       0

#define FF_USE_CHMOD        0

#define FF_USE_LABEL        1

#define FF_USE_FORWARD      0

#define FF_USE_STRFUNC      0
#define FF_CODE_PAGE        437
#define FF_USE_LFN          1
#define FF_MAX_LFN          255

#define FF_LFN_UNICODE      0

#define FF_LFN_BUF          255
#define FF_SFN_BUF          12
#define FF_FS_RPATH         0
#define FF_VOLUMES          4
#define FF_STR_VOLUME_ID    0
#define FF_MULTI_PARTITION  0

#define FF_MIN_SS           512
#define FF_MAX_SS           512

#define FF_LBA64            0
#define FF_FS_EXFAT         1

#define FF_USE_TRIM         0

#define FF_FS_NOFSINFO      0

#define FF_FS_TINY          0

#define FF_FS_NORTC         1
#define FF_NORTC_MON        1       /* January */
#define FF_NORTC_MDAY       1       /* 1st      */
#define FF_NORTC_YEAR       2026    /* Year     */

#define FF_FS_LOCK          0

#define FF_FS_REENTRANT     0

#endif /* FFCONF_H */