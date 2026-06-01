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
#include <vfs.h>

__attribute__((weak)) int random_read(void *buf, usz len)
{
    u8 *p = (u8 *)buf;
    u64 seed = kobalt_acpi_timer_ns();
    for (usz i = 0; i < len; i++) {
        seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
        p[i] = (u8)seed;
    }
    return (int)len;
}

s64 kpz_sys_getrandom(kpz_frame_t *f)
{
    void *buf   = (void *)f->arg1;
    usz   count = (usz)f->arg2;
    if (kpz_check_ptr(buf, count)) return KPZ_ERR(KPZE_FAULT);
    int rc = random_read(buf, count);
    return rc < 0 ? KPZ_ERR(KPZE_IO) : (s64)rc;
}

s64 kpz_sys_prctl(kpz_frame_t *f)
{
    s32 op  = (s32)f->arg1;
    u64 a2  = f->arg2;

    kposixz_proc_t *proc = kpz_current();

    switch (op) {
    case KPZ_PR_SET_NAME:
        return 0;
    case KPZ_PR_GET_NAME: {
        char *name = (char *)a2;
        if (kpz_check_ptr(name, 16)) return KPZ_ERR(KPZE_FAULT);
        kpz_strncpy(name, "kobalt", 16);
        return 0;
    }
    case KPZ_PR_SET_DUMPABLE:
    case KPZ_PR_GET_DUMPABLE:
        return 0;
    case KPZ_PR_SET_NO_NEW_PRIVS:
        return 0;
    case KPZ_PR_GET_NO_NEW_PRIVS:
        return 0;
    case KPZ_PR_SET_PDEATHSIG:
        return 0;
    case KPZ_PR_GET_PDEATHSIG: {
        s32 *sig = (s32 *)a2;
        if (kpz_check_ptr(sig, 4)) return KPZ_ERR(KPZE_FAULT);
        *sig = 0;
        return 0;
    }
    case KPZ_PR_SET_SECCOMP:
        return KPZ_ERR(KPZE_INVAL);
    case KPZ_PR_GET_SECCOMP:
        return 0;
    default:
        (void)proc;
        return KPZ_ERR(KPZE_INVAL);
    }
}

s64 kpz_sys_getrlimit(kpz_frame_t *f)
{
    u32            res = (u32)f->arg1;
    kpz_rlimit_t  *rl  = (kpz_rlimit_t *)f->arg2;

    if (res >= KPZ_RLIM_NLIMITS) return KPZ_ERR(KPZE_INVAL);
    if (kpz_check_ptr(rl, sizeof(*rl))) return KPZ_ERR(KPZE_FAULT);

    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    *rl = proc->rlimits[res];
    return 0;
}

s64 kpz_sys_setrlimit(kpz_frame_t *f)
{
    u32                  res = (u32)f->arg1;
    const kpz_rlimit_t  *rl  = (const kpz_rlimit_t *)f->arg2;

    if (res >= KPZ_RLIM_NLIMITS) return KPZ_ERR(KPZE_INVAL);
    if (kpz_check_ptr(rl, sizeof(*rl))) return KPZ_ERR(KPZE_FAULT);

    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    proc->rlimits[res] = *rl;
    return 0;
}

s64 kpz_sys_prlimit64(kpz_frame_t *f)
{
    kpz_pid_t       pid    = (kpz_pid_t)f->arg1;
    u32             res    = (u32)f->arg2;
    kpz_rlimit_t   *new_rl = (kpz_rlimit_t *)f->arg3;
    kpz_rlimit_t   *old_rl = (kpz_rlimit_t *)f->arg4;

    if (res >= KPZ_RLIM_NLIMITS) return KPZ_ERR(KPZE_INVAL);

    kposixz_proc_t *proc = pid ? kpz_proc_lookup(pid) : kpz_current();
    if (!proc) return KPZ_ERR(KPZE_NOENT);

    if (old_rl) {
        if (kpz_check_ptr(old_rl, sizeof(*old_rl))) return KPZ_ERR(KPZE_FAULT);
        *old_rl = proc->rlimits[res];
    }
    if (new_rl) {
        if (kpz_check_ptr(new_rl, sizeof(*new_rl))) return KPZ_ERR(KPZE_FAULT);
        proc->rlimits[res] = *new_rl;
    }
    return 0;
}

s64 kpz_sys_mremap(kpz_frame_t *f)
{
    uptr old_addr = (uptr)f->arg1;
    usz  old_sz   = (usz)f->arg2;
    usz  new_sz   = (usz)f->arg3;
    s32  flags    = (s32)f->arg4;
    uptr new_addr = (uptr)f->arg5;

    if (!old_sz || !new_sz) return KPZ_ERR(KPZE_INVAL);

    old_sz  = (old_sz  + 0xFFFULL) & ~0xFFFULL;
    new_sz  = (new_sz  + 0xFFFULL) & ~0xFFFULL;

    if (new_sz <= old_sz) {
        if (new_sz < old_sz)
            kobalt_vmm_free(old_addr + new_sz, old_sz - new_sz);
        return (s64)old_addr;
    }

    uptr hint = (flags & KPZ_MREMAP_MAYMOVE) ? 0 : old_addr;
    if (flags & 2 ) hint = new_addr;

    uptr mapped = kobalt_vmm_alloc(hint, new_sz, KPZ_PROT_READ | KPZ_PROT_WRITE);
    if (!mapped) return (s64)(uptr)KPZ_MAP_FAILED;

    kpz_memcpy((void *)mapped, (void *)old_addr, old_sz);
    kpz_memzero((void *)(mapped + old_sz), new_sz - old_sz);
    kobalt_vmm_free(old_addr, old_sz);
    return (s64)mapped;
}

s64 kpz_sys_mlock(kpz_frame_t *f)
{
    (void)f;
    return 0;
}

s64 kpz_sys_munlock(kpz_frame_t *f)
{
    (void)f;
    return 0;
}

s64 kpz_sys_seccomp(kpz_frame_t *f)
{
    (void)f;
    return KPZ_ERR(KPZE_INVAL);
}

s64 kpz_sys_bpf(kpz_frame_t *f)
{
    (void)f;
    return KPZ_ERR(KPZE_NOSYS);
}

s64 kpz_sys_statx(kpz_frame_t *f)
{
    s32           dirfd  = (s32)f->arg1;
    const char   *path   = (const char *)f->arg2;
    u32           mask   = (u32)f->arg4;
    kpz_statx_t  *buf    = (kpz_statx_t *)f->arg5;
    (void)f->arg3;

    if (dirfd != KPZ_AT_FDCWD) return KPZ_ERR(KPZE_NOSYS);
    if (!path || kpz_check_ptr(buf, sizeof(*buf))) return KPZ_ERR(KPZE_FAULT);

    vfs_stat_t st;
    if (vfs_stat(path, &st) < 0) return KPZ_ERR(KPZE_NOENT);

    kpz_memzero(buf, sizeof(*buf));
    buf->stx_mask      = mask & KPZ_STATX_BASIC_STATS;
    buf->stx_blksize   = 4096;
    buf->stx_nlink     = st.nlink ? st.nlink : 1;
    buf->stx_uid       = 0;
    buf->stx_gid       = 0;
    buf->stx_mode      = (u16)(st.mode & 0xFFFF);
    buf->stx_ino       = st.ino;
    buf->stx_size      = st.size;
    buf->stx_blocks    = (st.size + 511) / 512;
    buf->stx_mtime.tv_sec = (s64)st.mtime;
    buf->stx_ctime.tv_sec = (s64)st.ctime;
    buf->stx_dev_major = 0;
    buf->stx_dev_minor = 1;
    return 0;
}

typedef struct {
    u8            *data;
    usz            size;
    usz            cap;
    kpz_spinlock_t lk;
} kpz_memfd_t;

static s64 memfd_read(kposixz_file_t *f, void *buf, u64 len)
{
    kpz_memfd_t *m = (kpz_memfd_t *)f->priv;
    kpz_spin_lock(&m->lk);
    if (f->pos >= (kpz_off_t)m->size) { kpz_spin_unlock(&m->lk); return 0; }
    usz avail = m->size - (usz)f->pos;
    usz n = len < avail ? (usz)len : avail;
    kpz_memcpy(buf, m->data + f->pos, n);
    f->pos += (kpz_off_t)n;
    kpz_spin_unlock(&m->lk);
    return (s64)n;
}

static s64 memfd_write(kposixz_file_t *f, const void *buf, u64 len)
{
    kpz_memfd_t *m = (kpz_memfd_t *)f->priv;
    kpz_spin_lock(&m->lk);
    usz end = (usz)f->pos + (usz)len;
    if (end > m->cap) {
        usz newcap = end + 4096;
        u8 *nb = (u8 *)kmalloc(newcap);
        if (!nb) { kpz_spin_unlock(&m->lk); return KPZ_ERR(KPZE_NOMEM); }
        if (m->data) { kpz_memcpy(nb, m->data, m->size); kfree(m->data); }
        kpz_memzero(nb + m->size, newcap - m->size);
        m->data = nb; m->cap = newcap;
    }
    kpz_memcpy(m->data + f->pos, buf, (usz)len);
    if (end > m->size) m->size = end;
    f->pos += (kpz_off_t)len;
    kpz_spin_unlock(&m->lk);
    return (s64)len;
}

static s64 memfd_seek(kposixz_file_t *f, s64 off, u32 whence)
{
    kpz_memfd_t *m = (kpz_memfd_t *)f->priv;
    kpz_off_t np;
    switch (whence) {
    case KPZ_SEEK_SET: np = off; break;
    case KPZ_SEEK_CUR: np = f->pos + off; break;
    case KPZ_SEEK_END: np = (kpz_off_t)m->size + off; break;
    default: return KPZ_ERR(KPZE_INVAL);
    }
    if (np < 0) return KPZ_ERR(KPZE_INVAL);
    f->pos = np;
    return np;
}

static s64 memfd_stat(kposixz_file_t *f, kpz_stat_t *st)
{
    kpz_memfd_t *m = (kpz_memfd_t *)f->priv;
    kpz_memzero(st, sizeof(*st));
    st->st_mode    = KPZ_S_IFREG | 0600;
    st->st_nlink   = 1;
    st->st_size    = (kpz_off_t)m->size;
    st->st_blksize = 4096;
    st->st_blocks  = (st->st_size + 511) / 512;
    return 0;
}

static void memfd_close(kposixz_file_t *f)
{
    kpz_memfd_t *m = (kpz_memfd_t *)f->priv;
    if (m->data) kfree(m->data);
    kfree(m);
    f->priv = (void *)0;
}

static const kpz_vfs_ops_t memfd_ops = {
    .read  = memfd_read,
    .write = memfd_write,
    .seek  = memfd_seek,
    .stat  = memfd_stat,
    .close = memfd_close,
};

s64 kpz_sys_memfd_create(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);

    u32 flags = (u32)f->arg2;

    kpz_memfd_t *m = (kpz_memfd_t *)kmalloc(sizeof(kpz_memfd_t));
    if (!m) return KPZ_ERR(KPZE_NOMEM);
    kpz_memzero(m, sizeof(*m));

    kposixz_file_t *mf = (kposixz_file_t *)kmalloc(sizeof(kposixz_file_t));
    if (!mf) { kfree(m); return KPZ_ERR(KPZE_NOMEM); }
    kpz_memzero(mf, sizeof(*mf));
    mf->ops      = &memfd_ops;
    mf->priv     = m;
    mf->flags    = KPZ_O_RDWR;
    mf->mode     = KPZ_S_IFREG | 0600;
    mf->refcount = 1;

    s32 fd = kpz_fd_alloc(proc, mf);
    if (fd < 0) { kpz_fd_put(mf); return KPZ_ERR(KPZE_MFILE); }
    if (flags & KPZ_MFD_CLOEXEC) proc->fds.cloexec[fd] = 1;
    return (s64)fd;
}

s64 kpz_sys_membarrier(kpz_frame_t *f)
{
    s32 cmd = (s32)f->arg1;
    switch (cmd) {
    case KPZ_MEMBARRIER_CMD_QUERY:
        return KPZ_MEMBARRIER_CMD_GLOBAL |
               KPZ_MEMBARRIER_CMD_PRIVATE_EXPEDITED |
               KPZ_MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED;
    case KPZ_MEMBARRIER_CMD_GLOBAL:
    case KPZ_MEMBARRIER_CMD_PRIVATE_EXPEDITED:
    case KPZ_MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED:
        __asm__ volatile("mfence" ::: "memory");
        return 0;
    default:
        return KPZ_ERR(KPZE_INVAL);
    }
}

s64 kpz_sys_perf_event_open(kpz_frame_t *f)
{
    (void)f;
    return KPZ_ERR(KPZE_NOSYS);
}

s64 kpz_sys_sched_setaffinity(kpz_frame_t *f)
{
    (void)f;
    return 0;
}

s64 kpz_sys_sched_getaffinity(kpz_frame_t *f)
{
    void *mask = (void *)f->arg3;
    usz   sz   = (usz)f->arg2;
    if (!mask || !sz) return KPZ_ERR(KPZE_FAULT);
    if (kpz_check_ptr(mask, sz)) return KPZ_ERR(KPZE_FAULT);
    kpz_memzero(mask, sz);
    *(u8 *)mask = 1;
    return 0;
}

s64 kpz_sys_sched_setattr(kpz_frame_t *f)
{
    (void)f;
    return 0;
}

s64 kpz_sys_sched_getattr(kpz_frame_t *f)
{
    kpz_sched_attr_t *attr = (kpz_sched_attr_t *)f->arg2;
    u32 size = (u32)f->arg3;
    if (!attr || kpz_check_ptr(attr, sizeof(*attr))) return KPZ_ERR(KPZE_FAULT);
    usz fill = size < sizeof(*attr) ? (usz)size : sizeof(*attr);
    kpz_memzero(attr, fill);
    attr->size          = (u32)sizeof(*attr);
    attr->sched_policy  = 0;
    return 0;
}

s64 kpz_sys_getcpu(kpz_frame_t *f)
{
    u32 *cpu  = (u32 *)f->arg1;
    u32 *node = (u32 *)f->arg2;
    if (cpu  && !kpz_check_ptr(cpu,  4)) *cpu  = 0;
    if (node && !kpz_check_ptr(node, 4)) *node = 0;
    return 0;
}
