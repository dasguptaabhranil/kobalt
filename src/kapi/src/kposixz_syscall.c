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
#include <kposixz_amx.h>
#include <vfs.h>

kposixz_proc_t  *kpz_proc_table[KPOSIXZ_MAX_PROCS];
kpz_spinlock_t   kpz_proc_table_lock = KPZ_SPINLOCK_INIT;
kpz_percpu_t     kpz_bsp_percpu      KPZ_ALIGNED(64);

typedef s64 (*kpz_syscall_fn_t)(kpz_frame_t *);
static s64 sys_nosys(kpz_frame_t *f);

static s64 sys_read(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);
    kpz_fd_t fd  = (kpz_fd_t)f->arg1;
    void    *buf = (void *)f->arg2;
    u64      len = f->arg3;
    if (kpz_check_ptr(buf, len)) return KPZ_ERR(KPZE_FAULT);
    kposixz_file_t *file = kpz_fd_get(proc, fd);
    if (!file) return KPZ_ERR(KPZE_BADF);
    s64 ret;
    if (!file->ops->read) { ret = KPZ_ERR(KPZE_BADF); }
    else {
        kpz_spin_lock(&file->lock);
        ret = file->ops->read(file, buf, len);
        kpz_spin_unlock(&file->lock);
    }
    kpz_fd_put(file);
    return ret;
}

static s64 sys_write(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);
    kpz_fd_t    fd  = (kpz_fd_t)f->arg1;
    const void *buf = (const void *)f->arg2;
    u64         len = f->arg3;
    if (kpz_check_ptr(buf, len)) return KPZ_ERR(KPZE_FAULT);
    kposixz_file_t *file = kpz_fd_get(proc, fd);
    if (!file) return KPZ_ERR(KPZE_BADF);
    s64 ret;
    if (!file->ops->write) { ret = KPZ_ERR(KPZE_BADF); }
    else {
        kpz_spin_lock(&file->lock);
        ret = file->ops->write(file, buf, len);
        kpz_spin_unlock(&file->lock);
    }
    kpz_fd_put(file);
    return ret;
}

static s64 sys_open(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);
    const char *path  = (const char *)f->arg1;
    u32         flags = (u32)f->arg2;
    if (!path) return KPZ_ERR(KPZE_FAULT);
    if (flags & (KPZ_O_WRONLY | KPZ_O_RDWR | KPZ_O_CREAT))
        return KPZ_ERR(KPZE_ROFS);
    kposixz_file_t *file = kpz_kfs_open(path, flags);
    if (!file) return KPZ_ERR(KPZE_NOENT);
    s32 fd = kpz_fd_alloc(proc, file);
    if (fd < 0) { kpz_fd_put(file); return KPZ_ERR(KPZE_MFILE); }
    if (flags & KPZ_O_CLOEXEC) proc->fds.cloexec[fd] = 1;
    return (s64)fd;
}

static s64 sys_openat(kpz_frame_t *f)
{
    s32 dirfd = (s32)f->arg1;
    if (dirfd != KPZ_AT_FDCWD) return KPZ_ERR(KPZE_NOSYS);
    kpz_frame_t tmp = *f;
    tmp.arg1 = f->arg2; tmp.arg2 = f->arg3; tmp.arg3 = f->arg4;
    return sys_open(&tmp);
}

static s64 sys_close(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);
    s32 rc = kpz_fd_close(proc, (kpz_fd_t)f->arg1);
    return rc < 0 ? KPZ_ERR(KPZE_BADF) : 0;
}

static s64 sys_stat(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);
    const char *path = (const char *)f->arg1;
    kpz_stat_t *st   = (kpz_stat_t *)f->arg2;
    if (kpz_check_ptr(st, sizeof(*st))) return KPZ_ERR(KPZE_FAULT);
    kposixz_file_t *file = kpz_kfs_open(path, KPZ_O_RDONLY);
    if (!file) return KPZ_ERR(KPZE_NOENT);
    s64 ret = file->ops->stat(file, st);
    kpz_fd_put(file);
    return ret;
}

static s64 sys_fstat(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);
    kpz_fd_t    fd = (kpz_fd_t)f->arg1;
    kpz_stat_t *st = (kpz_stat_t *)f->arg2;
    if (kpz_check_ptr(st, sizeof(*st))) return KPZ_ERR(KPZE_FAULT);
    kposixz_file_t *file = kpz_fd_get(proc, fd);
    if (!file) return KPZ_ERR(KPZE_BADF);
    s64 ret = file->ops->stat ? file->ops->stat(file, st) : KPZ_ERR(KPZE_BADF);
    kpz_fd_put(file);
    return ret;
}

static s64 sys_lstat(kpz_frame_t *f) { return sys_stat(f); }

static s64 sys_lseek(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);
    kpz_fd_t fd     = (kpz_fd_t)f->arg1;
    s64      offset = (s64)f->arg2;
    u32      whence = (u32)f->arg3;
    kposixz_file_t *file = kpz_fd_get(proc, fd);
    if (!file) return KPZ_ERR(KPZE_BADF);
    s64 ret = file->ops->seek ? file->ops->seek(file, offset, whence)
                               : KPZ_ERR(KPZE_SPIPE);
    kpz_fd_put(file);
    return ret;
}

static s64 sys_access(kpz_frame_t *f)
{
    const char *path = (const char *)f->arg1;
    if (!path) return KPZ_ERR(KPZE_FAULT);
    kposixz_file_t *file = kpz_kfs_open(path, KPZ_O_RDONLY);
    if (!file) return KPZ_ERR(KPZE_NOENT);
    kpz_fd_put(file);
    return 0;
}

static s64 sys_ioctl(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);
    kpz_fd_t fd  = (kpz_fd_t)f->arg1;
    u64      req = f->arg2;
    u64      arg = f->arg3;
    kposixz_file_t *file = kpz_fd_get(proc, fd);
    if (!file) return KPZ_ERR(KPZE_BADF);
    s64 ret = file->ops->ioctl ? file->ops->ioctl(file, req, arg)
                                : KPZ_ERR(KPZE_NOTTY);
    kpz_fd_put(file);
    return ret;
}

static s64 sys_readv(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);
    kpz_fd_t           fd  = (kpz_fd_t)f->arg1;
    const kpz_iovec_t *iov = (const kpz_iovec_t *)f->arg2;
    s32                cnt = (s32)f->arg3;
    if (cnt < 0 || cnt > 1024) return KPZ_ERR(KPZE_INVAL);
    if (kpz_check_ptr(iov, (usz)cnt * sizeof(*iov))) return KPZ_ERR(KPZE_FAULT);
    kposixz_file_t *file = kpz_fd_get(proc, fd);
    if (!file || !file->ops->read) { kpz_fd_put(file); return KPZ_ERR(KPZE_BADF); }
    s64 total = 0;
    kpz_spin_lock(&file->lock);
    for (s32 i = 0; i < cnt; i++) {
        if (!iov[i].iov_len) continue;
        if (kpz_check_ptr(iov[i].iov_base, iov[i].iov_len)) {
            total = KPZ_ERR(KPZE_FAULT); break;
        }
        s64 r = file->ops->read(file, iov[i].iov_base, iov[i].iov_len);
        if (r < 0) { if (!total) total = r; break; }
        total += r;
        if ((usz)r < iov[i].iov_len) break;
    }
    kpz_spin_unlock(&file->lock);
    kpz_fd_put(file);
    return total;
}

static s64 sys_writev(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);
    kpz_fd_t           fd  = (kpz_fd_t)f->arg1;
    const kpz_iovec_t *iov = (const kpz_iovec_t *)f->arg2;
    s32                cnt = (s32)f->arg3;
    if (cnt < 0 || cnt > 1024) return KPZ_ERR(KPZE_INVAL);
    if (kpz_check_ptr(iov, (usz)cnt * sizeof(*iov))) return KPZ_ERR(KPZE_FAULT);
    kposixz_file_t *file = kpz_fd_get(proc, fd);
    if (!file || !file->ops->write) { kpz_fd_put(file); return KPZ_ERR(KPZE_BADF); }
    s64 total = 0;
    kpz_spin_lock(&file->lock);
    for (s32 i = 0; i < cnt; i++) {
        if (!iov[i].iov_len) continue;
        if (kpz_check_ptr(iov[i].iov_base, iov[i].iov_len)) {
            total = KPZ_ERR(KPZE_FAULT); break;
        }
        s64 r = file->ops->write(file, iov[i].iov_base, iov[i].iov_len);
        if (r < 0) { if (!total) total = r; break; }
        total += r;
        if ((usz)r < iov[i].iov_len) break;
    }
    kpz_spin_unlock(&file->lock);
    kpz_fd_put(file);
    return total;
}

static s64 sys_dup(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);
    s32 rc = kpz_fd_dup(proc, (kpz_fd_t)f->arg1);
    return rc < 0 ? KPZ_ERR(-rc) : (s64)rc;
}

static s64 sys_dup2(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);
    s32 rc = kpz_fd_dup2(proc, (kpz_fd_t)f->arg1, (kpz_fd_t)f->arg2);
    return rc < 0 ? KPZ_ERR(-rc) : (s64)rc;
}

static s64 sys_fcntl(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);
    kpz_fd_t fd  = (kpz_fd_t)f->arg1;
    s32      cmd = (s32)f->arg2;
    u64      arg = f->arg3;
    if (fd < 0 || fd >= KPOSIXZ_MAX_FD) return KPZ_ERR(KPZE_BADF);
    switch (cmd) {
    case KPZ_F_DUPFD: {
        kposixz_file_t *file = kpz_fd_get(proc, fd);
        if (!file) return KPZ_ERR(KPZE_BADF);
        kpz_spin_lock(&proc->fds.lock);
        s32 nfd = -1;
        for (s32 i = (s32)arg; i < KPOSIXZ_MAX_FD; i++) {
            if (!proc->fds.files[i]) { proc->fds.files[i] = file; nfd = i; break; }
        }
        kpz_spin_unlock(&proc->fds.lock);
        if (nfd < 0) { kpz_fd_put(file); return KPZ_ERR(KPZE_MFILE); }
        return (s64)nfd;
    }
    case KPZ_F_GETFD: return proc->fds.cloexec[fd] ? KPZ_FD_CLOEXEC : 0;
    case KPZ_F_SETFD: proc->fds.cloexec[fd] = (arg & KPZ_FD_CLOEXEC) ? 1 : 0; return 0;
    case KPZ_F_GETFL: {
        kposixz_file_t *file = kpz_fd_get(proc, fd);
        if (!file) return KPZ_ERR(KPZE_BADF);
        s64 flags = (s64)file->flags;
        kpz_fd_put(file);
        return flags;
    }
    case KPZ_F_SETFL: {
        kposixz_file_t *file = kpz_fd_get(proc, fd);
        if (!file) return KPZ_ERR(KPZE_BADF);
        file->flags = (u32)arg;
        kpz_fd_put(file);
        return 0;
    }
    default: return KPZ_ERR(KPZE_INVAL);
    }
}

static s64 sys_mmap(kpz_frame_t *f)
{
    uptr addr  = (uptr)f->arg1;
    usz  len   = (usz)f->arg2;
    u32  prot  = (u32)f->arg3;
    u32  flags = (u32)f->arg4;
    if (!len) return KPZ_ERR(KPZE_INVAL);
    len = (len + 0xFFFULL) & ~0xFFFULL;
    if (!(flags & KPZ_MAP_ANON)) return KPZ_ERR(KPZE_NOSYS);
    uptr mapped = kobalt_vmm_alloc(addr, len, prot);
    if (!mapped) return (s64)(uptr)KPZ_MAP_FAILED;
    kpz_memzero((void *)mapped, len);
    return (s64)mapped;
}

static s64 sys_mprotect(kpz_frame_t *f)
{
    uptr addr = (uptr)f->arg1;
    usz  len  = (usz)f->arg2;
    u32  prot = (u32)f->arg3;
    s32  rc   = kobalt_vmm_protect(addr, len, prot);
    return rc < 0 ? KPZ_ERR(KPZE_INVAL) : 0;
}

static s64 sys_munmap(kpz_frame_t *f)
{
    kobalt_vmm_free((uptr)f->arg1, (usz)f->arg2);
    return 0;
}

static s64 sys_brk(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_NOMEM);
    uptr new_brk = (uptr)f->arg1;
    if (!new_brk) return (s64)proc->mm_brk;
    if (new_brk < proc->mm_brk_start) return (s64)proc->mm_brk;

    uptr old_page = (proc->mm_brk + 0xFFFULL) & ~0xFFFULL;
    uptr new_page = (new_brk     + 0xFFFULL) & ~0xFFFULL;

    if (new_page > old_page) {
        uptr m = kobalt_vmm_alloc(old_page, new_page - old_page,
                                   KPZ_PROT_READ | KPZ_PROT_WRITE);
        if (!m) return (s64)proc->mm_brk;
        kpz_memzero((void *)old_page, new_page - old_page);
    } else if (new_page < old_page) {
        kobalt_vmm_free(new_page, old_page - new_page);
    }

    proc->mm_brk = new_brk;
    return (s64)new_brk;
}

static s64 sys_getpid(kpz_frame_t *f)  { (void)f; kposixz_proc_t *p = kpz_current(); return p ? (s64)p->pid  : 0; }
static s64 sys_getuid(kpz_frame_t *f)  { (void)f; kposixz_proc_t *p = kpz_current(); return p ? (s64)p->uid  : 0; }
static s64 sys_getgid(kpz_frame_t *f)  { (void)f; kposixz_proc_t *p = kpz_current(); return p ? (s64)p->gid  : 0; }
static s64 sys_geteuid(kpz_frame_t *f) { return sys_getuid(f); }
static s64 sys_getegid(kpz_frame_t *f) { return sys_getgid(f); }
static s64 sys_getppid(kpz_frame_t *f) { (void)f; kposixz_proc_t *p = kpz_current(); return p ? (s64)p->ppid : 1; }
static s64 sys_gettid(kpz_frame_t *f)  { return sys_getpid(f); }

static s64 sys_setsid(kpz_frame_t *f)
{
    (void)f;
    kposixz_proc_t *p = kpz_current();
    if (!p) return KPZ_ERR(KPZE_INVAL);
    p->sid  = p->pid;
    p->pgid = p->pid;
    return (s64)p->sid;
}

typedef struct { u64 uptime; u64 loads[3]; u64 totalram; u64 freeram;
                 u64 sharedram; u64 bufferram; u64 totalswap; u64 freeswap;
                 u16 procs; u8 pad[22]; } kpz_sysinfo_t;

static s64 sys_sysinfo(kpz_frame_t *f)
{
    kpz_sysinfo_t *si = (kpz_sysinfo_t *)f->arg1;
    if (kpz_check_ptr(si, sizeof(*si))) return KPZ_ERR(KPZE_FAULT);
    kpz_memzero(si, sizeof(*si));
    si->uptime   = kobalt_acpi_timer_ns() / 1000000000ULL;
    si->totalram = 256ULL << 20;
    si->freeram  =  64ULL << 20;
    si->procs    = 1;
    return 0;
}

static s64 sys_arch_prctl(kpz_frame_t *f)
{
    s32 code = (s32)f->arg1;
    u64 addr = f->arg2;
    switch (code) {
    case KPZ_ARCH_SET_FS:
        kpz_wrmsr(KPZ_MSR_FSBASE, addr);
        { kposixz_proc_t *p = kpz_current(); if (p) p->fs_base = addr; }
        return 0;
    case KPZ_ARCH_GET_FS:
        if (kpz_check_ptr((void *)addr, 8)) return KPZ_ERR(KPZE_FAULT);
        *(u64 *)addr = kpz_rdmsr(KPZ_MSR_FSBASE);
        return 0;
    case KPZ_ARCH_SET_GS:
        kpz_wrmsr(KPZ_MSR_GSBASE, addr);
        { kposixz_proc_t *p = kpz_current(); if (p) p->gs_base = addr; }
        return 0;
    case KPZ_ARCH_GET_GS:
        if (kpz_check_ptr((void *)addr, 8)) return KPZ_ERR(KPZE_FAULT);
        *(u64 *)addr = kpz_rdmsr(KPZ_MSR_GSBASE);
        return 0;
    case KPZ_ARCH_REQ_XCOMP_PERM:
        return (s64)kposixz_amx(KPOSIXZ_AMX_PERM_REQUEST, addr);
    default: return KPZ_ERR(KPZE_INVAL);
    }
}

static s64 sys_set_tid_address(kpz_frame_t *f)
{
    (void)f;
    kposixz_proc_t *p = kpz_current();
    return p ? (s64)p->pid : 0;
}

static s64 sys_clock_gettime(kpz_frame_t *f)
{
    s32             clk = (s32)f->arg1;
    kpz_timespec_t *ts  = (kpz_timespec_t *)f->arg2;
    if (kpz_check_ptr(ts, sizeof(*ts))) return KPZ_ERR(KPZE_FAULT);
    switch (clk) {
    case KPZ_CLOCK_REALTIME:
    case KPZ_CLOCK_MONOTONIC:
    case KPZ_CLOCK_MONOTONIC_RAW:
    case KPZ_CLOCK_REALTIME_COARSE:
    case KPZ_CLOCK_MONOTONIC_COARSE:
        kpz_ns_to_timespec(kobalt_acpi_timer_ns(), ts);
        return 0;
    default: return KPZ_ERR(KPZE_INVAL);
    }
}

static s64 sys_gettimeofday(kpz_frame_t *f)
{
    kpz_timeval_t *tv = (kpz_timeval_t *)f->arg1;
    if (!tv) return 0;
    if (kpz_check_ptr(tv, sizeof(*tv))) return KPZ_ERR(KPZE_FAULT);
    u64 ns = kobalt_acpi_timer_ns();
    tv->tv_sec  = (kpz_time_t)(ns / 1000000000ULL);
    tv->tv_usec = (kpz_suseconds_t)((ns % 1000000000ULL) / 1000ULL);
    return 0;
}

static s64 sys_rt_sigaction(kpz_frame_t *f)
{
    s32                    sig  = (s32)f->arg1;
    const kpz_sigaction_t *act  = (const kpz_sigaction_t *)f->arg2;
    kpz_sigaction_t       *oact = (kpz_sigaction_t *)f->arg3;
    if (sig < 1 || sig > 31) return KPZ_ERR(KPZE_INVAL);
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_INVAL);
    if (oact) {
        if (kpz_check_ptr(oact, sizeof(*oact))) return KPZ_ERR(KPZE_FAULT);
        kpz_memcpy(oact, &proc->sigactions[sig - 1], sizeof(*oact));
    }
    if (act) {
        if (kpz_check_ptr(act, sizeof(*act))) return KPZ_ERR(KPZE_FAULT);
        kpz_memcpy(&proc->sigactions[sig - 1], act, sizeof(*act));
    }
    return 0;
}

static s64 sys_rt_sigprocmask(kpz_frame_t *f)
{
    s32   how    = (s32)f->arg1;
    u64  *set    = (u64 *)f->arg2;
    u64  *oldset = (u64 *)f->arg3;
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_INVAL);
    if (oldset) {
        if (kpz_check_ptr(oldset, 8)) return KPZ_ERR(KPZE_FAULT);
        *oldset = proc->sig_mask;
    }
    if (set) {
        if (kpz_check_ptr(set, 8)) return KPZ_ERR(KPZE_FAULT);
        switch (how) {
        case 0: proc->sig_mask |=  *set; break;
        case 1: proc->sig_mask &= ~*set; break;
        case 2: proc->sig_mask  =  *set; break;
        default: return KPZ_ERR(KPZE_INVAL);
        }
    }
    return 0;
}

static s64 sys_rt_sigreturn(kpz_frame_t *f) { (void)f; return KPZ_ERR(KPZE_NOSYS); }

static s64 sys_exit(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    s32 code = (s32)f->arg1;
    if (proc) kpz_proc_exit(proc, code);
    __asm__ volatile("cli\n1: hlt\n jmp 1b" ::: "memory");
    __builtin_unreachable();
}

static s64 sys_exit_group(kpz_frame_t *f) { return sys_exit(f); }

static s64 sys_fork(kpz_frame_t *f)
{
    (void)f;
    return KPZ_ERR(KPZE_AGAIN);
}

static s64 sys_clone(kpz_frame_t *f)
{
    (void)f;
    return KPZ_ERR(KPZE_NOSYS);
}

static s64 sys_execve(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_FAULT);
    const char *path = (const char *)f->arg1;
    if (!path) return KPZ_ERR(KPZE_FAULT);

    kpz_pid_t new_pid = kposixz_spawn(path, (void *)0, 0, (void *)0, (void *)0);
    if ((s64)new_pid < 0) return (s64)new_pid;

    kpz_proc_exit(proc, 0);
    __builtin_unreachable();
}

static s64 sys_wait4(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_CHILD);
    kpz_pid_t wpid  = (kpz_pid_t)f->arg1;
    s32      *wstat = (s32 *)f->arg2;

    kpz_spin_lock(&kpz_proc_table_lock);
    for (s32 i = 1; i < KPOSIXZ_MAX_PROCS; i++) {
        kposixz_proc_t *c = kpz_proc_table[i];
        if (!c) continue;
        if (c->ppid != proc->pid) continue;
        if (wpid > 0 && c->pid != wpid) continue;
        if (kpz_atomic_load(&c->state) != KPZ_PROC_ZOMBIE) continue;

        kpz_pid_t dead = c->pid;
        s32 code = c->exit_code;
        kpz_spin_unlock(&kpz_proc_table_lock);

        if (wstat && !kpz_check_ptr(wstat, 4))
            *wstat = (code & 0xff) << 8;

        kpz_proc_free(c);
        return (s64)dead;
    }
    kpz_spin_unlock(&kpz_proc_table_lock);
    return KPZ_ERR(KPZE_CHILD);
}

static s64 sys_kill(kpz_frame_t *f)
{
    kpz_pid_t pid = (kpz_pid_t)f->arg1;
    s32       sig = (s32)f->arg2;

    if (pid <= 0) return KPZ_ERR(KPZE_INVAL);

    kposixz_proc_t *target = kpz_proc_lookup(pid);
    if (!target) return KPZ_ERR(KPZE_NOENT);

    if (sig == 9 || sig == 15) {
        kpz_atomic_store(&target->state, (u8)KPZ_PROC_ZOMBIE);
        target->exit_code = -sig;
    } else if (sig > 0 && sig < 64) {
        __atomic_or_fetch(&target->sig_pending, 1ULL << (sig - 1), __ATOMIC_SEQ_CST);
    }
    return 0;
}

static s64 sys_tkill(kpz_frame_t *f)
{
    kpz_frame_t tmp = *f;
    return sys_kill(&tmp);
}

static s64 sys_tgkill(kpz_frame_t *f)
{
    kpz_frame_t tmp = *f;
    tmp.arg1 = f->arg2;
    tmp.arg2 = f->arg3;
    return sys_kill(&tmp);
}

static s64 sys_futex(kpz_frame_t *f)
{
    u32 *uaddr = (u32 *)f->arg1;
    s32  op    = (s32)f->arg2 & ~KPZ_FUTEX_PRIVATE;
    u32  val   = (u32)f->arg3;

    if (kpz_check_ptr(uaddr, 4)) return KPZ_ERR(KPZE_FAULT);

    switch (op) {
    case KPZ_FUTEX_WAIT:
        if (__atomic_load_n(uaddr, __ATOMIC_SEQ_CST) != val)
            return KPZ_ERR(KPZE_AGAIN);
        kobalt_sched_yield();
        return 0;
    case KPZ_FUTEX_WAKE:
        return 0;
    default:
        return KPZ_ERR(KPZE_NOSYS);
    }
}

static s64 sys_fsync(kpz_frame_t *f)
{
    (void)f;
    return 0;
}

static s64 sys_fdatasync(kpz_frame_t *f)
{
    (void)f;
    return 0;
}

static s64 sys_truncate(kpz_frame_t *f)
{
    const char *path = (const char *)f->arg1;
    u64         sz   = (u64)f->arg2;
    if (!path) return KPZ_ERR(KPZE_FAULT);
    int rc = vfs_path_truncate(path, sz);
    return rc < 0 ? KPZ_ERR(KPZE_IO) : 0;
}

static s64 sys_ftruncate(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);
    kpz_fd_t fd = (kpz_fd_t)f->arg1;
    u64      sz = (u64)f->arg2;

    kposixz_file_t *file = kpz_fd_get(proc, fd);
    if (!file) return KPZ_ERR(KPZE_BADF);

    kpz_kfs_priv_t *priv = (kpz_kfs_priv_t *)file->priv;
    s64 ret = 0;
    if (!priv || priv->is_dir) {
        ret = KPZ_ERR(KPZE_INVAL);
    } else {
        int rc = vfs_truncate(priv->vfs_fd, sz);
        if (rc < 0) ret = KPZ_ERR(KPZE_IO);
        else priv->size = (usz)sz;
    }
    kpz_fd_put(file);
    return ret;
}

static s64 sys_getdents_common(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);
    kpz_fd_t fd  = (kpz_fd_t)f->arg1;
    void    *buf = (void *)f->arg2;
    u64      len = f->arg3;
    if (kpz_check_ptr(buf, len)) return KPZ_ERR(KPZE_FAULT);
    kposixz_file_t *file = kpz_fd_get(proc, fd);
    if (!file) return KPZ_ERR(KPZE_BADF);
    s64 ret = file->ops->getdents ? file->ops->getdents(file, buf, len)
                                  : KPZ_ERR(KPZE_NOTDIR);
    kpz_fd_put(file);
    return ret;
}

static s64 sys_getdents(kpz_frame_t *f)   { return sys_getdents_common(f); }
static s64 sys_getdents64(kpz_frame_t *f) { return sys_getdents_common(f); }

static s64 sys_getcwd(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);
    char *buf = (char *)f->arg1;
    usz   sz  = (usz)f->arg2;
    if (!buf || !sz) return KPZ_ERR(KPZE_FAULT);
    if (kpz_check_ptr(buf, sz)) return KPZ_ERR(KPZE_FAULT);
    usz cwdlen = kpz_strlen(proc->cwd) + 1;
    if (cwdlen > sz) return KPZ_ERR(KPZE_RANGE);
    kpz_strncpy(buf, proc->cwd, sz);
    return (s64)(uptr)buf;
}

static s64 sys_chdir(kpz_frame_t *f)
{
    kposixz_proc_t *proc = kpz_current();
    if (!proc) return KPZ_ERR(KPZE_BADF);
    const char *path = (const char *)f->arg1;
    if (!path) return KPZ_ERR(KPZE_FAULT);
    vfs_stat_t st;
    if (vfs_stat(path, &st) < 0) return KPZ_ERR(KPZE_NOENT);
    if ((st.mode & 0170000) != 0040000) return KPZ_ERR(KPZE_NOTDIR);
    kpz_strncpy(proc->cwd, path, sizeof(proc->cwd));
    return 0;
}

static s64 sys_rename(kpz_frame_t *f)
{
    const char *old = (const char *)f->arg1;
    const char *new = (const char *)f->arg2;
    if (!old || !new) return KPZ_ERR(KPZE_FAULT);
    int rc = vfs_rename(old, new);
    return rc < 0 ? KPZ_ERR(-rc) : 0;
}

static s64 sys_mkdir(kpz_frame_t *f)
{
    const char *path = (const char *)f->arg1;
    u32 mode = (u32)f->arg2;
    if (!path) return KPZ_ERR(KPZE_FAULT);
    int rc = vfs_mkdir(path, mode);
    return rc < 0 ? KPZ_ERR(KPZE_IO) : 0;
}

static s64 sys_rmdir(kpz_frame_t *f)
{
    const char *path = (const char *)f->arg1;
    if (!path) return KPZ_ERR(KPZE_FAULT);
    int rc = vfs_rmdir(path);
    return rc < 0 ? KPZ_ERR(KPZE_IO) : 0;
}

static s64 sys_unlink(kpz_frame_t *f)
{
    const char *path = (const char *)f->arg1;
    if (!path) return KPZ_ERR(KPZE_FAULT);
    int rc = vfs_unlink(path);
    return rc < 0 ? KPZ_ERR(KPZE_NOENT) : 0;
}

static s64 sys_symlink(kpz_frame_t *f)
{
    const char *target = (const char *)f->arg1;
    const char *link   = (const char *)f->arg2;
    if (!target || !link) return KPZ_ERR(KPZE_FAULT);
    int rc = vfs_symlink(target, link);
    return rc < 0 ? KPZ_ERR(KPZE_IO) : 0;
}

static s64 sys_readlink(kpz_frame_t *f)
{
    const char *path = (const char *)f->arg1;
    char       *buf  = (char *)f->arg2;
    usz         sz   = (usz)f->arg3;
    if (!path || kpz_check_ptr(buf, sz)) return KPZ_ERR(KPZE_FAULT);
    int rc = vfs_readlink(path, buf, sz);
    return rc < 0 ? KPZ_ERR(KPZE_INVAL) : (s64)rc;
}

static s64 sys_mkdirat(kpz_frame_t *f)
{
    if ((s32)f->arg1 != KPZ_AT_FDCWD) return KPZ_ERR(KPZE_NOSYS);
    kpz_frame_t tmp = *f;
    tmp.arg1 = f->arg2; tmp.arg2 = f->arg3;
    return sys_mkdir(&tmp);
}

static s64 sys_unlinkat(kpz_frame_t *f)
{
    if ((s32)f->arg1 != KPZ_AT_FDCWD) return KPZ_ERR(KPZE_NOSYS);
    kpz_frame_t tmp = *f;
    tmp.arg1 = f->arg2;
    u32 flags = (u32)f->arg3;
    if (flags & 0x200 ) return sys_rmdir(&tmp);
    return sys_unlink(&tmp);
}

static s64 sys_renameat(kpz_frame_t *f)
{
    if ((s32)f->arg1 != KPZ_AT_FDCWD || (s32)f->arg3 != KPZ_AT_FDCWD)
        return KPZ_ERR(KPZE_NOSYS);
    kpz_frame_t tmp = *f;
    tmp.arg1 = f->arg2; tmp.arg2 = f->arg4;
    return sys_rename(&tmp);
}

static s64 sys_newfstatat(kpz_frame_t *f)
{
    if ((s32)f->arg1 != KPZ_AT_FDCWD) return KPZ_ERR(KPZE_NOSYS);
    kpz_frame_t tmp = *f;
    tmp.arg1 = f->arg2; tmp.arg2 = f->arg3;
    return sys_stat(&tmp);
}

static s64 sys_readlinkat(kpz_frame_t *f)
{
    if ((s32)f->arg1 != KPZ_AT_FDCWD) return KPZ_ERR(KPZE_NOSYS);
    kpz_frame_t tmp = *f;
    tmp.arg1 = f->arg2; tmp.arg2 = f->arg3; tmp.arg3 = f->arg4;
    return sys_readlink(&tmp);
}

static s64 sys_nosys(kpz_frame_t *f)
{
    (void)f;
    return KPZ_ERR(KPZE_NOSYS);
}

extern s64 kpz_sys_socket(kpz_frame_t *f);
extern s64 kpz_sys_bind(kpz_frame_t *f);
extern s64 kpz_sys_connect(kpz_frame_t *f);
extern s64 kpz_sys_listen(kpz_frame_t *f);
extern s64 kpz_sys_accept(kpz_frame_t *f);
extern s64 kpz_sys_accept4(kpz_frame_t *f);
extern s64 kpz_sys_sendto(kpz_frame_t *f);
extern s64 kpz_sys_recvfrom(kpz_frame_t *f);
extern s64 kpz_sys_shutdown(kpz_frame_t *f);
extern s64 kpz_sys_getsockname(kpz_frame_t *f);
extern s64 kpz_sys_getpeername(kpz_frame_t *f);
extern s64 kpz_sys_setsockopt(kpz_frame_t *f);
extern s64 kpz_sys_getsockopt(kpz_frame_t *f);

extern s64 kpz_sys_pipe(kpz_frame_t *f);
extern s64 kpz_sys_poll(kpz_frame_t *f);
extern s64 kpz_sys_select(kpz_frame_t *f);
extern s64 kpz_sys_epoll_create1(kpz_frame_t *f);
extern s64 kpz_sys_epoll_ctl(kpz_frame_t *f);
extern s64 kpz_sys_epoll_wait(kpz_frame_t *f);
extern s64 kpz_sys_timerfd_create(kpz_frame_t *f);
extern s64 kpz_sys_eventfd(kpz_frame_t *f);

extern s64 kpz_sys_getrandom(kpz_frame_t *f);
extern s64 kpz_sys_prctl(kpz_frame_t *f);
extern s64 kpz_sys_getrlimit(kpz_frame_t *f);
extern s64 kpz_sys_setrlimit(kpz_frame_t *f);
extern s64 kpz_sys_prlimit64(kpz_frame_t *f);
extern s64 kpz_sys_mremap(kpz_frame_t *f);
extern s64 kpz_sys_mlock(kpz_frame_t *f);
extern s64 kpz_sys_munlock(kpz_frame_t *f);
extern s64 kpz_sys_seccomp(kpz_frame_t *f);
extern s64 kpz_sys_bpf(kpz_frame_t *f);
extern s64 kpz_sys_statx(kpz_frame_t *f);
extern s64 kpz_sys_memfd_create(kpz_frame_t *f);
extern s64 kpz_sys_membarrier(kpz_frame_t *f);
extern s64 kpz_sys_perf_event_open(kpz_frame_t *f);
extern s64 kpz_sys_sched_setaffinity(kpz_frame_t *f);
extern s64 kpz_sys_sched_getaffinity(kpz_frame_t *f);
extern s64 kpz_sys_sched_setattr(kpz_frame_t *f);
extern s64 kpz_sys_sched_getattr(kpz_frame_t *f);
extern s64 kpz_sys_getcpu(kpz_frame_t *f);

extern s64 kpz_sys_amx(kpz_frame_t *f);
extern long kposixz_amx(unsigned long op, unsigned long arg);

static const kpz_syscall_fn_t kpz_syscall_table[KPZ_SYS_NR_MAX] = {
    [KPZ_SYS_read]              = sys_read,
    [KPZ_SYS_write]             = sys_write,
    [KPZ_SYS_open]              = sys_open,
    [KPZ_SYS_close]             = sys_close,
    [KPZ_SYS_stat]              = sys_stat,
    [KPZ_SYS_fstat]             = sys_fstat,
    [KPZ_SYS_lstat]             = sys_lstat,
    [KPZ_SYS_poll]              = kpz_sys_poll,
    [KPZ_SYS_lseek]             = sys_lseek,
    [KPZ_SYS_mmap]              = sys_mmap,
    [KPZ_SYS_mprotect]          = sys_mprotect,
    [KPZ_SYS_munmap]            = sys_munmap,
    [KPZ_SYS_brk]               = sys_brk,
    [KPZ_SYS_rt_sigaction]      = sys_rt_sigaction,
    [KPZ_SYS_rt_sigprocmask]    = sys_rt_sigprocmask,
    [KPZ_SYS_rt_sigreturn]      = sys_rt_sigreturn,
    [KPZ_SYS_ioctl]             = sys_ioctl,
    [KPZ_SYS_readv]             = sys_readv,
    [KPZ_SYS_writev]            = sys_writev,
    [KPZ_SYS_access]            = sys_access,
    [KPZ_SYS_pipe]              = kpz_sys_pipe,
    [KPZ_SYS_select]            = kpz_sys_select,
    [KPZ_SYS_mremap]            = kpz_sys_mremap,
    [KPZ_SYS_dup]               = sys_dup,
    [KPZ_SYS_dup2]              = sys_dup2,
    [KPZ_SYS_getpid]            = sys_getpid,
    [KPZ_SYS_socket]            = kpz_sys_socket,
    [KPZ_SYS_connect]           = kpz_sys_connect,
    [KPZ_SYS_accept]            = kpz_sys_accept,
    [KPZ_SYS_sendto]            = kpz_sys_sendto,
    [KPZ_SYS_recvfrom]          = kpz_sys_recvfrom,
    [KPZ_SYS_shutdown]          = kpz_sys_shutdown,
    [KPZ_SYS_bind]              = kpz_sys_bind,
    [KPZ_SYS_listen]            = kpz_sys_listen,
    [KPZ_SYS_getsockname]       = kpz_sys_getsockname,
    [KPZ_SYS_getpeername]       = kpz_sys_getpeername,
    [KPZ_SYS_setsockopt]        = kpz_sys_setsockopt,
    [KPZ_SYS_getsockopt]        = kpz_sys_getsockopt,
    [KPZ_SYS_clone]             = sys_clone,
    [KPZ_SYS_fork]              = sys_fork,
    [KPZ_SYS_execve]            = sys_execve,
    [KPZ_SYS_exit]              = sys_exit,
    [KPZ_SYS_wait4]             = sys_wait4,
    [KPZ_SYS_kill]              = sys_kill,
    [KPZ_SYS_fcntl]             = sys_fcntl,
    [KPZ_SYS_fsync]             = sys_fsync,
    [KPZ_SYS_fdatasync]         = sys_fdatasync,
    [KPZ_SYS_truncate]          = sys_truncate,
    [KPZ_SYS_ftruncate]         = sys_ftruncate,
    [KPZ_SYS_getdents]          = sys_getdents,
    [KPZ_SYS_getcwd]            = sys_getcwd,
    [KPZ_SYS_chdir]             = sys_chdir,
    [KPZ_SYS_rename]            = sys_rename,
    [KPZ_SYS_mkdir]             = sys_mkdir,
    [KPZ_SYS_rmdir]             = sys_rmdir,
    [KPZ_SYS_unlink]            = sys_unlink,
    [KPZ_SYS_symlink]           = sys_symlink,
    [KPZ_SYS_readlink]          = sys_readlink,
    [KPZ_SYS_gettimeofday]      = sys_gettimeofday,
    [KPZ_SYS_getrlimit]         = kpz_sys_getrlimit,
    [KPZ_SYS_sysinfo]           = sys_sysinfo,
    [KPZ_SYS_getuid]            = sys_getuid,
    [KPZ_SYS_getgid]            = sys_getgid,
    [KPZ_SYS_geteuid]           = sys_geteuid,
    [KPZ_SYS_getegid]           = sys_getegid,
    [KPZ_SYS_getppid]           = sys_getppid,
    [KPZ_SYS_setsid]            = sys_setsid,
    [KPZ_SYS_mlock]             = kpz_sys_mlock,
    [KPZ_SYS_munlock]           = kpz_sys_munlock,
    [KPZ_SYS_prctl]             = kpz_sys_prctl,
    [KPZ_SYS_arch_prctl]        = sys_arch_prctl,
    [KPZ_SYS_setrlimit]         = kpz_sys_setrlimit,
    [KPZ_SYS_gettid]            = sys_gettid,
    [KPZ_SYS_tkill]             = sys_tkill,
    [KPZ_SYS_futex]             = sys_futex,
    [KPZ_SYS_sched_setaffinity] = kpz_sys_sched_setaffinity,
    [KPZ_SYS_sched_getaffinity] = kpz_sys_sched_getaffinity,
    [KPZ_SYS_getdents64]        = sys_getdents64,
    [KPZ_SYS_set_tid_address]   = sys_set_tid_address,
    [KPZ_SYS_clock_gettime]     = sys_clock_gettime,
    [KPZ_SYS_exit_group]        = sys_exit_group,
    [KPZ_SYS_epoll_wait]        = kpz_sys_epoll_wait,
    [KPZ_SYS_epoll_ctl]         = kpz_sys_epoll_ctl,
    [KPZ_SYS_tgkill]            = sys_tgkill,
    [KPZ_SYS_openat]            = sys_openat,
    [KPZ_SYS_mkdirat]           = sys_mkdirat,
    [KPZ_SYS_newfstatat]        = sys_newfstatat,
    [KPZ_SYS_unlinkat]          = sys_unlinkat,
    [KPZ_SYS_renameat]          = sys_renameat,
    [KPZ_SYS_readlinkat]        = sys_readlinkat,
    [KPZ_SYS_timerfd_create]    = kpz_sys_timerfd_create,
    [KPZ_SYS_eventfd]           = kpz_sys_eventfd,
    [KPZ_SYS_accept4]           = kpz_sys_accept4,
    [KPZ_SYS_epoll_create1]     = kpz_sys_epoll_create1,
    [KPZ_SYS_perf_event_open]   = kpz_sys_perf_event_open,
    [KPZ_SYS_prlimit64]         = kpz_sys_prlimit64,
    [KPZ_SYS_getcpu]            = kpz_sys_getcpu,
    [KPZ_SYS_sched_setattr]     = kpz_sys_sched_setattr,
    [KPZ_SYS_sched_getattr]     = kpz_sys_sched_getattr,
    [KPZ_SYS_seccomp]           = kpz_sys_seccomp,
    [KPZ_SYS_getrandom]         = kpz_sys_getrandom,
    [KPZ_SYS_memfd_create]      = kpz_sys_memfd_create,
    [KPZ_SYS_bpf]               = kpz_sys_bpf,
    [KPZ_SYS_membarrier]        = kpz_sys_membarrier,
    [KPZ_SYS_statx]             = kpz_sys_statx,
    [KPZ_SYS_amx]               = kpz_sys_amx,
};

KPZ_HOT s64 kposixz_dispatch(kpz_frame_t *frame)
{
    u64 nr = frame->nr;
    if (kpz_unlikely(nr >= KPZ_SYS_NR_MAX)) return KPZ_ERR(KPZE_NOSYS);

    kpz_syscall_fn_t fn = kpz_syscall_table[nr];
    if (kpz_unlikely(!fn)) return KPZ_ERR(KPZE_NOSYS);

    kposixz_proc_t *proc = kpz_current();
    if (proc) proc->syscall_count++;

    return fn(frame);
}

s32 kposixz_init(void)
{
    kpz_percpu_t *cpu = &kpz_bsp_percpu;
    kpz_memzero(cpu, sizeof(*cpu));
    cpu->self    = cpu;
    cpu->cpu_id  = 0;
    cpu->current = (void *)0;
    kpz_wrmsr(KPZ_MSR_KERNEL_GSBASE, (u64)(uptr)cpu);
    kposixz_enable_syscall();
    klog_info("kposixz", "POSIX initialised");
    return 0;
}

kpz_pid_t kposixz_current_pid(void)
{
    kposixz_proc_t *p = kpz_current();
    return p ? p->pid : 0;
}

void kposixz_shutdown(void)
{
    kpz_spin_lock(&kpz_proc_table_lock);
    for (s32 i = 0; i < KPOSIXZ_MAX_PROCS; i++) {
        if (kpz_proc_table[i]) {
            kpz_proc_free(kpz_proc_table[i]);
            kpz_proc_table[i] = (void *)0;
        }
    }
    kpz_spin_unlock(&kpz_proc_table_lock);
    klog_info("kposixz", "POSIX shutdown");
}
