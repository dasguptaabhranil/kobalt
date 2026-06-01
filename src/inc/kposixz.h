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

#define KPZE_OK         0
#define KPZE_PERM       1
#define KPZE_NOENT      2
#define KPZE_IO         5
#define KPZE_NOEXEC     8
#define KPZE_BADF       9
#define KPZE_CHILD      10
#define KPZE_AGAIN      11
#define KPZE_NOMEM      12
#define KPZE_ACCES      13
#define KPZE_FAULT      14
#define KPZE_BUSY       16
#define KPZE_EXIST      17
#define KPZE_NOTDIR     20
#define KPZE_ISDIR      21
#define KPZE_INVAL      22
#define KPZE_NFILE      23
#define KPZE_MFILE      24
#define KPZE_NOTTY      25
#define KPZE_FBIG       27
#define KPZE_NOSPC      28
#define KPZE_SPIPE      29
#define KPZE_ROFS       30
#define KPZE_PIPE       32
#define KPZE_RANGE      34
#define KPZE_NAMETOOLONG 36
#define KPZE_NOSYS      38
#define KPZE_NOTEMPTY   39
#define KPZE_OVERFLOW   75
#define KPZE_TIMEDOUT   110

#define KPZ_ERR(e)  ((s64)(-(e)))
#define KPZ_IS_ERR(v) ((u64)(v) >= (u64)(-4096LL))

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

#define KPZ_SYS_read              0
#define KPZ_SYS_write             1
#define KPZ_SYS_open              2
#define KPZ_SYS_close             3
#define KPZ_SYS_stat              4
#define KPZ_SYS_fstat             5
#define KPZ_SYS_lstat             6
#define KPZ_SYS_lseek             8
#define KPZ_SYS_mmap              9
#define KPZ_SYS_mprotect         10
#define KPZ_SYS_munmap           11
#define KPZ_SYS_brk              12
#define KPZ_SYS_rt_sigaction     13
#define KPZ_SYS_rt_sigprocmask   14
#define KPZ_SYS_rt_sigreturn     15
#define KPZ_SYS_ioctl            16
#define KPZ_SYS_writev           20
#define KPZ_SYS_access           21
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
#define KPZ_SYS_setsockopt       54
#define KPZ_SYS_getsockopt       55
#define KPZ_SYS_exit             60
#define KPZ_SYS_fcntl            72
#define KPZ_SYS_gettimeofday     96
#define KPZ_SYS_getuid          102
#define KPZ_SYS_getgid          104
#define KPZ_SYS_geteuid         107
#define KPZ_SYS_getegid         108
#define KPZ_SYS_getppid         110
#define KPZ_SYS_sysinfo         99
#define KPZ_SYS_arch_prctl      158
#define KPZ_SYS_gettid          186
#define KPZ_SYS_set_tid_address 218
#define KPZ_SYS_clock_gettime   228
#define KPZ_SYS_exit_group      231
#define KPZ_SYS_openat          257
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

void kposixz_shutdown(void);

#ifdef __cplusplus
}
#endif
#endif
