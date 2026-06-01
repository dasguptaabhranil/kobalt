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
#include <stddef.h>
#include <stdarg.h>

int kprintf(const char *fmt, ...);
int ksnprintf(char *buf, size_t n, const char *fmt, ...);
int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
void kputs(const char *s);

void klog_status(const char *subsys, const char *detail, const char *tag, const char *msg);
void klog_ok(const char *subsys, const char *detail);
void klog_fail(const char *subsys, const char *detail);
void klog_info(const char *subsys, const char *detail);
void klog_warn(const char *subsys, const char *detail);
