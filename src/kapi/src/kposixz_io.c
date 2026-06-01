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

#define PIPE_SZ     4096
#define EPOLL_MAX   64

typedef struct {
    u8             buf[PIPE_SZ];
    u32            rd, wr, len;
    kpz_spinlock_t lk;
    volatile s32   nrd, nwr;
} kpz_pipe_t;

typedef struct {
    kpz_fd_t       fd;
    u32            events;
    u64            data;
} kpz_epoll_slot_t;

typedef struct {
    kpz_epoll_slot_t slots[EPOLL_MAX];
    u32              n;
    kpz_spinlock_t   lk;
} kpz_epoll_t;

typedef struct {
    u64            expire_ns;
    u64            interval_ns;
    volatile u64   expirations;
    kpz_spinlock_t lk;
} kpz_timerfd_t;

typedef struct {
    volatile u64   val;
    u32            flags;
    kpz_spinlock_t lk;
} kpz_eventfd_t;

static s64 pipe_read(kposixz_file_t *f, void *buf, u64 len)
{
    kpz_pipe_t *p = (kpz_pipe_t *)f->priv;
    u8 *dst = (u8 *)buf;
    u64 got = 0;

    kpz_spin_lock(&p->lk);
    while (got < len && p->len > 0) {
        dst[got++] = p->buf[p->rd];
        p->rd = (p->rd + 1) & (PIPE_SZ - 1);
        p->len--;
    }
    kpz_spin_unlock(&p->lk);

    if (!got && !p->nwr) return 0;
    if (!got) return KPZ_ERR(KPZE_AGAIN);
    return (s64)got;
}

static s64 pipe_write(kposixz_file_t *f, const void *buf, u64 len)
{
    kpz_pipe_t *p = (kpz_pipe_t *)f->priv;
    const u8 *src = (const u8 *)buf;
    u64 put = 0;

    if (!p->nrd) return KPZ_ERR(KPZE_PIPE);

    kpz_spin_lock(&p->lk);
    while (put < len && p->len < PIPE_SZ) {
        p->buf[p->wr] = src[put++];
        p->wr = (p->wr + 1) & (PIPE_SZ - 1);
        p->len++;
    }
    kpz_spin_unlock(&p->lk);

    return put ? (s64)put : KPZ_ERR(KPZE_AGAIN);
}

static s64 pipe_dummy_write(kposixz_file_t *f, const void *buf, u64 len)
{
    (void)f; (void)buf; (void)len;
    return KPZ_ERR(KPZE_BADF);
}

static s64 pipe_dummy_read(kposixz_file_t *f, void *buf, u64 len)
{
    (void)f; (void)buf; (void)len;
    return KPZ_ERR(KPZE_BADF);
}

static u32 pipe_rd_poll(kposixz_file_t *f, u32 events)
{
    kpz_pipe_t *p = (kpz_pipe_t *)f->priv;
    u32 r = 0;
    if ((events & KPZ_POLLIN) && p->len > 0) r |= KPZ_POLLIN;
    if (!p->nwr) r |= KPZ_POLLHUP;
    return r;
}

static u32 pipe_wr_poll(kposixz_file_t *f, u32 events)
{
    kpz_pipe_t *p = (kpz_pipe_t *)f->priv;
    u32 r = 0;
    if ((events & KPZ_POLLOUT) && p->len < PIPE_SZ) r |= KPZ_POLLOUT;
    if (!p->nrd) r |= KPZ_POLLERR;
    return r;
}

static void pipe_rd_close(kposixz_file_t *f)
{
    kpz_pipe_t *p = (kpz_pipe_t *)f->priv;
    if (kpz_atomic_dec(&p->nrd) == 0 && kpz_atomic_load(&p->nwr) == 0)
        kfree(p);
}

static void pipe_wr_close(kposixz_file_t *f)
{
    kpz_pipe_t *p = (kpz_pipe_t *)f->priv;
    if (kpz_atomic_dec(&p->nwr) == 0 && kpz_atomic_load(&p->nrd) == 0)
        kfree(p);
}

static const kpz_vfs_ops_t pipe_rd_ops = {
    .read  = pipe_read,
    .write = pipe_dummy_write,
    .poll  = pipe_rd_poll,
    .close = pipe_rd_close,
};

static const kpz_vfs_ops_t pipe_wr_ops = {
    .read  = pipe_dummy_read,
    .write = pipe_write,
    .poll  = pipe_wr_poll,
    .close = pipe_wr_close,
};

s64 kpz_sys_pipe(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    s32 *fds = (s32 *)f->arg1;
    if (kpz_check_ptr(fds, 8)) return KPZ_ERR(KPZE_FAULT);

    kpz_pipe_t *pb = (kpz_pipe_t *)kmalloc(sizeof(kpz_pipe_t));
    if (!pb) return KPZ_ERR(KPZE_NOMEM);
    kpz_memzero(pb, sizeof(*pb));
    pb->nrd = 1; pb->nwr = 1;

    kposixz_file_t *rf = (kposixz_file_t *)kmalloc(sizeof(kposixz_file_t));
    kposixz_file_t *wf = (kposixz_file_t *)kmalloc(sizeof(kposixz_file_t));
    if (!rf || !wf) {
        kfree(pb); kfree(rf); kfree(wf);
        return KPZ_ERR(KPZE_NOMEM);
    }

    kpz_memzero(rf, sizeof(*rf));
    rf->ops = &pipe_rd_ops; rf->priv = pb; rf->flags = KPZ_O_RDONLY; rf->refcount = 1;

    kpz_memzero(wf, sizeof(*wf));
    wf->ops = &pipe_wr_ops; wf->priv = pb; wf->flags = KPZ_O_WRONLY; wf->refcount = 1;

    s32 rfd = kpz_fd_alloc(proc, rf);
    s32 wfd = kpz_fd_alloc(proc, wf);
    if (rfd < 0 || wfd < 0) {
        if (rfd >= 0) kpz_fd_close(proc, rfd); else kpz_fd_put(rf);
        kpz_fd_put(wf);
        return KPZ_ERR(KPZE_MFILE);
    }

    fds[0] = rfd; fds[1] = wfd;
    return 0;
}

static u32 file_poll(kposixz_proc_t *proc, kpz_fd_t fd, u32 events)
{
    kposixz_file_t *f = kpz_fd_get(proc, fd);
    if (!f) return KPZ_POLLNVAL;
    u32 r = f->ops->poll ? f->ops->poll(f, events)
                         : (events & (KPZ_POLLIN | KPZ_POLLOUT));
    kpz_fd_put(f);
    return r;
}

s64 kpz_sys_poll(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    kpz_pollfd_t *fds = (kpz_pollfd_t *)f->arg1;
    s32 nfds  = (s32)f->arg2;
    s32 toms  = (s32)f->arg3;
    (void)toms;

    if (nfds < 0 || nfds > KPOSIXZ_MAX_FD) return KPZ_ERR(KPZE_INVAL);
    if (kpz_check_ptr(fds, (usz)nfds * sizeof(*fds))) return KPZ_ERR(KPZE_FAULT);

    s32 n = 0;
    for (s32 i = 0; i < nfds; i++) {
        fds[i].revents = 0;
        if (fds[i].fd < 0) continue;
        u32 rev = file_poll(proc, fds[i].fd, (u32)fds[i].events);
        fds[i].revents = (s16)(rev & 0xffff);
        if (rev) n++;
    }
    return (s64)n;
}

s64 kpz_sys_select(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    s32           nfds  = (s32)f->arg1;
    kpz_fd_set_t *rfds  = (kpz_fd_set_t *)f->arg2;
    kpz_fd_set_t *wfds  = (kpz_fd_set_t *)f->arg3;
    kpz_fd_set_t *efds  = (kpz_fd_set_t *)f->arg4;

    if (nfds < 0 || nfds > 1024) return KPZ_ERR(KPZE_INVAL);

    kpz_fd_set_t ro, wo, eo;
    kpz_memzero(&ro, sizeof(ro));
    kpz_memzero(&wo, sizeof(wo));
    kpz_memzero(&eo, sizeof(eo));

    s32 n = 0;
    for (s32 i = 0; i < nfds; i++) {
        u32 word = (u32)i / 64, bit = (u32)i % 64;
        u32 want = 0;
        if (rfds && (rfds->fds_bits[word] >> bit) & 1) want |= KPZ_POLLIN;
        if (wfds && (wfds->fds_bits[word] >> bit) & 1) want |= KPZ_POLLOUT;
        if (!want) continue;

        u32 rev = file_poll(proc, (kpz_fd_t)i, want);
        if (rev & KPZ_POLLIN)  { ro.fds_bits[word] |= (1ULL << bit); n++; }
        if (rev & KPZ_POLLOUT) { wo.fds_bits[word] |= (1ULL << bit); n++; }
        (void)eo; (void)efds;
    }

    if (rfds) kpz_memcpy(rfds, &ro, sizeof(ro));
    if (wfds) kpz_memcpy(wfds, &wo, sizeof(wo));
    if (efds) kpz_memzero(efds, sizeof(*efds));
    return (s64)n;
}

static s64 epoll_close(kposixz_file_t *f)
{
    kfree(f->priv);
    f->priv = (void *)0;
    return 0;
}

static void epoll_file_close(kposixz_file_t *f)
{
    epoll_close(f);
}

static const kpz_vfs_ops_t epoll_ops = {
    .close = epoll_file_close,
};

s64 kpz_sys_epoll_create1(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);
    u32 flags = (u32)f->arg1;

    kpz_epoll_t *ep = (kpz_epoll_t *)kmalloc(sizeof(kpz_epoll_t));
    if (!ep) return KPZ_ERR(KPZE_NOMEM);
    kpz_memzero(ep, sizeof(*ep));

    kposixz_file_t *ef = (kposixz_file_t *)kmalloc(sizeof(kposixz_file_t));
    if (!ef) { kfree(ep); return KPZ_ERR(KPZE_NOMEM); }
    kpz_memzero(ef, sizeof(*ef));
    ef->ops      = &epoll_ops;
    ef->priv     = ep;
    ef->flags    = (flags & KPZ_O_CLOEXEC) ? KPZ_O_CLOEXEC : 0;
    ef->refcount = 1;

    s32 fd = kpz_fd_alloc(proc, ef);
    if (fd < 0) { kpz_fd_put(ef); return KPZ_ERR(KPZE_MFILE); }
    if (flags & KPZ_O_CLOEXEC) proc->fds.cloexec[fd] = 1;
    return (s64)fd;
}

s64 kpz_sys_epoll_ctl(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    kpz_fd_t epfd = (kpz_fd_t)f->arg1;
    s32      op   = (s32)f->arg2;
    kpz_fd_t tfd  = (kpz_fd_t)f->arg3;
    kpz_epoll_event_t *ev = (kpz_epoll_event_t *)f->arg4;

    kposixz_file_t *ef = kpz_fd_get(proc, epfd);
    if (!ef || ef->ops != &epoll_ops) { kpz_fd_put(ef); return KPZ_ERR(KPZE_BADF); }

    kpz_epoll_t *ep = (kpz_epoll_t *)ef->priv;
    kpz_spin_lock(&ep->lk);

    s64 ret = 0;
    switch (op) {
    case KPZ_EPOLL_CTL_ADD:
        if (!ev || kpz_check_ptr(ev, sizeof(*ev))) { ret = KPZ_ERR(KPZE_FAULT); break; }
        if (ep->n >= EPOLL_MAX) { ret = KPZ_ERR(KPZE_NOSPC); break; }
        for (u32 i = 0; i < ep->n; i++) {
            if (ep->slots[i].fd == tfd) { ret = KPZ_ERR(KPZE_EXIST); goto out; }
        }
        ep->slots[ep->n].fd     = tfd;
        ep->slots[ep->n].events = ev->events;
        ep->slots[ep->n].data   = ev->data;
        ep->n++;
        break;
    case KPZ_EPOLL_CTL_DEL:
        for (u32 i = 0; i < ep->n; i++) {
            if (ep->slots[i].fd == tfd) {
                ep->slots[i] = ep->slots[--ep->n];
                goto out;
            }
        }
        ret = KPZ_ERR(KPZE_NOENT);
        break;
    case KPZ_EPOLL_CTL_MOD:
        if (!ev || kpz_check_ptr(ev, sizeof(*ev))) { ret = KPZ_ERR(KPZE_FAULT); break; }
        for (u32 i = 0; i < ep->n; i++) {
            if (ep->slots[i].fd == tfd) {
                ep->slots[i].events = ev->events;
                ep->slots[i].data   = ev->data;
                goto out;
            }
        }
        ret = KPZ_ERR(KPZE_NOENT);
        break;
    default:
        ret = KPZ_ERR(KPZE_INVAL);
    }
out:
    kpz_spin_unlock(&ep->lk);
    kpz_fd_put(ef);
    return ret;
}

s64 kpz_sys_epoll_wait(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    kpz_fd_t          epfd   = (kpz_fd_t)f->arg1;
    kpz_epoll_event_t *evbuf = (kpz_epoll_event_t *)f->arg2;
    s32                maxev = (s32)f->arg3;

    if (maxev <= 0) return KPZ_ERR(KPZE_INVAL);
    if (kpz_check_ptr(evbuf, (usz)maxev * sizeof(*evbuf))) return KPZ_ERR(KPZE_FAULT);

    kposixz_file_t *ef = kpz_fd_get(proc, epfd);
    if (!ef || ef->ops != &epoll_ops) { kpz_fd_put(ef); return KPZ_ERR(KPZE_BADF); }

    kpz_epoll_t *ep = (kpz_epoll_t *)ef->priv;
    s32 n = 0;

    kpz_spin_lock(&ep->lk);
    for (u32 i = 0; i < ep->n && n < maxev; i++) {
        u32 rev = file_poll(proc, ep->slots[i].fd, ep->slots[i].events);
        if (!rev) continue;
        evbuf[n].events = rev;
        evbuf[n].data   = ep->slots[i].data;
        n++;
    }
    kpz_spin_unlock(&ep->lk);

    kpz_fd_put(ef);
    return (s64)n;
}

static s64 timerfd_read(kposixz_file_t *f, void *buf, u64 len)
{
    if (len < 8) return KPZ_ERR(KPZE_INVAL);
    kpz_timerfd_t *t = (kpz_timerfd_t *)f->priv;

    u64 now = kobalt_acpi_timer_ns();
    u64 exp = 0;

    kpz_spin_lock(&t->lk);
    if (now >= t->expire_ns && t->expire_ns > 0) {
        exp = 1 + t->expirations;
        t->expirations = 0;
        if (t->interval_ns)
            t->expire_ns = now + t->interval_ns;
        else
            t->expire_ns = 0;
    }
    kpz_spin_unlock(&t->lk);

    if (!exp) return KPZ_ERR(KPZE_AGAIN);
    kpz_memcpy(buf, &exp, 8);
    return 8;
}

static u32 timerfd_poll(kposixz_file_t *f, u32 events)
{
    kpz_timerfd_t *t = (kpz_timerfd_t *)f->priv;
    u64 now = kobalt_acpi_timer_ns();
    if ((events & KPZ_POLLIN) && t->expire_ns && now >= t->expire_ns)
        return KPZ_POLLIN;
    return 0;
}

static void timerfd_close(kposixz_file_t *f)
{
    kfree(f->priv);
    f->priv = (void *)0;
}

static const kpz_vfs_ops_t timerfd_ops = {
    .read  = timerfd_read,
    .poll  = timerfd_poll,
    .close = timerfd_close,
};

s64 kpz_sys_timerfd_create(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    s32 clkid = (s32)f->arg1;
    s32 flags = (s32)f->arg2;
    if (clkid != KPZ_CLOCK_REALTIME_id && clkid != KPZ_CLOCK_MONOTONIC_id)
        return KPZ_ERR(KPZE_INVAL);

    kpz_timerfd_t *t = (kpz_timerfd_t *)kmalloc(sizeof(kpz_timerfd_t));
    if (!t) return KPZ_ERR(KPZE_NOMEM);
    kpz_memzero(t, sizeof(*t));

    kposixz_file_t *tf = (kposixz_file_t *)kmalloc(sizeof(kposixz_file_t));
    if (!tf) { kfree(t); return KPZ_ERR(KPZE_NOMEM); }
    kpz_memzero(tf, sizeof(*tf));
    tf->ops      = &timerfd_ops;
    tf->priv     = t;
    tf->flags    = (flags & KPZ_TFD_NONBLOCK) ? KPZ_O_NONBLOCK : 0;
    tf->refcount = 1;

    s32 fd = kpz_fd_alloc(proc, tf);
    if (fd < 0) { kpz_fd_put(tf); return KPZ_ERR(KPZE_MFILE); }
    if (flags & KPZ_TFD_CLOEXEC) proc->fds.cloexec[fd] = 1;
    return (s64)fd;
}

static s64 eventfd_read(kposixz_file_t *f, void *buf, u64 len)
{
    if (len < 8) return KPZ_ERR(KPZE_INVAL);
    kpz_eventfd_t *e = (kpz_eventfd_t *)f->priv;

    kpz_spin_lock(&e->lk);
    u64 v = kpz_atomic_load(&e->val);
    if (!v) { kpz_spin_unlock(&e->lk); return KPZ_ERR(KPZE_AGAIN); }

    if (e->flags & KPZ_EFD_SEMAPHORE) {
        u64 one = 1;
        kpz_memcpy(buf, &one, 8);
        kpz_atomic_dec(&e->val);
    } else {
        kpz_memcpy(buf, &v, 8);
        kpz_atomic_store(&e->val, 0ULL);
    }
    kpz_spin_unlock(&e->lk);
    return 8;
}

static s64 eventfd_write(kposixz_file_t *f, const void *buf, u64 len)
{
    if (len < 8) return KPZ_ERR(KPZE_INVAL);
    kpz_eventfd_t *e = (kpz_eventfd_t *)f->priv;
    u64 add;
    kpz_memcpy(&add, buf, 8);
    if (add == ~0ULL) return KPZ_ERR(KPZE_INVAL);
    __atomic_add_fetch(&e->val, add, __ATOMIC_SEQ_CST);
    return 8;
}

static u32 eventfd_poll(kposixz_file_t *f, u32 events)
{
    kpz_eventfd_t *e = (kpz_eventfd_t *)f->priv;
    u32 r = 0;
    if ((events & KPZ_POLLIN)  && kpz_atomic_load(&e->val) > 0) r |= KPZ_POLLIN;
    if ((events & KPZ_POLLOUT) && kpz_atomic_load(&e->val) < ~0ULL - 1) r |= KPZ_POLLOUT;
    return r;
}

static void eventfd_close(kposixz_file_t *f)
{
    kfree(f->priv);
    f->priv = (void *)0;
}

static const kpz_vfs_ops_t eventfd_ops = {
    .read  = eventfd_read,
    .write = eventfd_write,
    .poll  = eventfd_poll,
    .close = eventfd_close,
};

s64 kpz_sys_eventfd(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    u64 initval = f->arg1;
    u32 flags   = (u32)f->arg2;

    kpz_eventfd_t *e = (kpz_eventfd_t *)kmalloc(sizeof(kpz_eventfd_t));
    if (!e) return KPZ_ERR(KPZE_NOMEM);
    kpz_memzero(e, sizeof(*e));
    e->val   = initval;
    e->flags = flags;

    kposixz_file_t *ef = (kposixz_file_t *)kmalloc(sizeof(kposixz_file_t));
    if (!ef) { kfree(e); return KPZ_ERR(KPZE_NOMEM); }
    kpz_memzero(ef, sizeof(*ef));
    ef->ops      = &eventfd_ops;
    ef->priv     = e;
    ef->flags    = (flags & KPZ_EFD_NONBLOCK) ? KPZ_O_NONBLOCK : 0;
    ef->refcount = 1;

    s32 fd = kpz_fd_alloc(proc, ef);
    if (fd < 0) { kpz_fd_put(ef); return KPZ_ERR(KPZE_MFILE); }
    if (flags & KPZ_EFD_CLOEXEC) proc->fds.cloexec[fd] = 1;
    return (s64)fd;
}
