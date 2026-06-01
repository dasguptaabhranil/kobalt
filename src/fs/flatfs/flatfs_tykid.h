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

#ifndef FLATFS_TYKID_H
#define FLATFS_TYKID_H

#include "flatfs.h"

#ifdef KOBALT_KERNEL_IDENT
#  include "tykid.h"
#  define FLATFS_TYKID_AVAILABLE 1
#else
#  define FLATFS_TYKID_AVAILABLE 0
#endif

flatfs_err_t flatfs_tykid_bind(tykid_gate_ctx_t *ctx);

void flatfs_tykid_unbind(void);

flatfs_err_t flatfs_tykid_seal_check(void);

uint64_t flatfs_tykid_entropy(void);

void flatfs_tykid_mac_set(void *blk, size_t sz);
int  flatfs_tykid_mac_ok(const void *blk, size_t sz);

void flatfs_tykid_audit_mount(int ok);

void flatfs_tykid_audit_unmount(void);

void flatfs_tykid_audit_journal(int committed);

void flatfs_tykid_audit_crc_err(uint64_t blk);

void flatfs_tykid_audit_super_corrupt(void);

void flatfs_tykid_audit_readonly(void);

void flatfs_tykid_audit_handoff(int yielding);

#endif
