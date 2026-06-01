/* Copyright (C) 2026  Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * src/net/kpz_socket.c — lwIP socket <-> kposixz_file_t bridge.
 *
 * We forward-declare the lwIP socket API manually instead of relying on
 * <lwip/sockets.h> so this file compiles regardless of LWIP_SOCKET / 
 * LWIP_COMPAT_SOCKETS settings in lwipopts.h.
 */

#include "kpz_socket.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

/* ── lwIP socket API — forward declarations ──────────────────────────────────
 * These are always compiled into lwIP when LWIP_SOCKET=1.
 * Declaring them here avoids any dependency on lwipopts.h include order.
 */
struct sockaddr;
typedef unsigned int lwip_socklen_t;

extern int lwip_socket  (int domain, int type, int protocol);
extern int lwip_close   (int s);
extern int lwip_read    (int s, void *mem, unsigned int len);
extern int lwip_write   (int s, const void *dataptr, unsigned int size);
extern int lwip_ioctl   (int s, long cmd, void *argp);
extern int lwip_bind    (int s, const struct sockaddr *name, lwip_socklen_t namelen);
extern int lwip_connect (int s, const struct sockaddr *name, lwip_socklen_t namelen);
extern int lwip_listen  (int s, int backlog);
extern int lwip_accept  (int s, struct sockaddr *addr, lwip_socklen_t *addrlen);
extern int lwip_shutdown(int s, int how);
extern int lwip_sendto  (int s, const void *dataptr, unsigned int size, int flags,
                         const struct sockaddr *to, lwip_socklen_t tolen);
extern int lwip_recvfrom(int s, void *mem, unsigned int len, int flags,
                         struct sockaddr *from, lwip_socklen_t *fromlen);
extern int lwip_getsockname(int s, struct sockaddr *name, lwip_socklen_t *namelen);
extern int lwip_getpeername(int s, struct sockaddr *name, lwip_socklen_t *namelen);
extern int lwip_setsockopt(int s, int level, int optname,
                           const void *optval, lwip_socklen_t optlen);
extern int lwip_getsockopt(int s, int level, int optname,
                           void *optval, lwip_socklen_t *optlen);

/* ── Magic cookie ─────────────────────────────────────────────────────────── */
#define KPZ_SOCK_MAGIC  0x534F434Bu     /* 'SOCK' */

/* ── Private state ────────────────────────────────────────────────────────── */
typedef struct {
    u32 magic;
    int lwip_fd;
    int domain;
    int type;
    int protocol;
} kpz_sock_priv_t;

/* ── VFS op implementations ───────────────────────────────────────────────── */

static s64 sock_read(kposixz_file_t *f, void *buf, u64 len)
{
    kpz_sock_priv_t *s = (kpz_sock_priv_t *)f->priv;
    int n = lwip_read(s->lwip_fd, buf, (unsigned int)len);
    if (n < 0) return KPZ_ERR(KPZE_IO);
    return (s64)n;
}

static s64 sock_write(kposixz_file_t *f, const void *buf, u64 len)
{
    kpz_sock_priv_t *s = (kpz_sock_priv_t *)f->priv;
    int n = lwip_write(s->lwip_fd, buf, (unsigned int)len);
    if (n < 0) return KPZ_ERR(KPZE_IO);
    return (s64)n;
}

static void sock_close(kposixz_file_t *f)   /* void — matches kpz_vfs_ops_t */
{
    kpz_sock_priv_t *s = (kpz_sock_priv_t *)f->priv;
    lwip_close(s->lwip_fd);
    s->magic = 0;
    kfree(s);
    f->priv = NULL;
    kfree(f);
}

static s64 sock_ioctl(kposixz_file_t *f, u64 req, u64 arg)
{
    kpz_sock_priv_t *s = (kpz_sock_priv_t *)f->priv;
    return (s64)lwip_ioctl(s->lwip_fd, (long)req, (void *)(uptr)arg);
}

static s64 sock_seek(kposixz_file_t *f, s64 off, u32 whence)
{
    (void)f; (void)off; (void)whence;
    return KPZ_ERR(KPZE_SPIPE);
}

static const kpz_vfs_ops_t kpz_sock_ops = {
    .read  = sock_read,
    .write = sock_write,
    .seek  = sock_seek,
    .stat  = NULL,
    .ioctl = sock_ioctl,
    .close = sock_close,
};

/* ── Internal: wrap an already-open lwIP fd ───────────────────────────────── */

static kposixz_file_t *sock_wrap_lwfd(int lfd)
{
    kpz_sock_priv_t *priv = (kpz_sock_priv_t *)kmalloc(sizeof(*priv));
    if (!priv) return NULL;

    priv->magic    = KPZ_SOCK_MAGIC;
    priv->lwip_fd  = lfd;
    priv->domain   = 0;
    priv->type     = 0;
    priv->protocol = 0;

    kposixz_file_t *f = (kposixz_file_t *)kmalloc(sizeof(*f));
    if (!f) { kfree(priv); return NULL; }

    kpz_memzero(f, sizeof(*f));
    f->ops      = &kpz_sock_ops;
    f->priv     = priv;
    f->refcount = 1;
    return f;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

kposixz_file_t *kpz_socket_create(int domain, int type, int protocol)
{
    int lfd = lwip_socket(domain, type, protocol);
    if (lfd < 0) return NULL;

    kposixz_file_t *f = sock_wrap_lwfd(lfd);
    if (!f) { lwip_close(lfd); return NULL; }

    kpz_sock_priv_t *priv = (kpz_sock_priv_t *)f->priv;
    priv->domain   = domain;
    priv->type     = type;
    priv->protocol = protocol;
    return f;
}

kposixz_file_t *kpz_socket_wrap(int lwip_fd)
{
    return sock_wrap_lwfd(lwip_fd);
}

int kpz_sock_get_lwfd(kposixz_file_t *f)
{
    if (!f || !f->priv) return -1;
    kpz_sock_priv_t *s = (kpz_sock_priv_t *)f->priv;
    if (s->magic != KPZ_SOCK_MAGIC) return -1;
    return s->lwip_fd;
}