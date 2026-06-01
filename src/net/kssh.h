/* Copyright (c) 2026  Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef KOBALT_KSSH_H
#define KOBALT_KSSH_H

#include <stdint.h>
#include <stddef.h>
#include <crypto.h>

typedef void (*kssh_digest_hook_fn)(const uint8_t digest[CRYPTO_SHA256_DIGEST_SIZE],
                                    uint64_t iteration);
extern volatile kssh_digest_hook_fn kssh_integrity_hook;

void kssh_tick_notify(uint64_t now_ticks);

void crypto_daemon(void *arg);

#endif