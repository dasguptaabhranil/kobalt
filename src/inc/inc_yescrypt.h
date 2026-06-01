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

#pragma once

#include <stdint.h>
#include <stddef.h>

#define YESCRYPT_OK             0
#define YESCRYPT_ERR_PARAM     -1
#define YESCRYPT_ERR_MEMORY    -2
#define YESCRYPT_ERR_FORMAT    -3
#define YESCRYPT_ERR_MISMATCH  -4

#define YESCRYPT_HASH_MAXLEN    96u

typedef enum {
    YESCRYPT_REQ_HASH   = 1,
    YESCRYPT_REQ_VERIFY = 2,
} yescrypt_req_type_t;

typedef struct {
    volatile int            done;
    yescrypt_req_type_t     type;
    char                    password[256];
    char                    encoded_hash[YESCRYPT_HASH_MAXLEN];
    int                     result;
} yescrypt_req_t;

typedef struct {
    uint64_t total_submitted;
    uint64_t total_completed;
    uint64_t total_hashes;
    uint64_t total_verifies;
    uint64_t total_errors;
    uint64_t total_mismatches;
} yescrypt_stats_t;

int  yescrypt_daemon_start(void);

int  yescrypt_daemon_submit(yescrypt_req_t *req);

void yescrypt_daemon_wait(const yescrypt_req_t *req);

void yescrypt_daemon_release(int slot);

void yescrypt_daemon_stats(yescrypt_stats_t *out);
