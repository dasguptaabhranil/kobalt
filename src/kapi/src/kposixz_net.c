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

#include "kposixz_internal.h"
#include "../../net/kpz_socket.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

struct sockaddr { u16 sa_family; char sa_data[14]; };
typedef unsigned int kpz_socklen_t;

extern int lwip_bind       (int s, const struct sockaddr *name, kpz_socklen_t namelen);
extern int lwip_connect    (int s, const struct sockaddr *name, kpz_socklen_t namelen);
extern int lwip_listen     (int s, int backlog);
extern int lwip_accept     (int s, struct sockaddr *addr, kpz_socklen_t *addrlen);
extern int lwip_shutdown   (int s, int how);
extern int lwip_sendto     (int s, const void *dataptr, unsigned int size, int flags,
                            const struct sockaddr *to, kpz_socklen_t tolen);
extern int lwip_recvfrom   (int s, void *mem, unsigned int len, int flags,
                            struct sockaddr *from, kpz_socklen_t *fromlen);
extern int lwip_getsockname(int s, struct sockaddr *name, kpz_socklen_t *namelen);
extern int lwip_getpeername(int s, struct sockaddr *name, kpz_socklen_t *namelen);
extern int lwip_setsockopt (int s, int level, int optname,
                            const void *optval, kpz_socklen_t optlen);
extern int lwip_getsockopt (int s, int level, int optname,
                            void *optval, kpz_socklen_t *optlen);
extern int lwip_ioctl      (int s, long cmd, void *argp);

#define SOCK_NONBLOCK  0x800
#define SOCK_CLOEXEC   0x80000
#define FIONBIO        0x5421

static int get_lwfd(kposixz_proc_t *proc, kpz_fd_t fd, kposixz_file_t **out)
{
    kposixz_file_t *f = kpz_fd_get(proc, fd);
    if (!f) { *out = NULL; return -1; }
    int lfd = kpz_sock_get_lwfd(f);
    if (lfd < 0) { kpz_fd_put(f); *out = NULL; return -1; }
    *out = f;
    return lfd;
}

s64 kpz_sys_socket(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    kposixz_file_t *file = kpz_socket_create((int)f->arg1, (int)f->arg2, (int)f->arg3);
    if (!file) return KPZ_ERR(KPZE_MFILE);

    s32 fd = kpz_fd_alloc(proc, file);
    if (fd < 0) { kpz_fd_put(file); return KPZ_ERR(KPZE_MFILE); }
    return (s64)fd;
}

s64 kpz_sys_bind(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    const struct sockaddr *addr = (const struct sockaddr *)f->arg2;
    kpz_socklen_t addrlen = (kpz_socklen_t)f->arg3;
    if (kpz_check_ptr(addr, addrlen)) return KPZ_ERR(KPZE_FAULT);

    kposixz_file_t *file;
    int lfd = get_lwfd(proc, (kpz_fd_t)f->arg1, &file);
    if (lfd < 0) return KPZ_ERR(KPZE_NOTSOCK);

    int rc = lwip_bind(lfd, addr, addrlen);
    kpz_fd_put(file);
    return rc < 0 ? KPZ_ERR(KPZE_INVAL) : 0;
}

s64 kpz_sys_connect(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    const struct sockaddr *addr = (const struct sockaddr *)f->arg2;
    kpz_socklen_t addrlen = (kpz_socklen_t)f->arg3;
    if (kpz_check_ptr(addr, addrlen)) return KPZ_ERR(KPZE_FAULT);

    kposixz_file_t *file;
    int lfd = get_lwfd(proc, (kpz_fd_t)f->arg1, &file);
    if (lfd < 0) return KPZ_ERR(KPZE_NOTSOCK);

    int rc = lwip_connect(lfd, addr, addrlen);
    kpz_fd_put(file);
    return rc < 0 ? KPZ_ERR(KPZE_CONNREFUSED) : 0;
}

s64 kpz_sys_listen(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    kposixz_file_t *file;
    int lfd = get_lwfd(proc, (kpz_fd_t)f->arg1, &file);
    if (lfd < 0) return KPZ_ERR(KPZE_NOTSOCK);

    int rc = lwip_listen(lfd, (int)f->arg2);
    kpz_fd_put(file);
    return rc < 0 ? KPZ_ERR(KPZE_INVAL) : 0;
}

s64 kpz_sys_accept(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    struct sockaddr  *addr = (struct sockaddr *)f->arg2;
    kpz_socklen_t    *alen = (kpz_socklen_t *)f->arg3;
    if (addr && alen && kpz_check_ptr(addr, *alen)) return KPZ_ERR(KPZE_FAULT);

    kposixz_file_t *server;
    int server_lfd = get_lwfd(proc, (kpz_fd_t)f->arg1, &server);
    if (server_lfd < 0) return KPZ_ERR(KPZE_NOTSOCK);

    int client_lfd = lwip_accept(server_lfd, addr, alen);
    kpz_fd_put(server);
    if (client_lfd < 0) return KPZ_ERR(KPZE_AGAIN);

    kposixz_file_t *client = kpz_socket_wrap(client_lfd);
    if (!client) { kpz_fd_put(client); return KPZ_ERR(KPZE_MFILE); }

    s32 new_fd = kpz_fd_alloc(proc, client);
    if (new_fd < 0) { kpz_fd_put(client); return KPZ_ERR(KPZE_MFILE); }
    return (s64)new_fd;
}

s64 kpz_sys_accept4(kpz_frame_t *f)
{
    int flags = (int)f->arg4;
    kpz_frame_t tmp = *f; tmp.arg4 = 0;
    s64 new_fd = kpz_sys_accept(&tmp);
    if (new_fd < 0) return new_fd;

    kposixz_proc_t *proc = kpz_current();
    if (!proc) return new_fd;

    if (flags & SOCK_CLOEXEC)
        proc->fds.cloexec[(kpz_fd_t)new_fd] = 1;

    if (flags & SOCK_NONBLOCK) {
        kposixz_file_t *nf;
        int lfd = get_lwfd(proc, (kpz_fd_t)new_fd, &nf);
        if (lfd >= 0) {
            int one = 1;
            lwip_ioctl(lfd, FIONBIO, &one);
            kpz_fd_put(nf);
        }
    }
    return new_fd;
}

s64 kpz_sys_sendto(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    const void           *buf   = (const void *)f->arg2;
    usz                   len   = (usz)f->arg3;
    int                   flags = (int)f->arg4;
    const struct sockaddr *dest = (const struct sockaddr *)f->arg5;
    kpz_socklen_t          alen = (kpz_socklen_t)f->arg6;

    if (kpz_check_ptr(buf, len)) return KPZ_ERR(KPZE_FAULT);
    if (dest && kpz_check_ptr(dest, alen)) return KPZ_ERR(KPZE_FAULT);

    kposixz_file_t *file;
    int lfd = get_lwfd(proc, (kpz_fd_t)f->arg1, &file);
    if (lfd < 0) return KPZ_ERR(KPZE_NOTSOCK);

    int n = lwip_sendto(lfd, buf, (unsigned int)len, flags, dest, alen);
    kpz_fd_put(file);
    return n < 0 ? KPZ_ERR(KPZE_IO) : (s64)n;
}

s64 kpz_sys_recvfrom(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    void            *buf   = (void *)f->arg2;
    usz              len   = (usz)f->arg3;
    int              flags = (int)f->arg4;
    struct sockaddr *src   = (struct sockaddr *)f->arg5;
    kpz_socklen_t   *alen  = (kpz_socklen_t *)f->arg6;

    if (kpz_check_ptr(buf, len)) return KPZ_ERR(KPZE_FAULT);
    if (src && alen && kpz_check_ptr(src, *alen)) return KPZ_ERR(KPZE_FAULT);

    kposixz_file_t *file;
    int lfd = get_lwfd(proc, (kpz_fd_t)f->arg1, &file);
    if (lfd < 0) return KPZ_ERR(KPZE_NOTSOCK);

    int n = lwip_recvfrom(lfd, buf, (unsigned int)len, flags, src, alen);
    kpz_fd_put(file);
    return n < 0 ? KPZ_ERR(KPZE_IO) : (s64)n;
}

s64 kpz_sys_shutdown(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    kposixz_file_t *file;
    int lfd = get_lwfd(proc, (kpz_fd_t)f->arg1, &file);
    if (lfd < 0) return KPZ_ERR(KPZE_NOTSOCK);

    int rc = lwip_shutdown(lfd, (int)f->arg2);
    kpz_fd_put(file);
    return rc < 0 ? KPZ_ERR(KPZE_INVAL) : 0;
}

s64 kpz_sys_getsockname(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    struct sockaddr *addr = (struct sockaddr *)f->arg2;
    kpz_socklen_t   *alen = (kpz_socklen_t *)f->arg3;
    if (!addr || !alen) return KPZ_ERR(KPZE_FAULT);
    if (kpz_check_ptr(alen, sizeof(*alen))) return KPZ_ERR(KPZE_FAULT);
    if (kpz_check_ptr(addr, *alen))         return KPZ_ERR(KPZE_FAULT);

    kposixz_file_t *file;
    int lfd = get_lwfd(proc, (kpz_fd_t)f->arg1, &file);
    if (lfd < 0) return KPZ_ERR(KPZE_NOTSOCK);

    int rc = lwip_getsockname(lfd, addr, alen);
    kpz_fd_put(file);
    return rc < 0 ? KPZ_ERR(KPZE_INVAL) : 0;
}

s64 kpz_sys_getpeername(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    struct sockaddr *addr = (struct sockaddr *)f->arg2;
    kpz_socklen_t   *alen = (kpz_socklen_t *)f->arg3;
    if (!addr || !alen) return KPZ_ERR(KPZE_FAULT);
    if (kpz_check_ptr(alen, sizeof(*alen))) return KPZ_ERR(KPZE_FAULT);
    if (kpz_check_ptr(addr, *alen))         return KPZ_ERR(KPZE_FAULT);

    kposixz_file_t *file;
    int lfd = get_lwfd(proc, (kpz_fd_t)f->arg1, &file);
    if (lfd < 0) return KPZ_ERR(KPZE_NOTSOCK);

    int rc = lwip_getpeername(lfd, addr, alen);
    kpz_fd_put(file);
    return rc < 0 ? KPZ_ERR(KPZE_NOTCONN) : 0;
}

s64 kpz_sys_setsockopt(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    const void    *optval = (const void *)f->arg4;
    kpz_socklen_t  optlen = (kpz_socklen_t)f->arg5;
    if (kpz_check_ptr(optval, optlen)) return KPZ_ERR(KPZE_FAULT);

    kposixz_file_t *file;
    int lfd = get_lwfd(proc, (kpz_fd_t)f->arg1, &file);
    if (lfd < 0) return KPZ_ERR(KPZE_NOTSOCK);

    int rc = lwip_setsockopt(lfd, (int)f->arg2, (int)f->arg3, optval, optlen);
    kpz_fd_put(file);
    return rc < 0 ? KPZ_ERR(KPZE_INVAL) : 0;
}

s64 kpz_sys_getsockopt(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    void          *optval = (void *)f->arg4;
    kpz_socklen_t *optlen = (kpz_socklen_t *)f->arg5;
    if (!optval || !optlen) return KPZ_ERR(KPZE_FAULT);
    if (kpz_check_ptr(optlen, sizeof(*optlen))) return KPZ_ERR(KPZE_FAULT);
    if (kpz_check_ptr(optval, *optlen))         return KPZ_ERR(KPZE_FAULT);

    kposixz_file_t *file;
    int lfd = get_lwfd(proc, (kpz_fd_t)f->arg1, &file);
    if (lfd < 0) return KPZ_ERR(KPZE_NOTSOCK);

    int rc = lwip_getsockopt(lfd, (int)f->arg2, (int)f->arg3, optval, optlen);
    kpz_fd_put(file);
    return rc < 0 ? KPZ_ERR(KPZE_INVAL) : 0;
}
