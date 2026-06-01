/* Copyright (C) 2026  Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * src/net/kpz_socket.h — lwIP socket backend for K-POSIXZ file descriptors.
 */

#pragma once

#include "../kapi/inc/kposixz_internal.h"

/* kpz_socket_create — allocate a kposixz_file_t wrapping a new lwIP socket.
 * Returns a file object with refcount=1 on success, NULL on failure.
 */
kposixz_file_t *kpz_socket_create(int domain, int type, int protocol);

/* kpz_socket_wrap — create a kposixz_file_t around an already-open lwIP fd.
 * Used by accept() to wrap the fd returned by lwip_accept().
 */
kposixz_file_t *kpz_socket_wrap(int lwip_fd);

/* kpz_sock_get_lwfd — get the underlying lwIP fd. Returns -1 if not a socket. */
int kpz_sock_get_lwfd(kposixz_file_t *f);