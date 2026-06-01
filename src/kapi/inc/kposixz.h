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

#ifndef __KPOSIXZ_H__
#define __KPOSIXZ_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __KOBALT_TYPES_DEFINED__
#define __KOBALT_TYPES_DEFINED__
typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;
typedef signed   char       s8;
typedef signed   short      s16;
typedef signed   int        s32;
typedef signed   long long  s64;
typedef __UINTPTR_TYPE__    uptr;
typedef __SIZE_TYPE__        usz;
#endif

#define KPZ_NORETURN    __attribute__((noreturn))
#define KPZ_NOINLINE    __attribute__((noinline))
#define KPZ_HOT         __attribute__((hot))
#define KPZ_COLD        __attribute__((cold))
#define KPZ_PACKED      __attribute__((packed))
#define KPZ_ALIGNED(n)  __attribute__((aligned(n)))
#define KPZ_UNUSED      __attribute__((unused))
#define KPZ_WARN_UNUSED __attribute__((warn_unused_result))
#define KPZ_ALWAYS_INL  __attribute__((always_inline)) static inline
#define kpz_likely(x)   __builtin_expect(!!(x), 1)
#define kpz_unlikely(x) __builtin_expect(!!(x), 0)

#define KPZE_OK          0
#define KPZE_PERM        1
#define KPZE_NOENT       2
#define KPZE_IO          5
#define KPZE_NOEXEC      8
#define KPZE_BADF        9
#define KPZE_CHILD       10
#define KPZE_AGAIN       11
#define KPZE_NOMEM       12
#define KPZE_ACCES       13
#define KPZE_FAULT       14
#define KPZE_BUSY        16
#define KPZE_EXIST       17
#define KPZE_XDEV        18
#define KPZE_NOTDIR      20
#define KPZE_ISDIR       21
#define KPZE_INVAL       22
#define KPZE_NFILE       23
#define KPZE_MFILE       24
#define KPZE_NOTTY       25
#define KPZE_FBIG        27
#define KPZE_NOSPC       28
#define KPZE_SPIPE       29
#define KPZE_ROFS        30
#define KPZE_PIPE        32
#define KPZE_RANGE       34
#define KPZE_NAMETOOLONG 36
#define KPZE_NOSYS       38
#define KPZE_NOTEMPTY    39
#define KPZE_OVERFLOW    75
#define KPZE_TIMEDOUT    110
#define KPZE_NOTSOCK     88
#define KPZE_CONNREFUSED 111
#define KPZE_NOTCONN     107

#define KPZ_ERR(e)      ((s64)(-(e)))
#define KPZ_IS_ERR(v)   ((u64)(v) >= (u64)(-4096LL))

#define KPZ_O_RDONLY    0x000000
#define KPZ_O_WRONLY    0x000001
#define KPZ_O_RDWR      0x000002
#define KPZ_O_CREAT     0x000040
#define KPZ_O_EXCL      0x000080
#define KPZ_O_NOCTTY    0x000100
#define KPZ_O_TRUNC     0x000200
#define KPZ_O_APPEND    0x000400
#define KPZ_O_NONBLOCK  0x000800
#define KPZ_O_CLOEXEC   0x080000
#define KPZ_O_DIRECTORY 0x010000
#define KPZ_O_NOFOLLOW  0x020000

#define KPZ_SEEK_SET    0
#define KPZ_SEEK_CUR    1
#define KPZ_SEEK_END    2

#define KPZ_PROT_NONE   0x00
#define KPZ_PROT_READ   0x01
#define KPZ_PROT_WRITE  0x02
#define KPZ_PROT_EXEC   0x04
#define KPZ_MAP_SHARED  0x01
#define KPZ_MAP_PRIVATE 0x02
#define KPZ_MAP_FIXED   0x10
#define KPZ_MAP_ANON    0x20
#define KPZ_MAP_FAILED  ((void *)~(uptr)0)
#define KPZ_MREMAP_MAYMOVE 1

#define KPZ_F_DUPFD        0
#define KPZ_F_GETFD        1
#define KPZ_F_SETFD        2
#define KPZ_F_GETFL        3
#define KPZ_F_SETFL        4
#define KPZ_FD_CLOEXEC     1

#define KPZ_TIOCGWINSZ  0x5413
#define KPZ_TCGETS      0x5401

#define KPZ_ARCH_SET_FS  0x1002
#define KPZ_ARCH_GET_FS  0x1003
#define KPZ_ARCH_SET_GS  0x1001
#define KPZ_ARCH_GET_GS  0x1004
#define KPZ_ARCH_REQ_XCOMP_PERM 0x1023

#define KPZ_CLOCK_REALTIME          0
#define KPZ_CLOCK_MONOTONIC         1
#define KPZ_CLOCK_MONOTONIC_RAW     4
#define KPZ_CLOCK_REALTIME_COARSE   5
#define KPZ_CLOCK_MONOTONIC_COARSE  6

#define KPZ_S_IFMT   0170000
#define KPZ_S_IFREG  0100000
#define KPZ_S_IFDIR  0040000
#define KPZ_S_IFCHR  0020000
#define KPZ_S_IFBLK  0060000
#define KPZ_S_IFIFO  0010000
#define KPZ_S_IFLNK  0120000
#define KPZ_S_IFSOCK 0140000

#define KPZ_S_ISREG(m)  (((m) & KPZ_S_IFMT) == KPZ_S_IFREG)
#define KPZ_S_ISDIR(m)  (((m) & KPZ_S_IFMT) == KPZ_S_IFDIR)
#define KPZ_S_ISCHR(m)  (((m) & KPZ_S_IFMT) == KPZ_S_IFCHR)

#define KPZ_POLLIN   0x0001
#define KPZ_POLLPRI  0x0002
#define KPZ_POLLOUT  0x0004
#define KPZ_POLLERR  0x0008
#define KPZ_POLLHUP  0x0010
#define KPZ_POLLNVAL 0x0020

#define KPZ_EPOLLIN       0x00000001U
#define KPZ_EPOLLPRI      0x00000002U
#define KPZ_EPOLLOUT      0x00000004U
#define KPZ_EPOLLERR      0x00000008U
#define KPZ_EPOLLHUP      0x00000010U
#define KPZ_EPOLLET       0x80000000U
#define KPZ_EPOLL_CTL_ADD 1
#define KPZ_EPOLL_CTL_DEL 2
#define KPZ_EPOLL_CTL_MOD 3

#define KPZ_FUTEX_WAIT        0
#define KPZ_FUTEX_WAKE        1
#define KPZ_FUTEX_PRIVATE     128
#define KPZ_FUTEX_WAIT_PRIVATE (KPZ_FUTEX_WAIT | KPZ_FUTEX_PRIVATE)
#define KPZ_FUTEX_WAKE_PRIVATE (KPZ_FUTEX_WAKE | KPZ_FUTEX_PRIVATE)

#define KPZ_RLIMIT_CPU       0
#define KPZ_RLIMIT_FSIZE     1
#define KPZ_RLIMIT_DATA      2
#define KPZ_RLIMIT_STACK     3
#define KPZ_RLIMIT_CORE      4
#define KPZ_RLIMIT_RSS       5
#define KPZ_RLIMIT_NPROC     6
#define KPZ_RLIMIT_NOFILE    7
#define KPZ_RLIMIT_MEMLOCK   8
#define KPZ_RLIMIT_AS        9
#define KPZ_RLIMIT_LOCKS     10
#define KPZ_RLIMIT_SIGPENDING 11
#define KPZ_RLIMIT_MSGQUEUE  12
#define KPZ_RLIMIT_NICE      13
#define KPZ_RLIMIT_RTPRIO    14
#define KPZ_RLIMIT_RTTIME    15
#define KPZ_RLIM_NLIMITS     16
#define KPZ_RLIM_INFINITY    (~0ULL)

#define KPZ_PR_SET_PDEATHSIG   1
#define KPZ_PR_GET_PDEATHSIG   2
#define KPZ_PR_GET_DUMPABLE    3
#define KPZ_PR_SET_DUMPABLE    4
#define KPZ_PR_SET_NAME        15
#define KPZ_PR_GET_NAME        16
#define KPZ_PR_SET_NO_NEW_PRIVS 38
#define KPZ_PR_GET_NO_NEW_PRIVS 39
#define KPZ_PR_SET_SECCOMP     22
#define KPZ_PR_GET_SECCOMP     21

#define KPZ_AT_FDCWD  (-100)

#define KPZ_STATX_TYPE       0x00000001U
#define KPZ_STATX_MODE       0x00000002U
#define KPZ_STATX_NLINK      0x00000004U
#define KPZ_STATX_UID        0x00000008U
#define KPZ_STATX_GID        0x00000010U
#define KPZ_STATX_ATIME      0x00000020U
#define KPZ_STATX_MTIME      0x00000040U
#define KPZ_STATX_CTIME      0x00000080U
#define KPZ_STATX_INO        0x00000100U
#define KPZ_STATX_SIZE       0x00000200U
#define KPZ_STATX_BLOCKS     0x00000400U
#define KPZ_STATX_BASIC_STATS 0x000007ffU
#define KPZ_STATX_BTIME      0x00000800U

#define KPZ_SECCOMP_SET_MODE_STRICT  0
#define KPZ_SECCOMP_SET_MODE_FILTER  1

#define KPZ_MEMBARRIER_CMD_QUERY                    0
#define KPZ_MEMBARRIER_CMD_GLOBAL                   1
#define KPZ_MEMBARRIER_CMD_PRIVATE_EXPEDITED        8
#define KPZ_MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED 16

#define KPZ_EFD_CLOEXEC    0x080000
#define KPZ_EFD_NONBLOCK   0x000800
#define KPZ_EFD_SEMAPHORE  0x000001

#define KPZ_TFD_CLOEXEC    0x080000
#define KPZ_TFD_NONBLOCK   0x000800
#define KPZ_CLOCK_REALTIME_id  0
#define KPZ_CLOCK_MONOTONIC_id 1

#define KPZ_MFD_CLOEXEC      0x0001U
#define KPZ_MFD_ALLOW_SEALING 0x0002U

typedef s32 kpz_pid_t;
typedef u32 kpz_uid_t;
typedef u32 kpz_gid_t;
typedef s32 kpz_fd_t;
typedef u32 kpz_mode_t;
typedef s64 kpz_off_t;
typedef u64 kpz_ino_t;
typedef u64 kpz_dev_t;
typedef s64 kpz_time_t;
typedef s64 kpz_suseconds_t;

typedef struct kpz_timespec {
    kpz_time_t  tv_sec;
    s64         tv_nsec;
} kpz_timespec_t;

typedef struct kpz_timeval {
    kpz_time_t      tv_sec;
    kpz_suseconds_t tv_usec;
} kpz_timeval_t;

typedef struct KPZ_ALIGNED(8) kpz_stat {
    kpz_dev_t   st_dev;
    kpz_ino_t   st_ino;
    kpz_mode_t  st_mode;
    u32         st_nlink;
    kpz_uid_t   st_uid;
    kpz_gid_t   st_gid;
    kpz_dev_t   st_rdev;
    kpz_off_t   st_size;
    u64         st_blksize;
    u64         st_blocks;
    kpz_timespec_t st_atim;
    kpz_timespec_t st_mtim;
    kpz_timespec_t st_ctim;
} kpz_stat_t;

typedef struct kpz_winsize {
    u16 ws_row;
    u16 ws_col;
    u16 ws_xpixel;
    u16 ws_ypixel;
} kpz_winsize_t;

typedef struct kpz_iovec {
    void *iov_base;
    usz   iov_len;
} kpz_iovec_t;

typedef struct kpz_sigaction {
    void  (*sa_handler)(s32);
    u64    sa_flags;
    void  (*sa_restorer)(void);
    u64    sa_mask[2];
} kpz_sigaction_t;

typedef struct kpz_pollfd {
    s32 fd;
    s16 events;
    s16 revents;
} kpz_pollfd_t;

typedef struct kpz_fd_set {
    u64 fds_bits[16];
} kpz_fd_set_t;

typedef struct KPZ_PACKED kpz_epoll_event {
    u32 events;
    u64 data;
} kpz_epoll_event_t;

typedef struct kpz_rlimit {
    u64 rlim_cur;
    u64 rlim_max;
} kpz_rlimit_t;

typedef struct KPZ_ALIGNED(8) kpz_dirent64 {
    kpz_ino_t d_ino;
    s64       d_off;
    u16       d_reclen;
    u8        d_type;
    char      d_name[256];
} kpz_dirent64_t;

typedef struct kpz_statx_ts {
    s64 tv_sec;
    u32 tv_nsec;
    s32 __pad;
} kpz_statx_ts_t;

typedef struct KPZ_ALIGNED(8) kpz_statx {
    u32          stx_mask;
    u32          stx_blksize;
    u64          stx_attributes;
    u32          stx_nlink;
    u32          stx_uid;
    u32          stx_gid;
    u16          stx_mode;
    u16          __spare0;
    u64          stx_ino;
    u64          stx_size;
    u64          stx_blocks;
    u64          stx_attributes_mask;
    kpz_statx_ts_t stx_atime;
    kpz_statx_ts_t stx_btime;
    kpz_statx_ts_t stx_ctime;
    kpz_statx_ts_t stx_mtime;
    u32          stx_rdev_major;
    u32          stx_rdev_minor;
    u32          stx_dev_major;
    u32          stx_dev_minor;
    u64          stx_mnt_id;
    u64          __spare2[13];
} kpz_statx_t;

typedef struct kpz_sched_attr {
    u32 size;
    u32 sched_policy;
    u64 sched_flags;
    s32 sched_nice;
    u32 sched_priority;
    u64 sched_runtime;
    u64 sched_deadline;
    u64 sched_period;
} kpz_sched_attr_t;

typedef struct kpz_itimerspec {
    kpz_timespec_t it_interval;
    kpz_timespec_t it_value;
} kpz_itimerspec_t;

#define KPZ_SYS_read              0
#define KPZ_SYS_write             1
#define KPZ_SYS_open              2
#define KPZ_SYS_close             3
#define KPZ_SYS_stat              4
#define KPZ_SYS_fstat             5
#define KPZ_SYS_lstat             6
#define KPZ_SYS_poll              7
#define KPZ_SYS_lseek             8
#define KPZ_SYS_mmap              9
#define KPZ_SYS_mprotect         10
#define KPZ_SYS_munmap           11
#define KPZ_SYS_brk              12
#define KPZ_SYS_rt_sigaction     13
#define KPZ_SYS_rt_sigprocmask   14
#define KPZ_SYS_rt_sigreturn     15
#define KPZ_SYS_ioctl            16
#define KPZ_SYS_readv            19
#define KPZ_SYS_writev           20
#define KPZ_SYS_access           21
#define KPZ_SYS_pipe             22
#define KPZ_SYS_select           23
#define KPZ_SYS_mremap           25
#define KPZ_SYS_dup              32
#define KPZ_SYS_dup2             33
#define KPZ_SYS_getpid           39
#define KPZ_SYS_socket           41
#define KPZ_SYS_connect          42
#define KPZ_SYS_accept           43
#define KPZ_SYS_sendto           44
#define KPZ_SYS_recvfrom         45
#define KPZ_SYS_shutdown         48
#define KPZ_SYS_bind             49
#define KPZ_SYS_listen           50
#define KPZ_SYS_getsockname      51
#define KPZ_SYS_getpeername      52
#define KPZ_SYS_setsockopt       54
#define KPZ_SYS_getsockopt       55
#define KPZ_SYS_clone            56
#define KPZ_SYS_fork             57
#define KPZ_SYS_execve           59
#define KPZ_SYS_exit             60
#define KPZ_SYS_wait4            61
#define KPZ_SYS_kill             62
#define KPZ_SYS_fcntl            72
#define KPZ_SYS_fsync            74
#define KPZ_SYS_fdatasync        75
#define KPZ_SYS_truncate         76
#define KPZ_SYS_ftruncate        77
#define KPZ_SYS_getdents         78
#define KPZ_SYS_getcwd           79
#define KPZ_SYS_chdir            80
#define KPZ_SYS_rename           82
#define KPZ_SYS_mkdir            83
#define KPZ_SYS_rmdir            84
#define KPZ_SYS_unlink           87
#define KPZ_SYS_symlink          88
#define KPZ_SYS_readlink         89
#define KPZ_SYS_getrlimit        97
#define KPZ_SYS_sysinfo          99
#define KPZ_SYS_getuid          102
#define KPZ_SYS_getgid          104
#define KPZ_SYS_geteuid         107
#define KPZ_SYS_getegid         108
#define KPZ_SYS_getppid         110
#define KPZ_SYS_setsid          112
#define KPZ_SYS_mlock           149
#define KPZ_SYS_munlock         150
#define KPZ_SYS_prctl           157
#define KPZ_SYS_arch_prctl      158
#define KPZ_SYS_setrlimit       160
#define KPZ_SYS_gettid          186
#define KPZ_SYS_tkill           200
#define KPZ_SYS_futex           202
#define KPZ_SYS_sched_setaffinity 203
#define KPZ_SYS_sched_getaffinity 204
#define KPZ_SYS_getdents64      217
#define KPZ_SYS_set_tid_address 218
#define KPZ_SYS_clock_gettime   228
#define KPZ_SYS_exit_group      231
#define KPZ_SYS_epoll_wait      232
#define KPZ_SYS_epoll_ctl       233
#define KPZ_SYS_tgkill          234
#define KPZ_SYS_openat          257
#define KPZ_SYS_mkdirat         258
#define KPZ_SYS_newfstatat      262
#define KPZ_SYS_unlinkat        263
#define KPZ_SYS_renameat        264
#define KPZ_SYS_readlinkat      267
#define KPZ_SYS_timerfd_create  283
#define KPZ_SYS_eventfd         284
#define KPZ_SYS_accept4         288
#define KPZ_SYS_epoll_create1   291
#define KPZ_SYS_perf_event_open 298
#define KPZ_SYS_prlimit64       302
#define KPZ_SYS_getcpu          309
#define KPZ_SYS_sched_setattr   314
#define KPZ_SYS_sched_getattr   315
#define KPZ_SYS_seccomp         317
#define KPZ_SYS_getrandom       318
#define KPZ_SYS_memfd_create    319
#define KPZ_SYS_bpf             321
#define KPZ_SYS_membarrier      324
#define KPZ_SYS_gettimeofday    96
#define KPZ_SYS_statx           332
#define KPZ_SYS_amx             448

#define KPZ_SYS_NR_MAX          512

#define KPOSIXZ_MAX_PROCS     256
#define KPOSIXZ_MAX_FD        256
#define KPOSIXZ_MAX_PATH      256
#define KPOSIXZ_KERNEL_STACK  16384

KPZ_WARN_UNUSED s32 kposixz_init(void);

KPZ_WARN_UNUSED kpz_pid_t
kposixz_spawn(const char *elf_path,
              const void *elf_image, usz elf_size,
              const char *const argv[],
              const char *const envp[]);

kpz_pid_t kposixz_current_pid(void);
void      kposixz_shutdown(void);

#ifdef __cplusplus
}
#endif
#endif
