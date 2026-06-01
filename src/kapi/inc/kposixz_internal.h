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

#ifndef __KPOSIXZ_INTERNAL_H__
#define __KPOSIXZ_INTERNAL_H__

#include "kposixz.h"
#include <vfs.h>

extern void *kmalloc(usz size);
extern void  kfree(void *ptr);
extern void  klog_info(const char *tag, const char *msg);
extern void  klog_fail(const char *tag, const char *msg);

extern uptr  kobalt_vmm_alloc(uptr hint, usz size, u32 prot);
extern void  kobalt_vmm_free(uptr addr, usz size);
extern s32   kobalt_vmm_protect(uptr addr, usz size, u32 prot);

extern u64   kobalt_acpi_timer_ns(void);
extern void  kobalt_sched_yield(void);

static inline void kpz_wrmsr(u32 msr, u64 val) {
    __asm__ volatile("wrmsr"
        :: "c"(msr), "a"((u32)val), "d"((u32)(val >> 32))
        : "memory");
}
static inline u64 kpz_rdmsr(u32 msr) {
    u32 lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((u64)hi << 32) | lo;
}

#define KPZ_MSR_STAR           0xC0000081U
#define KPZ_MSR_LSTAR          0xC0000082U
#define KPZ_MSR_FMASK          0xC0000084U
#define KPZ_MSR_FSBASE         0xC0000100U
#define KPZ_MSR_GSBASE         0xC0000101U
#define KPZ_MSR_KERNEL_GSBASE  0xC0000102U

#define kpz_atomic_load(ptr)          __atomic_load_n((ptr), __ATOMIC_ACQUIRE)
#define kpz_atomic_store(ptr, val)    __atomic_store_n((ptr), (val), __ATOMIC_RELEASE)
#define kpz_atomic_inc(ptr)           __atomic_add_fetch((ptr), 1, __ATOMIC_SEQ_CST)
#define kpz_atomic_dec(ptr)           __atomic_sub_fetch((ptr), 1, __ATOMIC_SEQ_CST)
#define kpz_atomic_cas(ptr, old, new) \
    __atomic_compare_exchange_n((ptr), (old), (new), 0, \
                                __ATOMIC_SEQ_CST, __ATOMIC_ACQUIRE)

typedef volatile u32 kpz_spinlock_t;
#define KPZ_SPINLOCK_INIT  0U

static inline void kpz_spin_lock(kpz_spinlock_t *l) {
    while (__atomic_test_and_set(l, __ATOMIC_ACQUIRE))
        __asm__ volatile("pause" ::: "memory");
}
static inline void kpz_spin_unlock(kpz_spinlock_t *l) {
    __atomic_clear(l, __ATOMIC_RELEASE);
}

typedef struct KPZ_ALIGNED(16) kpz_frame {
    u64 nr;
    u64 arg1, arg2, arg3, arg4, arg5, arg6;
    u64 rbx, rbp, r12, r13, r14, r15;
    u64 rip;
    u64 rflags;
} kpz_frame_t;

struct kposixz_file;

typedef struct kpz_vfs_ops {
    s64  (*read)    (struct kposixz_file *f, void *buf, u64 len);
    s64  (*write)   (struct kposixz_file *f, const void *buf, u64 len);
    s64  (*seek)    (struct kposixz_file *f, s64 off, u32 whence);
    s64  (*stat)    (struct kposixz_file *f, kpz_stat_t *out);
    s64  (*ioctl)   (struct kposixz_file *f, u64 req, u64 arg);
    void (*close)   (struct kposixz_file *f);
    s64  (*getdents)(struct kposixz_file *f, void *buf, u64 len);
    u32  (*poll)    (struct kposixz_file *f, u32 events);
} kpz_vfs_ops_t;

typedef struct kposixz_file {
    const kpz_vfs_ops_t *ops;
    void                *priv;
    kpz_off_t            pos;
    u32                  flags;
    u32                  mode;
    volatile s32         refcount;
    kpz_spinlock_t       lock;
} kposixz_file_t;

typedef struct {
    kposixz_file_t *files[KPOSIXZ_MAX_FD];
    u8              cloexec[KPOSIXZ_MAX_FD];
    kpz_spinlock_t  lock;
} kpz_fd_table_t;

#define KPZ_PROC_EMBRYO    0
#define KPZ_PROC_RUNNING   1
#define KPZ_PROC_SLEEPING  2
#define KPZ_PROC_ZOMBIE    3
#define KPZ_PROC_DEAD      4

typedef struct KPZ_ALIGNED(64) kposixz_proc {
    kpz_pid_t            pid;
    kpz_pid_t            ppid;
    kpz_uid_t            uid;
    kpz_gid_t            gid;
    volatile u8          state;

    uptr                 mm_base;
    uptr                 mm_brk;
    uptr                 mm_brk_start;
    uptr                 stack_top;

    u8                  *kstack;
    uptr                 kstack_top;

    kpz_fd_table_t       fds;

    u64                  fs_base;
    u64                  gs_base;

    s32                  exit_code;

    kpz_sigaction_t      sigactions[32];
    u64                  sig_mask;
    u64                  sig_pending;

    char                 cwd[KPOSIXZ_MAX_PATH];

    kpz_rlimit_t         rlimits[KPZ_RLIM_NLIMITS];

    kpz_pid_t            sid;
    kpz_pid_t            pgid;

    struct kposixz_proc *next;

    volatile s32         refcount;

    u64                  syscall_count;
    u64                  start_ns;
} kposixz_proc_t;

typedef struct KPZ_ALIGNED(64) kpz_percpu {
    struct kpz_percpu  *self;
    u64                 kernel_rsp;
    u64                 user_rsp;
    u32                 cpu_id;
    u32                 _pad;
    kposixz_proc_t     *current;
} kpz_percpu_t;

extern kposixz_proc_t  *kpz_proc_table[KPOSIXZ_MAX_PROCS];
extern kpz_spinlock_t   kpz_proc_table_lock;
extern kpz_percpu_t     kpz_bsp_percpu;

kposixz_proc_t *kpz_proc_alloc(kpz_pid_t ppid);
void            kpz_proc_free(kposixz_proc_t *proc);
kposixz_proc_t *kpz_proc_lookup(kpz_pid_t pid);
void            kpz_proc_exit(kposixz_proc_t *proc, s32 code) __attribute__((noreturn));
kpz_pid_t       kpz_proc_next_pid(void);

s32             kpz_fd_alloc(kposixz_proc_t *proc, kposixz_file_t *file);
kposixz_file_t *kpz_fd_get(kposixz_proc_t *proc, kpz_fd_t fd);
void            kpz_fd_put(kposixz_file_t *file);
s32             kpz_fd_close(kposixz_proc_t *proc, kpz_fd_t fd);
s32             kpz_fd_dup(kposixz_proc_t *proc, kpz_fd_t oldfd);
s32             kpz_fd_dup2(kposixz_proc_t *proc, kpz_fd_t oldfd, kpz_fd_t newfd);

typedef struct {
    int  vfs_fd;
    usz  size;
    u8   is_dir;
    char dirpath[VFS_PATH_MAX];
} kpz_kfs_priv_t;

kposixz_file_t *kpz_kfs_open(const char *path, u32 flags);
kposixz_file_t *kpz_devfs_open(const char *path, u32 flags);
kposixz_file_t *kpz_devfs_open_stdin(void);
kposixz_file_t *kpz_devfs_open_stdout(void);
kposixz_file_t *kpz_devfs_open_stderr(void);

s64             kposixz_dispatch(kpz_frame_t *frame);

void            kposixz_syscall_entry(void);
void            kposixz_enable_syscall(void);

static inline kposixz_proc_t *kpz_current(void) {
    kposixz_proc_t *p;
    __asm__ volatile("movq %%gs:32, %0" : "=r"(p));
    return p;
}

static inline void kpz_set_current(kposixz_proc_t *p) {
    __asm__ volatile("movq %0, %%gs:32" :: "r"(p) : "memory");
    if (p)
        __asm__ volatile("movq %0, %%gs:8" :: "r"(p->kstack_top) : "memory");
}

static inline void kpz_ns_to_timespec(u64 ns, kpz_timespec_t *ts) {
    ts->tv_sec  = (kpz_time_t)(ns / 1000000000ULL);
    ts->tv_nsec = (s64)(ns % 1000000000ULL);
}

#define KPZ_USER_SPACE_MAX  0x0000800000000000ULL
static inline int kpz_check_ptr(const void *ptr, usz len) {
    uptr p = (uptr)ptr;
    if (!p || !len) return -KPZE_FAULT;
    if (p >= KPZ_USER_SPACE_MAX) return -KPZE_FAULT;
    if (p + len < p) return -KPZE_FAULT;
    if (p + len > KPZ_USER_SPACE_MAX) return -KPZE_FAULT;
    return 0;
}

static inline usz kpz_user_strnlen(const char *s, usz max) {
    usz n = 0;
    while (n < max && s[n]) n++;
    return n;
}

static inline void kpz_memzero(void *p, usz n) {
    volatile u8 *b = (volatile u8 *)p;
    while (n--) *b++ = 0;
}
static inline void kpz_memcpy(void *dst, const void *src, usz n) {
    u8 *d = (u8 *)dst; const u8 *s = (const u8 *)src;
    while (n--) *d++ = *s++;
}
static inline s32 kpz_strncmp(const char *a, const char *b, usz n) {
    for (usz i = 0; i < n; i++) {
        if (a[i] != b[i]) return (u8)a[i] - (u8)b[i];
        if (!a[i]) return 0;
    }
    return 0;
}
static inline usz kpz_strlen(const char *s) {
    usz n = 0; while (s[n]) n++; return n;
}
static inline void kpz_strncpy(char *d, const char *s, usz n) {
    usz i = 0;
    for (; i < n - 1 && s[i]; i++) d[i] = s[i];
    d[i] = '\0';
}

#endif
