/* Copyright (C) 2026 Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <kernel.h>
#include <idt.h>
#include <gdt.h>
#include <cpuid.h>
#include <msr.h>
#include <smap.h>
#include <pat.h>
#include <speculation.h>
#include <debug.h>
#include <exception.h>
#include <fpu.h>
#include <hw_breakpoint.h>
#include <signal_frame.h>
#include <syscall.h>
#include <uaccess.h>
#include <vdso.h>
#include <xsave.h>
#include <acpi.h>
#include <madt.h>
#include <pci.h>
#include <pci_msi.h>
#include "../drivers/vga/vga.h"
#include "../drivers/vga/fb.h"
#include "../drivers/vga/fb_font.h"
#include "../drivers/vga/fb_console.h"
#include "../drivers/ps2/kbd.h"
#include <iommu.h>
#include <virtio.h>
#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/timeouts.h>
#include <lwip/icmp.h>
#include <lwip/ip_addr.h>
#include <lwip/raw.h>
#include <netif/ethernet.h>
#include <lwip/ip4_addr.h>
#include <lwip/inet_chksum.h>
#include <lwip/dhcp.h>
#include <lwip/prot/dhcp.h>
#include <lwip/etharp.h>
#include <lwip/tcp.h>
#include <lwip/udp.h>
#include <lwip/stats.h>
#include <kmalloc.h>
#include <sched.h>
#include <irq.h>
#include <apic_timer.h>
#include <kssh.h>
#include <kobalt_ident.h>
#include <tykid.h>
#include <kposixz.h>
#include <ktrace.h>
#include <sound.h>
#include <blkdev.h>
#include "../drivers/ahci/ahci.h"
#include "../drivers/nvme/nvme.h"
#include "../drivers/virtio/storage/virtio_blk.h"
#include <usb_init.h>
#include "../drivers/net/e1000/e1000.h"
#include "../drivers/net/igc/kobalt_igc.h"
#include "../drivers/net/ixgbe/kobalt_ixgbe.h"
#include "../drivers/usb/ehci/kobalt_ehci.h"
#include <percpu.h>
#include <spinlock.h>
#include <ipi.h>
#include <smp.h>
#include <random.h>
#include <flatfs.h>
#include <flatfs_vfs.h>
#include <flatfs_kobalt.h>
#include <tmpfs_vfs.h>
#include <fatfs_kobalt.h>
#include <ff.h>
#include <devfs_kobalt.h>
#include <devfs_vfs.h>
#include <procfs_vfs.h>
#include <sysfs_vfs.h>
#include <vfs.h>
#include <rtc.h>
#include <numa.h>
#include <cpufreq.h>
#include <cpuidle.h>
#include <hugepage.h>
#include <mreclaim.h>
#include <swap.h>
#include <cpuhp.h>
#include <waitqueue.h>
#include <amx_init.h>
#include <hrtimer.h>
#include "ksh.h"

uint32_t        sched_thread_count(void);
sched_thread_t *sched_get_thread(uint32_t idx);
sched_thread_t *sched_get_thread_by_tid(uint32_t tid);
uint32_t        sched_thread_get_tid(sched_thread_t *t);
uint32_t        sched_thread_get_state(sched_thread_t *t);
const char     *sched_thread_get_name(sched_thread_t *t);
sched_thread_t *sched_current(void);
void            sched_kill_current(void);
void            sched_unblock(sched_thread_t *t);
uint64_t        sched_tick_count(void);
void            sched_migrate(sched_thread_t *t, uint32_t cpu);
void            sched_set_priority(sched_thread_t *t, int prio);
void            sched_set_affinity(sched_thread_t *t, uint64_t mask);
void            sched_thread_signal(sched_thread_t *t, int sig);

void amx_run_tests(void);
extern int g_amx_supported;
extern uint64_t tsc_khz;

int tykid_kobalt_builtin_approved(tykid_gate_ctx_t *ctx, const char *name);

#ifndef KOBALT_OS
#define KOBALT_OS       "Kobalt"
#define KOBALT_VERSION  "1.0.4-b"
#define KOBALT_BUILD    "#1 SMP"
#define KOBALT_ARCH     "x86_64"
#define KOBALT_HOSTNAME "kobalt"
#endif

#define KSH_MAX_ARGS  32
#define KSH_LINE_MAX  512
#define KSH_HIST_MAX  32
#define NALIAS        16

extern struct netif kobalt_netif;
#ifndef IP_PROTO_IP
#define IP_PROTO_IP 0
#endif
extern void    net_poll(void);
extern uint32_t sys_now(void);
extern tykid_gate_ctx_t *tykid_kobalt_get_ctx(void);

static char g_alias_name[NALIAS][32];
static char g_alias_val[NALIAS][256];
static int  g_nalias;

static char g_hist[KSH_HIST_MAX][KSH_LINE_MAX];
static int  g_hist_n;
static int  g_hist_pos;

static volatile int g_ping_received = 0;

static inline uint64_t rdmsr64(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr64(uint32_t msr, uint64_t v)
{
    __asm__ volatile("wrmsr" :: "c"(msr), "a"((uint32_t)v), "d"((uint32_t)(v >> 32)));
}

static inline uint64_t rdtsc_now(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc":"=a"(lo),"=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline void cpuid_ex(uint32_t leaf, uint32_t sub,
                             uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(sub));
}

static inline int katoi(const char *s)
{
    int n = 0, neg = 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') n = n * 10 + (*s++ - '0');
    return neg ? -n : n;
}

static void tsc_delay_ms(uint32_t ms)
{
    if (!tsc_khz) {
        uint32_t t0 = sys_now();
        while ((sys_now() - t0) < ms) { cpu_relax(); sched_yield(); }
        return;
    }
    uint64_t end = rdtsc_now() + (uint64_t)tsc_khz * ms;
    while (rdtsc_now() < end) { cpu_relax(); sched_yield(); }
}

typedef struct { const char *name; const char *brief; int (*handler)(int, char **); } ksh_cmd_t;

static int cmd_help(int argc, char **argv);

static void kputc(char c) { vga_putc(c); uart_putc(c); }
void kputs(const char *s) { vga_puts(s); uart_puts(s); }

__attribute__((weak)) void kmalloc_dump(void) {}
__attribute__((weak)) int  fatfs_check_volume(int v) { (void)v; return -1; }
__attribute__((weak)) percpu_t *percpu_get_by_id(uint32_t id) { (void)id; return NULL; }
__attribute__((weak)) uintptr_t ksym_find(const char *n) { (void)n; return 0; }
__attribute__((weak)) uint64_t hpet_counter(void) { return 0; }
__attribute__((weak)) rtc_time_t g_boot_time;
__attribute__((weak)) void kposixz_dump_table(void)  { kputs("kposixz: not linked\n"); }
__attribute__((weak)) void kposixz_run_tests(void)   { kputs("kposixz: not linked\n"); }
__attribute__((weak)) void ktrace_attach(sched_thread_t *t) { (void)t; }
__attribute__((weak)) void ktrace_dump(void)         {}
__attribute__((weak)) void ktrace_clear(void)        {}
__attribute__((weak)) void acpi_pm_dump(void)        { kputs("acpi_pm: not linked\n"); }
__attribute__((weak)) void mm_dump_physmap(void)     { kputs("physmap: not linked\n"); }
__attribute__((weak)) uint64_t irq_spurious_count(void) { return 0; }
__attribute__((weak)) void usb_list_devices(void)    { kputs("usb: not linked\n"); }
__attribute__((weak)) void usb_dump_descriptors(uint32_t id) { (void)id; }
__attribute__((weak)) void usb_run_loopback_test(uint32_t id) { (void)id; }
__attribute__((weak)) int  vfs_do_mount(const char *d, const char *p, const char *t) { (void)d;(void)p;(void)t; return -38; }
__attribute__((weak)) void vfs_print_mounts(void)   {}
__attribute__((weak)) void vfs_flush_all(void)      {}
__attribute__((weak)) void ioapic_print_all(void)   {}
__attribute__((weak)) void acpi_print_tables(void)  {}
__attribute__((weak)) void irq_print_stats(void)    {}
__attribute__((weak)) void ipi_send_nmi_all(void)   {}
__attribute__((weak)) void exception_print_stats(void) {}
__attribute__((weak)) uintptr_t g_kaslr_base;
__attribute__((weak)) void kmalloc_stats(unsigned idx, size_t *tot, size_t *fr) { (void)idx; *tot=0; *fr=0; }
__attribute__((weak)) void kmalloc_stats_node(uint32_t node, unsigned idx, size_t *tot, size_t *fr) { (void)node;(void)idx;*tot=0;*fr=0; }

static const char *state_str(uint32_t s)
{
    switch (s) {
    case 0: return "R";
    case 1: return "U";
    case 2: return "S";
    case 3: return "Z";
    default: return "?";
    }
}

static int cmd_ps(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint32_t n = sched_thread_count();
    kprintf("  TID  CPU  ST  WEIGHT  VRUNTIME            NAME\n");
    for (uint32_t i = 0; i < n; i++) {
        sched_thread_t *t = sched_get_thread(i);
        if (!t) continue;
        const char *raw_nm = sched_thread_get_name(t);
        char nmbuf[33];
        if (raw_nm) {
            int k;
            for (k = 0; k < 32 && raw_nm[k]; k++) nmbuf[k] = raw_nm[k];
            nmbuf[k] = '\0';
        } else {
            nmbuf[0] = '?'; nmbuf[1] = '\0';
        }
        kprintf("  %3u  %3u   %s   %5u  %lld  %s\n",
                sched_thread_get_tid(t),
                t->cpu,
                state_str(sched_thread_get_state(t)),
                t->weight,
                (long long)t->vruntime,
                nmbuf);
    }
    return 0;
}

static int cmd_kill(int argc, char **argv)
{
    if (argc < 2) { kputs("usage: kill <tid>\n"); return 1; }
    uint32_t tid = (uint32_t)katoi(argv[1]);
    sched_thread_t *t = sched_get_thread_by_tid(tid);
    if (!t) { kprintf("kill: tid %u not found\n", tid); return 1; }
    if (t == sched_current()) { sched_kill_current(); }
    t->state = THREAD_ZOMBIE;
    sched_unblock(t);
    kprintf("kill: signaled tid %u\n", tid);
    return 0;
}

static int cmd_nice(int argc, char **argv)
{
    if (argc < 3) { kputs("usage: nice <tid> <niceval -20..19>\n"); return 1; }
    uint32_t tid = (uint32_t)katoi(argv[1]);
    int      prio = katoi(argv[2]);
    sched_thread_t *t = sched_get_thread_by_tid(tid);
    if (!t) { kprintf("nice: tid %u not found\n", tid); return 1; }
    sched_set_priority(t, prio);
    kprintf("nice: tid %u prio -> %d  weight=%u\n", tid, prio, t->weight);
    return 0;
}

static int cmd_renice(int argc, char **argv) { return cmd_nice(argc, argv); }

static int cmd_sched(int argc, char **argv)
{
    (void)argc; (void)argv;
    kprintf("scheduler: EEVDF  cpus: %u  ticks: %llu\n",
            smp_cpu_count(), (unsigned long long)sched_tick_count());
    return 0;
}

static int cmd_top(int argc, char **argv)
{
    (void)argc; (void)argv;
    kputs("top -- press any key to stop\n");
    for (int it = 0; it < 5; it++) {
        kprintf("\n-- snapshot %d --\n", it + 1);
        cmd_ps(0, NULL);
        uint32_t waited = 0;
        while (waited < 1000) {
            int c = kbd_getc();
            if (c > 0) return 0;
            tsc_delay_ms(10);
            waited += 10;
        }
    }
    return 0;
}

static int cmd_wait(int argc, char **argv)
{
    if (argc < 2) { kputs("usage: wait <tid>\n"); return 1; }
    uint32_t tid = (uint32_t)katoi(argv[1]);
    for (uint32_t i = 0; i < 500; i++) {
        sched_thread_t *t = sched_get_thread_by_tid(tid);
        if (!t || sched_thread_get_state(t) == SCHED_STATE_DEAD) {
            kprintf("wait: tid %u exited\n", tid);
            return 0;
        }
        tsc_delay_ms(10);
        sched_yield();
    }
    kprintf("wait: timeout waiting for tid %u\n", tid);
    return 1;
}

static int cmd_meminfo(int argc, char **argv)
{
    (void)argc; (void)argv;
    static const size_t szs[] = {16,32,64,128,256,512,1024,2048,4096};
    size_t total_bytes = 0, free_bytes = 0;
    int any = 0;
    kprintf("  cache   obj_sz    total    free    used\n");
    for (unsigned i = 0; i < 9; i++) {
        size_t tot, fr;
        kmalloc_stats(i, &tot, &fr);
        if (tot) any = 1;
        total_bytes += tot * szs[i];
        free_bytes  += fr  * szs[i];
        kprintf("  [%u]    %5lu   %6lu  %6lu  %6lu\n",
                i, (unsigned long)szs[i], (unsigned long)tot, (unsigned long)fr, (unsigned long)(tot - fr));
    }
    if (!any) kputs("  (kmalloc stats unavailable - check kmalloc_init linkage)\n");
    kprintf("  slab total : %lu KiB\n", (unsigned long)(total_bytes / 1024));
    kprintf("  slab free  : %lu KiB\n", (unsigned long)(free_bytes  / 1024));
    kprintf("  slab used  : %lu KiB\n", (unsigned long)((total_bytes - free_bytes) / 1024));
    uint32_t nn = numa_node_count();
    kprintf("  numa nodes : %u\n", nn);
    for (uint32_t i = 0; i < nn; i++) {
        uint64_t len = g_numa_nodes[i].mem_len;
        if (!len || len == ~(uint64_t)0) continue;
        kprintf("  node %u phys: %llu MiB\n", i,
                (unsigned long long)(len / (1024*1024)));
    }
    return 0;
}

static int cmd_vmstat(int argc, char **argv)
{
    (void)argc; (void)argv;
    size_t tot = 0, fr = 0;
    for (unsigned i = 0; i < 9; i++) {
        size_t t2, f2;
        kmalloc_stats(i, &t2, &f2);
        tot += t2; fr += f2;
    }
    uint64_t phys_mib = 0;
    uint32_t nn = numa_node_count();
    for (uint32_t i = 0; i < nn; i++) {
        uint64_t len = g_numa_nodes[i].mem_len;
        if (!len || len == ~(uint64_t)0) continue;
        phys_mib += len / (1024*1024);
    }
    uint32_t nr = 0, nb = 0;
    uint32_t n = sched_thread_count();
    for (uint32_t i = 0; i < n; i++) {
        sched_thread_t *t = sched_get_thread(i);
        if (!t) continue;
        uint32_t s = sched_thread_get_state(t);
        if (s == 0 || s == 1) nr++;
        else if (s == 2) nb++;
    }
    kprintf("  procs_run       : %u\n", nr);
    kprintf("  procs_blocked   : %u\n", nb);
    kprintf("  slab_objs_total : %lu\n", (unsigned long)tot);
    kprintf("  slab_objs_free  : %lu\n", (unsigned long)fr);
    kprintf("  slab_objs_used  : %lu\n", (unsigned long)(tot - fr));
    kprintf("  phys_total_mib  : %llu\n", (unsigned long long)phys_mib);
    kprintf("  numa_nodes      : %u\n", nn);
    return 0;
}

static int cmd_mslab(int argc, char **argv)
{
    (void)argc; (void)argv;
    static const size_t szs[] = {16,32,64,128,256,512,1024,2048,4096};
    int any = 0;
    kprintf("  idx  obj_sz  total   free   used   util%%\n");
    for (unsigned i = 0; i < 9; i++) {
        size_t tot, fr;
        kmalloc_stats(i, &tot, &fr);
        if (tot) any = 1;
        unsigned util = tot ? (unsigned)(((tot - fr) * 100u) / tot) : 0;
        kprintf("  [%u]  %5lu  %6lu  %5lu  %5lu   %3u%%\n",
                i, (unsigned long)szs[i], (unsigned long)tot, (unsigned long)fr, (unsigned long)(tot - fr), util);
    }
    if (!any) kputs("  (all zero - kmalloc stats not linked or allocator not warmed up)\n");
    return 0;
}

static int cmd_pmap(int argc, char **argv)
{
    if (argc < 2) { kputs("usage: pmap <tid>\n"); return 1; }
    uint32_t tid = (uint32_t)katoi(argv[1]);
    sched_thread_t *t = sched_get_thread_by_tid(tid);
    if (!t) { kprintf("pmap: tid %u not found\n", tid); return 1; }
    kprintf("pmap: tid %u  \"%s\"\n", tid,
            sched_thread_get_name(t) ? sched_thread_get_name(t) : "?");
    if (t->kstack_base) {
        kprintf("  [kstack]  0x%016llx - 0x%016llx  rw  %luK\n",
                (unsigned long long)(uintptr_t)t->kstack_base,
                (unsigned long long)((uintptr_t)t->kstack_base + t->kstack_size),
                (unsigned long)(t->kstack_size / 1024u));
    } else {
        kprintf("  [kstack]  (static / boot thread)\n");
    }
    kprintf("  [userspace] none (kernel-only thread)\n");
    return 0;
}

static int cmd_oom(int argc, char **argv)
{
    (void)argc; (void)argv;
    kputs("OOM killer: triggering reclaim\n");
    mreclaim_pressure();
    return 0;
}

static int cmd_numa(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint32_t n = numa_node_count();
    kprintf("  NUMA nodes: %u\n", n);
    for (uint32_t i = 0; i < n; i++) {
        uint64_t base = g_numa_nodes[i].mem_base;
        uint64_t len  = g_numa_nodes[i].mem_len;
        if (!len || len == ~(uint64_t)0) {
            kprintf("  node %u: (no memory info)\n", i);
            continue;
        }
        kprintf("  node %u: base=0x%016llx  len=0x%016llx  (%llu MiB)\n",
                i,
                (unsigned long long)base,
                (unsigned long long)len,
                (unsigned long long)(len / (1024*1024)));
    }
    return 0;
}

static int cmd_mount(int argc, char **argv)
{
    if (argc >= 4) {
        int rc = vfs_do_mount(argv[1], argv[2], argv[3]);
        if (rc < 0) kprintf("mount: failed (err %d)\n", rc);
        else kprintf("mount: %s at %s (%s)\n", argv[1], argv[2], argv[3]);
        return rc < 0 ? 1 : 0;
    }
    vfs_print_mounts();
    kputs("  registered: flatfs tmpfs devfs procfs sysfs fatfs\n");
    return 0;
}

static int cmd_ls(int argc, char **argv)
{
    const char *path = argc >= 2 ? argv[1] : "/";
    vfs_dirent_t de;
    int found = 0;
    for (uint64_t i = 0; ; i++) {
        int rc = vfs_readdir(path, i, &de);
        if (rc < 0) break;
        const char *t = de.type == VFS_DT_DIR ? "/" :
                        de.type == VFS_DT_LNK ? "@" : "";
        kprintf("  %s%s\n", de.name, t);
        found++;
    }
    if (!found) kputs("  (empty or path not found)\n");
    return 0;
}

static int cmd_cat(int argc, char **argv)
{
    if (argc < 2) { kputs("usage: cat <path>\n"); return 1; }
    int fd = vfs_open(argv[1], VFS_O_RDONLY, 0);
    if (fd < 0) { kprintf("cat: cannot open %s (err %d)\n", argv[1], fd); return 1; }
    char buf[256];
    int n;
    while ((n = vfs_read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        kputs(buf);
    }
    vfs_close(fd);
    kputs("\n");
    return 0;
}

static int cmd_write(int argc, char **argv)
{
    if (argc < 3) { kputs("usage: write <path> <data>\n"); return 1; }
    int fd = vfs_open(argv[1], VFS_O_WRONLY | VFS_O_CREAT | VFS_O_TRUNC, 0644);
    if (fd < 0) { kprintf("write: cannot open %s (err %d)\n", argv[1], fd); return 1; }
    size_t dlen = strlen(argv[2]);
    int n = vfs_write(fd, argv[2], dlen);
    vfs_close(fd);
    if (n < 0) { kprintf("write: error %d\n", n); return 1; }
    kprintf("wrote %d bytes to %s\n", (int)n, argv[1]);
    return 0;
}

static int cmd_stat(int argc, char **argv)
{
    if (argc < 2) { kputs("usage: stat <path>\n"); return 1; }
    vfs_stat_t st;
    int rc = vfs_stat(argv[1], &st);
    if (rc < 0) { kprintf("stat: %s: error %d\n", argv[1], rc); return 1; }
    kprintf("  file:  %s\n", argv[1]);
    kprintf("  size:  %llu\n", (unsigned long long)st.size);
    kprintf("  mode:  %04o\n", st.mode);
    kprintf("  inode: %llu\n", (unsigned long long)st.ino);
    return 0;
}

static int cmd_file(int argc, char **argv)
{
    if (argc < 2) { kputs("usage: file <path>\n"); return 1; }
    vfs_stat_t st;
    int rc = vfs_stat(argv[1], &st);
    if (rc < 0) { kprintf("file: %s not found (err %d)\n", argv[1], rc); return 1; }
    const char *typ = (st.mode & 0xF000) == 0x4000 ? "directory" :
                      (st.mode & 0xF000) == 0xA000 ? "symlink"   : "regular file";
    kprintf("%s: %s  ino=%llu  size=%llu  mode=%04o\n",
            argv[1], typ,
            (unsigned long long)st.ino,
            (unsigned long long)st.size,
            st.mode & 0xFFF);
    return 0;
}

static int cmd_sync(int argc, char **argv)
{
    (void)argc; (void)argv;
    vfs_flush_all();
    kputs("sync done\n");
    return 0;
}

static int cmd_truncate(int argc, char **argv)
{
    if (argc < 3) { kputs("usage: truncate <path> <size>\n"); return 1; }
    uint64_t sz = 0;
    for (const char *p = argv[2]; *p >= '0' && *p <= '9'; p++) sz = sz*10+(*p-'0');
    int fd = vfs_open(argv[1], VFS_O_WRONLY, 0);
    if (fd < 0) { kprintf("truncate: %s not found\n", argv[1]); return 1; }
    vfs_truncate(fd, sz);
    vfs_close(fd);
    return 0;
}

static int cmd_rm(int argc, char **argv)
{
    if (argc < 2) { kputs("usage: rm <path>\n"); return 1; }
    int rc = vfs_unlink(argv[1]);
    if (rc < 0) kprintf("rm: %s failed (err %d)\n", argv[1], rc);
    return rc < 0 ? 1 : 0;
}

static int cmd_mkdir(int argc, char **argv)
{
    if (argc < 2) { kputs("usage: mkdir <path>\n"); return 1; }
    int rc = vfs_mkdir(argv[1], 0755);
    if (rc == -38) {
        kprintf("mkdir: %s: filesystem does not support mkdir\n", argv[1]);
    } else if (rc < 0) {
        kprintf("mkdir: %s failed (err %d)\n", argv[1], rc);
    } else {
        kprintf("mkdir: %s created\n", argv[1]);
    }
    return rc < 0 ? 1 : 0;
}

static int cmd_fsck(int argc, char **argv)
{
    int idx = 0;
    if (argc >= 2) idx = katoi(argv[1]);
    blkdev_t *dev = blkdev_get((unsigned)idx);
    if (!dev) { kprintf("fsck: blkdev %d not found\n", idx); return 1; }
    uint8_t sec[512];
    if (blkdev_read(dev, 0, 1, sec) < 0) { kputs("fsck: sector read error\n"); return 1; }
    kprintf("fsck: blkdev %d (%s)\n", idx, dev->name);
    if (sec[510] == 0x55 && sec[511] == 0xAA) {
        kputs("  MBR signature valid (0x55AA)\n");
        kputs("  no filesystem-level check available in kernel\n");
        kputs("  status: OK\n");
    } else {
        kprintf("  MBR signature absent (bytes: 0x%02x 0x%02x)\n", sec[510], sec[511]);
        kputs("  disk may be unpartitioned, GPT, or raw image\n");
        kputs("  status: no MBR\n");
    }
    return 0;
}

static int cmd_mkfs(int argc, char **argv)
{
    if (argc < 2) { kputs("usage: mkfs <vol_idx>\n"); return 1; }
    int vol = katoi(argv[1]);
    if (vol < 0 || vol >= FATFS_KOBALT_MAX_VOLS) {
        kprintf("mkfs: invalid volume %d\n", vol); return 1;
    }
    if (!blkdev_get((unsigned)vol)) {
        kprintf("mkfs: no blkdev %d\n", vol); return 1;
    }
    char path[4] = { (char)('0'+vol), ':', '/', '\0' };
    MKFS_PARM opt;
    __builtin_memset(&opt, 0, sizeof(opt));
    opt.fmt = FM_FAT32;
    static uint8_t work[4096];
    kprintf("mkfs: formatting blkdev %d as FAT32...\n", vol);
    FRESULT fr = f_mkfs(path, &opt, work, sizeof(work));
    if (fr != FR_OK) { kprintf("mkfs: f_mkfs failed (fr=%d)\n", (int)fr); return 1; }
    kprintf("mkfs: blkdev %d formatted as FAT32\n", vol);
    return 0;
}

static int cmd_blkdump(int argc, char **argv)
{
    if (argc < 3) { kputs("usage: blkdump <dev> <lba>\n"); return 1; }
    unsigned int dev = 0; unsigned long long lba = 0;
    for (const char *p = argv[1]; *p >= '0' && *p <= '9'; p++) dev = dev*10+(*p-'0');
    for (const char *p = argv[2]; *p >= '0' && *p <= '9'; p++) lba = lba*10+(*p-'0');
    blkdev_t *bd = blkdev_get(dev);
    if (!bd) { kputs("blkdump: invalid dev\n"); return 1; }
    static uint8_t buf[512] __attribute__((aligned(512)));
    if (blkdev_read(bd, lba, 1, buf) != 0) { kputs("blkdump: read failed\n"); return 1; }
    static const char hx[] = "0123456789abcdef";
    char line[64];
    for (uint32_t off = 0; off < 512; off += 16) {
        uint32_t pos = 0;
        line[pos++] = hx[(off>>12)&0xF]; line[pos++] = hx[(off>>8)&0xF];
        line[pos++] = hx[(off>>4)&0xF];  line[pos++] = hx[off&0xF];
        line[pos++] = ':'; line[pos++] = ' ';
        for (uint32_t k = 0; k < 16; k++) {
            uint8_t b = buf[off+k];
            line[pos++] = hx[b>>4]; line[pos++] = hx[b&0xF]; line[pos++] = ' ';
        }
        line[pos++] = '\n'; line[pos] = '\0';
        kputs(line);
    }
    return 0;
}

static int cmd_blkdev(int argc, char **argv)
{
    (void)argc; (void)argv;
    unsigned n = blkdev_count();
    if (!n) { kputs("  no block devices registered\n"); return 0; }
    kprintf("  idx  name              sectors       sz   total\n");
    for (unsigned i = 0; i < n; i++) {
        blkdev_t *d = blkdev_get(i);
        if (!d) continue;
        uint64_t mib = d->num_sectors * d->sector_size / (1024 * 1024);
        kprintf("  [%u]  %-16s  %10llu  %4u   %llu MiB\n",
                i, d->name,
                (unsigned long long)d->num_sectors,
                d->sector_size,
                (unsigned long long)mib);
    }
    return 0;
}

static int cmd_lsblk(int argc, char **argv) { return cmd_blkdev(argc, argv); }

static int cmd_partinfo(int argc, char **argv)
{
    int idx = 0;
    if (argc >= 2) idx = katoi(argv[1]);
    blkdev_t *dev = blkdev_get((unsigned)idx);
    if (!dev) { kprintf("partinfo: blkdev %d not found\n", idx); return 1; }
    uint8_t mbr[512];
    if (blkdev_read(dev, 0, 1, mbr) < 0) {
        kprintf("partinfo: read error on blkdev %d\n", idx); return 1;
    }
    kprintf("blkdev %d: %s  %llu sectors x %u bytes = %llu MiB\n",
            idx, dev->name,
            (unsigned long long)dev->num_sectors,
            dev->sector_size,
            (unsigned long long)(dev->num_sectors * dev->sector_size / (1024*1024)));
    if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
        kprintf("  no MBR signature (got 0x%02x 0x%02x at offset 510)\n",
                mbr[510], mbr[511]);
        kputs("  disk may be unpartitioned, GPT, or raw\n");
        return 0;
    }
    kputs("  MBR signature valid (0x55AA)\n");
    kprintf("  %-4s  %-8s  %-8s  %-8s  type\n", "#", "start", "size", "end");
    int found = 0;
    for (int p = 0; p < 4; p++) {
        uint8_t *e = mbr + 446 + p * 16;
        if (e[4] == 0) continue;
        uint32_t lba_start = (uint32_t)e[8]  | ((uint32_t)e[9]  << 8) |
                             ((uint32_t)e[10] << 16) | ((uint32_t)e[11] << 24);
        uint32_t lba_size  = (uint32_t)e[12] | ((uint32_t)e[13] << 8) |
                             ((uint32_t)e[14] << 16) | ((uint32_t)e[15] << 24);
        kprintf("  [%d]  %8u  %8u  %8u  0x%02x  %s\n",
                p, lba_start, lba_size, lba_start + lba_size, e[4],
                e[0] & 0x80 ? "bootable" : "");
        found++;
    }
    if (!found) kputs("  no partitions in MBR table\n");
    return 0;
}

static int cmd_cpuinfo(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint32_t a, b, c, d;
    char vendor[13];
    cpuid_ex(0, 0, &a, &b, &c, &d);
    uint32_t max_leaf = a;
    __builtin_memcpy(vendor + 0, &b, 4);
    __builtin_memcpy(vendor + 4, &d, 4);
    __builtin_memcpy(vendor + 8, &c, 4);
    vendor[12] = '\0';

    char brand[49] = {0};
    for (int i = 0; i < 3; i++) {
        uint32_t ba, bb, bc, bd;
        cpuid_ex(0x80000002 + (uint32_t)i, 0, &ba, &bb, &bc, &bd);
        __builtin_memcpy(brand + i*16 + 0,  &ba, 4);
        __builtin_memcpy(brand + i*16 + 4,  &bb, 4);
        __builtin_memcpy(brand + i*16 + 8,  &bc, 4);
        __builtin_memcpy(brand + i*16 + 12, &bd, 4);
    }

    cpuid_ex(1, 0, &a, &b, &c, &d);
    uint32_t stepping = a & 0xF;
    uint32_t model    = ((a >> 4) & 0xF) | (((a >> 16) & 0xF) << 4);
    uint32_t family   = ((a >> 8) & 0xF) + ((a >> 20) & 0xFF);
    uint32_t ncores   = (b >> 16) & 0xFF;

    kprintf("  vendor  : %s\n", vendor);
    kprintf("  brand   : %s\n", brand);
    kprintf("  family  : %u  model %u  stepping %u\n", family, model, stepping);
    kprintf("  cpus    : %u online\n", smp_cpu_count());
    kprintf("  cores   : %u logical (cpuid)\n", ncores);
    kprintf("  max_leaf: 0x%x\n", max_leaf);

    uint64_t cr4 = read_cr4();
    kprintf("  SMEP    : %s\n", cr4 & (1u << 20) ? "on" : "off");
    kprintf("  SMAP    : %s\n", cr4 & (1u << 21) ? "on" : "off");
    kprintf("  CR0.WP  : %s\n", read_cr0() & (1u << 16) ? "on" : "off");

    if (tsc_khz)
        kprintf("  tsc     : %llu kHz (%llu MHz)\n",
                (unsigned long long)tsc_khz,
                (unsigned long long)(tsc_khz / 1000));

    for (unsigned i = 0; i < smp_cpu_count(); i++) {
        uint32_t khz = cpufreq_get_khz(i);
        if (khz)
            kprintf("  cpu%u    : %u.%03u MHz  gov=%s\n",
                    i, khz / 1000, khz % 1000,
                    cpufreq_gov_name(cpufreq_get_gov()));
        else if (tsc_khz)
            kprintf("  cpu%u    : ~%llu MHz (from TSC; cpufreq unavailable)\n",
                    i, (unsigned long long)(tsc_khz / 1000));
        else
            kprintf("  cpu%u    : freq unknown\n", i);
    }
    return 0;
}

static int cmd_cpuid(int argc, char **argv)
{
    uint32_t leaf = 0, a, b, c, d;
    if (argc >= 2) {
        const char *p = argv[1];
        if (p[0]=='0'&&(p[1]=='x'||p[1]=='X')) p+=2;
        for (; *p; p++) {
            leaf <<= 4;
            if (*p>='0'&&*p<='9') leaf|=*p-'0';
            else if (*p>='a'&&*p<='f') leaf|=*p-'a'+10;
            else if (*p>='A'&&*p<='F') leaf|=*p-'A'+10;
        }
    }
    cpuid_ex(leaf, 0, &a, &b, &c, &d);
    kprintf("CPUID leaf 0x%08x: eax=%08x ebx=%08x ecx=%08x edx=%08x\n",
            leaf, a, b, c, d);
    return 0;
}

static int cmd_msr(int argc, char **argv)
{
    if (argc < 2) {
        kputs("usage: msr <hex_addr> [value]\n");
        kputs("  common: 0x1B=APIC_BASE  0x10a=ARCH_CAPS  0x48=SPEC_CTRL  0xC0000080=EFER\n");
        return 1;
    }
    uint32_t addr = 0;
    const char *s = argv[1];
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    while (*s) {
        addr <<= 4;
        if (*s >= '0' && *s <= '9') addr |= (uint32_t)(*s - '0');
        else if (*s >= 'a' && *s <= 'f') addr |= (uint32_t)(*s - 'a' + 10);
        else if (*s >= 'A' && *s <= 'F') addr |= (uint32_t)(*s - 'A' + 10);
        s++;
    }
    if (argc >= 3) {
        uint64_t val = 0;
        const char *v = argv[2];
        if (v[0] == '0' && (v[1] == 'x' || v[1] == 'X')) v += 2;
        while (*v) {
            val <<= 4;
            if (*v >= '0' && *v <= '9') val |= (uint64_t)(*v - '0');
            else if (*v >= 'a' && *v <= 'f') val |= (uint64_t)(*v - 'a' + 10);
            else if (*v >= 'A' && *v <= 'F') val |= (uint64_t)(*v - 'A' + 10);
            v++;
        }
        wrmsr64(addr, val);
        kprintf("msr 0x%08x <- 0x%016llx\n", addr, (unsigned long long)val);
    } else {
        uint64_t val = rdmsr64(addr);
        kprintf("msr 0x%08x = 0x%016llx\n", addr, (unsigned long long)val);
    }
    return 0;
}

static int cmd_rdtsc(int argc, char **argv)
{
    (void)argc; (void)argv;
    kprintf("TSC: %llu\n", (unsigned long long)rdtsc_now());
    return 0;
}

static int cmd_x2apic(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint64_t apic_base = rdmsr64(0x1B);
    kprintf("IA32_APIC_BASE: 0x%llx\n", (unsigned long long)apic_base);
    kprintf("x2APIC %s\n", (apic_base & (1<<10)) ? "enabled" : "disabled");
    kprintf("BSP: %s\n",   (apic_base & (1<<8))  ? "yes" : "no");
    return 0;
}

static int cmd_apictimer(int argc, char **argv)
{
    (void)argc; (void)argv;
    kprintf("APIC timer freq: %u Hz\n", (uint32_t)(apic_ticks_per_ms * 1000UL));
    kprintf("ticks so far:    %llu\n", (unsigned long long)apic_timer_ticks());
    return 0;
}

static int cmd_tsc(int argc, char **argv)
{
    (void)argc; (void)argv;
    kprintf("TSC freq: %llu kHz (%llu MHz)\n",
            (unsigned long long)tsc_khz,
            (unsigned long long)(tsc_khz / 1000));
    kprintf("TSC now:  %llu\n", (unsigned long long)rdtsc_now());
    return 0;
}

static int cmd_ioapic(int argc, char **argv)
{
    (void)argc; (void)argv;
    volatile uint32_t *ioregsel = (volatile uint32_t *)0xFEC00000UL;
    volatile uint32_t *iowin    = (volatile uint32_t *)0xFEC00010UL;

    *ioregsel = 0x00;
    uint32_t id  = (*iowin >> 24) & 0xF;
    *ioregsel = 0x01;
    uint32_t ver = *iowin;
    uint32_t version   = ver & 0xFF;
    uint32_t max_redir = (ver >> 16) & 0xFF;

    kprintf("  IOAPIC @ 0xFEC00000\n");
    kprintf("  ID      : %u\n", id);
    kprintf("  version : 0x%02x\n", version);
    kprintf("  inputs  : %u (0..%u)\n", max_redir + 1, max_redir);
    kprintf("  IRQ  VEC  DST  MASK  TRIG   POL   DEST\n");
    for (uint32_t i = 0; i <= max_redir; i++) {
        *ioregsel = 0x10 + i * 2;
        uint32_t lo = *iowin;
        *ioregsel = 0x11 + i * 2;
        uint32_t hi = *iowin;
        uint8_t vec  = lo & 0xFF;
        uint8_t mask = (lo >> 16) & 1;
        uint8_t trig = (lo >> 15) & 1;
        uint8_t pol  = (lo >> 13) & 1;
        uint8_t dst  = (hi >> 24) & 0xFF;
        if (vec == 0 && mask) continue;
        kprintf("  %3u  %3u  %3u     %u  %-5s  %-4s  %s\n",
                i, vec, dst, mask,
                trig ? "level" : "edge",
                pol  ? "low"   : "high",
                (lo >> 11) & 1 ? "logical" : "physical");
    }
    return 0;
}

static int cmd_pci(int argc, char **argv)
{
    (void)argc; (void)argv;
    pci_list_devices();
    return 0;
}

static int cmd_acpi(int argc, char **argv)
{
    if (argc >= 2) {
        void *t = acpi_find_table(argv[1]);
        kprintf("ACPI table '%s': %s\n", argv[1], t ? "found" : "not found");
        return 0;
    }
    acpi_print_tables();
    return 0;
}

static int cmd_uname(int argc, char **argv)
{
    int all = argc >= 2 && argv[1][0] == '-' && argv[1][1] == 'a';
    if (all) {
        char dtbuf[20];
        rtc_time_t t; rtc_read(&t);
        rtc_fmt(&t, dtbuf, sizeof(dtbuf));
        kprintf("%s %s %s %s %s UTC %s\n",
                KOBALT_OS, KOBALT_HOSTNAME, KOBALT_VERSION,
                KOBALT_BUILD, dtbuf, KOBALT_ARCH);
    } else {
        kprintf("%s\n", KOBALT_OS);
    }
    return 0;
}

static int cmd_amx(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (!g_amx_supported) { kputs("AMX not supported on this CPU\n"); return 1; }
    kputs("AMX: supported\n");
    amx_run_tests();
    return 0;
}

static int cmd_irq(int argc, char **argv)
{
    (void)argc; (void)argv;
    irq_print_stats();
    kprintf("  spurious: %llu\n", (unsigned long long)irq_spurious_count());
    kputs("  per-vector counters: see ioapic for routing\n");
    return 0;
}

static int cmd_ipi(int argc, char **argv)
{
    if (argc < 2) { kputs("usage: ipi <cpu>\n"); return 1; }
    uint32_t cpu = 0;
    for (const char *p = argv[1]; *p >= '0' && *p <= '9'; p++) cpu = cpu*10+(*p-'0');
    ipi_send_to_cpu(cpu, IPI_VECTOR_RESCHED);
    kprintf("IPI sent to cpu %u\n", cpu);
    return 0;
}

static int cmd_nmi(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint32_t n = smp_cpu_count();
    int any = 0;
    for (uint32_t i = 0; i < n; i++) {
        percpu_t *p = percpu_get_by_id(i);
        if (!p) continue;
        kprintf("  cpu%u: nmi_count=%u\n", i, p->nmi_count);
        any = 1;
    }
    if (!any) kputs("  (percpu_get_by_id not available; NMI counts inaccessible)\n");
    return 0;
}

static int cmd_except(int argc, char **argv)
{
    (void)argc; (void)argv;
    exception_print_stats();
    kputs("  fatal exceptions print to console and halt\n");
    return 0;
}

static int cmd_cpu(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint32_t n = smp_cpu_count();
    int any = 0;
    for (uint32_t i = 0; i < n; i++) {
        percpu_t *pc = percpu_get_by_id(i);
        if (!pc) continue;
        uint64_t tot = pc->idle_ticks + pc->sched_ticks + 1;
        kprintf("  cpu%u  apic=%u  idle=%llu%%  sched_ticks=%llu\n",
                i, pc->apic_id,
                (unsigned long long)(pc->idle_ticks * 100ULL / tot),
                (unsigned long long)pc->sched_ticks);
        any = 1;
    }
    if (!any) {
        kprintf("  %u cpu(s) online (percpu struct unavailable via percpu_get_by_id)\n", n);
    }
    return 0;
}

static int cmd_migrate(int argc, char **argv)
{
    if (argc < 3) { kputs("usage: migrate <tid> <cpu>\n"); return 1; }
    uint32_t tid = (uint32_t)katoi(argv[1]);
    uint32_t cpu = (uint32_t)katoi(argv[2]);
    sched_thread_t *t = sched_get_thread_by_tid(tid);
    if (!t) { kprintf("migrate: tid %u not found\n", tid); return 1; }
    if (cpu >= smp_cpu_count()) { kprintf("migrate: cpu %u out of range\n", cpu); return 1; }
    sched_migrate(t, cpu);
    kprintf("migrate: tid %u -> cpu %u\n", tid, cpu);
    return 0;
}

static int cmd_affinity(int argc, char **argv)
{
    if (argc < 3) { kputs("usage: affinity <tid> <cpu_mask_hex>\n"); return 1; }
    uint32_t tid = (uint32_t)katoi(argv[1]);
    sched_thread_t *t = sched_get_thread_by_tid(tid);
    if (!t) { kprintf("affinity: tid %u not found\n", tid); return 1; }
    uint64_t mask = 0;
    const char *p = argv[2];
    if (p[0]=='0'&&(p[1]=='x'||p[1]=='X')) p+=2;
    for (; *p; p++) {
        mask <<= 4;
        if (*p>='0'&&*p<='9') mask|=*p-'0';
        else if (*p>='a'&&*p<='f') mask|=*p-'a'+10;
        else if (*p>='A'&&*p<='F') mask|=*p-'A'+10;
    }
    sched_set_affinity(t, mask);
    kprintf("affinity: tid %u mask 0x%llx set\n", tid, (unsigned long long)mask);
    return 0;
}

static int cmd_tlbshoot(int argc, char **argv)
{
    (void)argc; (void)argv;
    tlb_flush_all();
    kputs("TLB shootdown done\n");
    return 0;
}

static int cmd_tykid(int argc, char **argv)
{
    tykid_gate_ctx_t *ctx = tykid_kobalt_get_ctx();
    if (!ctx) { kputs("tykid: not initialized\n"); return 1; }
    if (argc >= 2 && argv[1][0]=='s' && argv[1][1]=='t') {
        char buf[2048];
        tykid_dump_state(ctx, buf, sizeof(buf));
        kputs(buf);
        return 0;
    }
    if (argc >= 3 && argv[1][0]=='c' && argv[1][1]=='h') {
        kprintf("tykid: %s -> %s\n", argv[2],
                tykid_kobalt_builtin_approved(ctx, argv[2]) ? "approved" : "denied");
        return 0;
    }
    kputs("usage: tykid status | check <driver>\n");
    return 1;
}

static int cmd_ifconfig(int argc, char **argv)
{
    if (argc >= 2 && argv[1][0]=='u' && argv[1][1]=='p') {
        netif_set_up(&kobalt_netif); kputs("interface up\n"); return 0;
    }
    if (argc >= 2 && argv[1][0]=='d' && argv[1][1]=='o') {
        netif_set_down(&kobalt_netif); kputs("interface down\n"); return 0;
    }
    uint8_t *mac = kobalt_netif.hwaddr;
    kprintf("eth0  link/ether %02x:%02x:%02x:%02x:%02x:%02x\n",
            mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    kprintf("      inet %s", ip4addr_ntoa(netif_ip4_addr(&kobalt_netif)));
    kprintf("  netmask %s", ip4addr_ntoa(netif_ip4_netmask(&kobalt_netif)));
    kprintf("  gw %s\n",    ip4addr_ntoa(netif_ip4_gw(&kobalt_netif)));
    kprintf("      MTU %u  %s\n", kobalt_netif.mtu,
            netif_is_up(&kobalt_netif) ? "UP" : "DOWN");
    return 0;
}

static u8_t ping_recv_cb(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr)
{
    (void)arg; (void)pcb;
    struct ip_hdr *iph = (struct ip_hdr *)p->payload;
    u16_t ihl = IPH_HL_BYTES(iph);
    if (p->tot_len >= (u16_t)(ihl+8)) {
        uint8_t *icmp = (uint8_t *)p->payload + ihl;
        if (icmp[0] == 0) {
            g_ping_received = 1;
            kprintf("\n  pong from %d.%d.%d.%d\n",
                    ip4_addr1(ip_2_ip4(addr)), ip4_addr2(ip_2_ip4(addr)),
                    ip4_addr3(ip_2_ip4(addr)), ip4_addr4(ip_2_ip4(addr)));
        }
    }
    pbuf_free(p); return 1;
}

static int cmd_ping(int argc, char **argv)
{
    if (argc < 2) { kputs("usage: ping <ip>\n"); return 1; }
    ip4_addr_t target;
    if (!ip4addr_aton(argv[1], &target)) { kprintf("ping: invalid IP '%s'\n", argv[1]); return 1; }
    struct raw_pcb *pcb = raw_new(IP_PROTO_ICMP);
    if (!pcb) return 1;
    g_ping_received = 0;
    raw_recv(pcb, ping_recv_cb, NULL);
    struct pbuf *p = pbuf_alloc(PBUF_IP, sizeof(struct icmp_echo_hdr), PBUF_RAM);
    if (!p) { raw_remove(pcb); return 1; }
    memset(p->payload, 0, p->len);
    struct icmp_echo_hdr *iecho = (struct icmp_echo_hdr *)p->payload;
    ICMPH_TYPE_SET(iecho, ICMP_ECHO);
    iecho->id = lwip_htons(0x4b42); iecho->seqno = lwip_htons(1);
    iecho->chksum = 0; iecho->chksum = inet_chksum(iecho, p->len);
    kprintf("pinging %s...\n", argv[1]);
    raw_sendto(pcb, p, (ip_addr_t*)&target);
    pbuf_free(p);
    uint32_t waited = 0;
    while (waited < 3000 && !g_ping_received) {
        net_poll(); sys_check_timeouts();
        tsc_delay_ms(10);
        waited += 10;
    }
    if (!g_ping_received) kputs("  request timeout\n");
    raw_remove(pcb);
    return 0;
}

static int cmd_arp(int argc, char **argv)
{
    (void)argc; (void)argv;
    kputs("  IP Address      MAC Address           Type\n");
    kputs("  --------------  --------------------  ------\n");
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        ip4_addr_t *ipret; struct netif *nifret; struct eth_addr *ethret;
        if (etharp_get_entry(i, &ipret, &nifret, &ethret))
            kprintf("  %-14s  %02x:%02x:%02x:%02x:%02x:%02x  Dynamic\n",
                    ip4addr_ntoa(ipret),
                    ethret->addr[0], ethret->addr[1], ethret->addr[2],
                    ethret->addr[3], ethret->addr[4], ethret->addr[5]);
    }
    return 0;
}

static int cmd_route(int argc, char **argv)
{
    (void)argc; (void)argv;
    kputs("  Destination     Gateway         Genmask         Iface\n");
    kputs("  0.0.0.0         ");
    kputs(ip4addr_ntoa(netif_ip4_gw(&kobalt_netif)));
    kputs("        0.0.0.0         eth0\n");
    kprintf("  %-15s ", ip4addr_ntoa(netif_ip4_addr(&kobalt_netif)));
    kprintf(" 0.0.0.0         ");
    kprintf("%-15s eth0\n", ip4addr_ntoa(netif_ip4_netmask(&kobalt_netif)));
    return 0;
}

#ifndef LWIP_TCP_PRIV_H
struct tcp_pcb;
struct tcp_pcb_listen;
extern struct tcp_pcb        *tcp_active_pcbs;
extern struct tcp_pcb        *tcp_tw_pcbs;
extern union tcp_listen_pcbs_t {
    struct tcp_pcb_listen *listen_pcbs;
    struct tcp_pcb        *pcbs;
} tcp_listen_pcbs;
#endif

static const char *tcp_state_name(enum tcp_state s)
{
    switch (s) {
        case CLOSED:      return "CLOSED";
        case LISTEN:      return "LISTEN";
        case SYN_SENT:    return "SYN_SENT";
        case SYN_RCVD:    return "SYN_RCVD";
        case ESTABLISHED: return "ESTABLISHED";
        case FIN_WAIT_1:  return "FIN_WAIT_1";
        case FIN_WAIT_2:  return "FIN_WAIT_2";
        case CLOSE_WAIT:  return "CLOSE_WAIT";
        case CLOSING:     return "CLOSING";
        case LAST_ACK:    return "LAST_ACK";
        case TIME_WAIT:   return "TIME_WAIT";
        default:          return "UNKNOWN";
    }
}

static int cmd_netstat(int argc, char **argv)
{
    (void)argc; (void)argv;
    char lbuf[22], rbuf[22];
    int found = 0;
    kputs("  Proto  Local Addr          Foreign Addr        State\n");
    kputs("  -----  ------------------  ------------------  -----\n");
    for (struct tcp_pcb *p = tcp_active_pcbs; p; p = p->next) {
        ksnprintf(lbuf, sizeof(lbuf), "%s:%u",
                  ip4addr_ntoa(ip_2_ip4(&p->local_ip)), p->local_port);
        ksnprintf(rbuf, sizeof(rbuf), "%s:%u",
                  ip4addr_ntoa(ip_2_ip4(&p->remote_ip)), p->remote_port);
        kprintf("  tcp    %-20s  %-20s  %s\n", lbuf, rbuf, tcp_state_name(p->state));
        found++;
    }
    for (struct tcp_pcb_listen *p = tcp_listen_pcbs.listen_pcbs; p; p = p->next) {
        ksnprintf(lbuf, sizeof(lbuf), "%s:%u",
                  ip4addr_ntoa(ip_2_ip4(&p->local_ip)), p->local_port);
        kprintf("  tcp    %-20s  %-20s  LISTEN\n", lbuf, "0.0.0.0:0");
        found++;
    }
    for (struct tcp_pcb *p = tcp_tw_pcbs; p; p = p->next) {
        ksnprintf(lbuf, sizeof(lbuf), "%s:%u",
                  ip4addr_ntoa(ip_2_ip4(&p->local_ip)), p->local_port);
        ksnprintf(rbuf, sizeof(rbuf), "%s:%u",
                  ip4addr_ntoa(ip_2_ip4(&p->remote_ip)), p->remote_port);
        kprintf("  tcp    %-20s  %-20s  TIME_WAIT\n", lbuf, rbuf);
        found++;
    }
    for (struct udp_pcb *p = udp_pcbs; p; p = p->next) {
        ksnprintf(lbuf, sizeof(lbuf), "%s:%u",
                  ip4addr_ntoa(ip_2_ip4(&p->local_ip)), p->local_port);
        ksnprintf(rbuf, sizeof(rbuf), "%s:%u",
                  ip4addr_ntoa(ip_2_ip4(&p->remote_ip)), p->remote_port);
        kprintf("  udp    %-20s  %-20s  -\n", lbuf, rbuf);
        found++;
    }
    if (!found) kputs("  (no active connections)\n");
    return 0;
}

static volatile int g_capture_count  = 0;
static volatile int g_capture_target = 0;

static u8_t tcpdump_raw_cb(void *arg, struct raw_pcb *pcb,
                            struct pbuf *p, const ip_addr_t *addr)
{
    (void)arg; (void)pcb;
    if (g_capture_count >= g_capture_target) return 0;
    struct ip_hdr *iph = (struct ip_hdr *)p->payload;
    uint8_t  proto = IPH_PROTO(iph);
    uint16_t tlen  = lwip_ntohs(IPH_LEN(iph));
    const char *prname = (proto == IP_PROTO_TCP)  ? "TCP"  :
                         (proto == IP_PROTO_UDP)  ? "UDP"  :
                         (proto == IP_PROTO_ICMP) ? "ICMP" : "IP";
    kprintf("  [%3d] %-6s %s -> %s  len=%u\n",
            g_capture_count + 1,
            prname,
            ip4addr_ntoa(ip_2_ip4(addr)),
            ip4addr_ntoa(ip_2_ip4(&kobalt_netif.ip_addr)),
            (unsigned)tlen);
    g_capture_count++;
    return 1;
}

static int cmd_tcpdump(int argc, char **argv)
{
    int n = 10;
    if (argc >= 2) {
        n = 0;
        for (const char *p = argv[1]; *p >= '0' && *p <= '9'; p++) n = n*10+(*p-'0');
    }
    struct raw_pcb *pcb_icmp = raw_new_ip_type(IPADDR_TYPE_ANY, IP_PROTO_ICMP);
    struct raw_pcb *pcb_tcp  = raw_new_ip_type(IPADDR_TYPE_ANY, IP_PROTO_TCP);
    struct raw_pcb *pcb_udp  = raw_new_ip_type(IPADDR_TYPE_ANY, IP_PROTO_UDP);
    if (!pcb_icmp && !pcb_tcp && !pcb_udp) { kputs("tcpdump: out of PCBs\n"); return 1; }
    g_capture_count  = 0;
    g_capture_target = n;
    if (pcb_icmp) { raw_recv(pcb_icmp, tcpdump_raw_cb, NULL); raw_bind(pcb_icmp, IP_ADDR_ANY); }
    if (pcb_tcp)  { raw_recv(pcb_tcp,  tcpdump_raw_cb, NULL); raw_bind(pcb_tcp,  IP_ADDR_ANY); }
    if (pcb_udp)  { raw_recv(pcb_udp,  tcpdump_raw_cb, NULL); raw_bind(pcb_udp,  IP_ADDR_ANY); }
    kprintf("tcpdump: capturing %d packets (30s timeout)...\n", n);
    uint32_t waited = 0;
    while (g_capture_count < g_capture_target && waited < 30000u) {
        net_poll(); sys_check_timeouts();
        tsc_delay_ms(10);
        waited += 10;
    }
    if (pcb_icmp) raw_remove(pcb_icmp);
    if (pcb_tcp)  raw_remove(pcb_tcp);
    if (pcb_udp)  raw_remove(pcb_udp);
    kprintf("tcpdump: captured %d packet(s)\n", g_capture_count);
    return 0;
}

static volatile int g_nc_connected = 0;
static volatile int g_nc_done      = 0;
static struct tcp_pcb *g_nc_pcb    = NULL;

static err_t nc_connected_cb(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    (void)arg;
    if (err != ERR_OK) {
        kprintf("nc: connect error %d\n", (int)err);
        g_nc_done = 1;
        return err;
    }
    g_nc_connected = 1;
    kprintf("nc: connected to %s:%u\n",
            ip4addr_ntoa(ip_2_ip4(&tpcb->remote_ip)), tpcb->remote_port);
    tcp_close(tpcb);
    g_nc_pcb  = NULL;
    g_nc_done = 1;
    return ERR_OK;
}

static void nc_err_cb(void *arg, err_t err)
{
    (void)arg; (void)err;
    kputs("nc: connection refused or reset\n");
    g_nc_pcb  = NULL;
    g_nc_done = 1;
}

static int cmd_nc(int argc, char **argv)
{
    if (argc < 3) { kputs("usage: nc <ip> <port>\n"); return 1; }
    ip4_addr_t addr;
    if (!ip4addr_aton(argv[1], &addr)) { kputs("nc: bad IP\n"); return 1; }
    uint16_t port = 0;
    for (const char *p = argv[2]; *p >= '0' && *p <= '9'; p++) port = port*10+(*p-'0');
    g_nc_connected = 0; g_nc_done = 0;
    g_nc_pcb = tcp_new();
    if (!g_nc_pcb) { kputs("nc: out of PCBs\n"); return 1; }
    tcp_err(g_nc_pcb, nc_err_cb);
    err_t e = tcp_connect(g_nc_pcb, (ip_addr_t *)&addr, port, nc_connected_cb);
    if (e != ERR_OK) {
        tcp_abort(g_nc_pcb); g_nc_pcb = NULL;
        kprintf("nc: tcp_connect failed (%d)\n", (int)e);
        return 1;
    }
    uint32_t waited = 0;
    while (!g_nc_done && waited < 5000u) {
        net_poll(); sys_check_timeouts();
        tsc_delay_ms(10);
        waited += 10;
    }
    if (!g_nc_done) {
        kputs("nc: timeout\n");
        if (g_nc_pcb) { tcp_abort(g_nc_pcb); g_nc_pcb = NULL; }
    }
    return g_nc_connected ? 0 : 1;
}

static int cmd_lsusb(int argc, char **argv)
{
    (void)argc; (void)argv;
    usb_list_devices();
    return 0;
}

static int cmd_usbdesc(int argc, char **argv)
{
    if (argc < 2) { kputs("usage: usbdesc <dev_id>\n"); return 1; }
    uint32_t id = 0;
    for (const char *p = argv[1]; *p >= '0' && *p <= '9'; p++) id=id*10+(*p-'0');
    usb_dump_descriptors(id);
    return 0;
}

static int cmd_usbtest(int argc, char **argv)
{
    if (argc < 2) { kputs("usage: usbtest <dev_id>\n"); return 1; }
    uint32_t id = 0;
    for (const char *p = argv[1]; *p >= '0' && *p <= '9'; p++) id=id*10+(*p-'0');
    usb_run_loopback_test(id);
    return 0;
}

static int cmd_dd(int argc, char **argv)
{
    if (argc < 5) { kputs("usage: dd <src_dev> <dst_dev> <lba_src> <lba_dst> [count]\n"); return 1; }
    unsigned int sdev=0, ddev=0; unsigned long long slba=0, dlba=0; unsigned int cnt=1;
    for (const char *p = argv[1]; *p >= '0' && *p <= '9'; p++) sdev=sdev*10+(*p-'0');
    for (const char *p = argv[2]; *p >= '0' && *p <= '9'; p++) ddev=ddev*10+(*p-'0');
    for (const char *p = argv[3]; *p >= '0' && *p <= '9'; p++) slba=slba*10+(*p-'0');
    for (const char *p = argv[4]; *p >= '0' && *p <= '9'; p++) dlba=dlba*10+(*p-'0');
    if (argc >= 6) { cnt=0; for (const char *p = argv[5]; *p >= '0' && *p <= '9'; p++) cnt=cnt*10+(*p-'0'); }
    blkdev_t *src = blkdev_get(sdev), *dst = blkdev_get(ddev);
    if (!src || !dst) { kputs("dd: invalid device\n"); return 1; }
    static uint8_t dd_buf[4096] __attribute__((aligned(4096)));
    uint32_t per_pass = 4096 / src->sector_size;
    if (!per_pass) per_pass = 1;
    uint32_t done = 0;
    while (done < cnt) {
        uint32_t n = cnt - done; if (n > per_pass) n = per_pass;
        if (blkdev_read(src, slba+done, n, dd_buf) != 0) { kputs("dd: read error\n"); return 1; }
        if (blkdev_write(dst, dlba+done, n, dd_buf) != 0) { kputs("dd: write error\n"); return 1; }
        done += n;
    }
    kprintf("dd: %u sectors copied\n", cnt);
    return 0;
}

static int cmd_bench(int argc, char **argv)
{
    (void)argc; (void)argv;
    static uint8_t bench_buf[4 * 1024 * 1024];
    const size_t sz = sizeof(bench_buf);

    if (!tsc_khz) {
        kputs("bench: TSC not calibrated, cannot measure time\n");
        return 1;
    }

    uint64_t t0 = rdtsc_now();
    for (size_t i = 0; i < sz; i++) bench_buf[i] = (uint8_t)i;
    uint64_t t1 = rdtsc_now();
    for (size_t i = 0; i < sz; i++) { volatile uint8_t x = bench_buf[i]; (void)x; }
    uint64_t t2 = rdtsc_now();

    uint64_t mhz   = tsc_khz / 1000;
    if (!mhz) mhz = 1;
    uint64_t w_ns  = (t1 - t0) * 1000ULL / mhz;
    uint64_t r_ns  = (t2 - t1) * 1000ULL / mhz;
    uint64_t w_mbs = w_ns ? (uint64_t)sz * 1000ULL / w_ns : 0;
    uint64_t r_mbs = r_ns ? (uint64_t)sz * 1000ULL / r_ns : 0;
    kprintf("  write %lu MiB: %llu ns  (%llu MiB/s)\n",
            (unsigned long)(sz/(1024*1024)), (unsigned long long)w_ns, (unsigned long long)w_mbs);
    kprintf("  read  %lu MiB: %llu ns  (%llu MiB/s)\n",
            (unsigned long)(sz/(1024*1024)), (unsigned long long)r_ns, (unsigned long long)r_mbs);
    kprintf("  tsc freq used: %llu MHz\n", (unsigned long long)mhz);
    return 0;
}

static int cmd_spectre(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint32_t a, b, c, d;
    cpuid_ex(7, 0, &a, &b, &c, &d);
    int ibrs_all  = (d >> 26) & 1;
    int stibp_cap = (d >> 27) & 1;
    int arch_caps = (d >> 29) & 1;
    uint64_t spec_ctrl = 0;
    if (arch_caps) spec_ctrl = rdmsr64(0x48);
    int ibrs  = (int)((spec_ctrl >> 0) & 1);
    int stibp = (int)((spec_ctrl >> 1) & 1);
    int ssbd  = (int)((spec_ctrl >> 2) & 1);
    kprintf("  Spectre v1 (bounds check bypass)    : mitigated via array_index_nospec\n");
    kprintf("  Spectre v2 (branch target injection): IBRS=%s  IBRS_ALL=%s\n",
            ibrs ? "on" : "off", ibrs_all ? "yes" : "no");
    kprintf("  Spectre v3 (SpectreRSB)             : STIBP=%s cap=%s\n",
            stibp ? "on" : "off", stibp_cap ? "yes" : "no");
    kprintf("  Spectre v4 (spec store bypass)      : SSBD=%s\n",
            ssbd ? "on" : "off");
    kprintf("  IA32_ARCH_CAPS present              : %s\n", arch_caps ? "yes" : "no");
    if (arch_caps) {
        uint64_t caps = rdmsr64(0x10a);
        kprintf("  RDCL_NO (meltdown-free silicon)     : %s\n", caps & 1 ? "yes" : "no");
        kprintf("  IBRS_ALL                            : %s\n", (caps >> 1) & 1 ? "yes" : "no");
    }
    return 0;
}

static int cmd_meltdown(int argc, char **argv)
{
    (void)argc; (void)argv;
    int rdcl_no = 0;
    uint32_t a, b, c, d;
    cpuid_ex(7, 0, &a, &b, &c, &d);
    if ((d >> 29) & 1) {
        uint64_t caps = rdmsr64(0x10a);
        rdcl_no = (int)(caps & 1);
    }
    kprintf("  Meltdown (CVE-2017-5754)\n");
    kprintf("  RDCL_NO (silicon not vulnerable): %s\n", rdcl_no ? "yes" : "no");
    kprintf("  CR0.WP                          : %s\n",
            read_cr0() & (1u << 16) ? "on" : "off");
    kprintf("  KPTI                            : not implemented\n");
    if (!rdcl_no) kprintf("  status: likely vulnerable (no KPTI)\n");
    else          kprintf("  status: hardware not vulnerable\n");
    return 0;
}

static int cmd_smep(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint64_t cr4 = read_cr4();
    kprintf("  SMEP (CR4.20): %s\n", cr4 & (1u << 20) ? "enabled" : "disabled");
    kprintf("  SMAP (CR4.21): %s\n", cr4 & (1u << 21) ? "enabled" : "disabled");
    return 0;
}

static int cmd_smap(int argc, char **argv) { return cmd_smep(argc, argv); }

static int cmd_nx(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint64_t efer = rdmsr64(MSR_IA32_EFER);
    kprintf("NX (XD) bit: %s\n", ((efer>>11)&1) ? "enabled" : "disabled");
    return 0;
}

static int cmd_kaslr(int argc, char **argv)
{
    (void)argc; (void)argv;
    extern uint8_t _kernel_seal_start;
    uintptr_t base = (uintptr_t)&_kernel_seal_start;
    kprintf("  kernel text base : 0x%016llx\n", (unsigned long long)base);
    if (base == 0)
        kputs("  KASLR: _kernel_seal_start not exported by linker script\n");
    else if (base == 0x200000)
        kputs("  KASLR: disabled (fixed at 0x200000)\n");
    else
        kputs("  KASLR: enabled (randomized)\n");
    return 0;
}

static int cmd_acpipm(int argc, char **argv)
{
    (void)argc; (void)argv;
    acpi_pm_dump();
    return 0;
}

static int cmd_cpufreq(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint32_t n = smp_cpu_count();
    kprintf("  governor: %s\n", cpufreq_gov_name(cpufreq_get_gov()));
    int any = 0;
    for (uint32_t i = 0; i < n; i++) {
        cpufreq_policy_t pol;
        if (cpufreq_get_policy(i, &pol) < 0) continue;
        if (!pol.cur_khz && !pol.base_khz) continue;
        kprintf("  cpu%u: cur=%u.%03u MHz  base=%u.%03u MHz  "
                "p=[%u..%u] boost=%u  turbo=%s  hwp=%s\n",
                i,
                pol.cur_khz  / 1000, pol.cur_khz  % 1000,
                pol.base_khz / 1000, pol.base_khz % 1000,
                pol.min_pstate, pol.max_pstate, pol.boost_pstate,
                pol.turbo ? "on" : "off",
                pol.hwp   ? "on" : "off");
        any = 1;
    }
    if (!any) {
        if (tsc_khz)
            kprintf("  cpufreq driver unavailable; TSC: ~%llu MHz\n",
                    (unsigned long long)(tsc_khz / 1000));
        else
            kputs("  cpufreq unavailable and TSC not calibrated\n");
    }
    return 0;
}

static int cmd_mwait(int argc, char **argv)
{
    (void)argc; (void)argv;
    kputs("entering MWAIT C1...\n");
    static volatile uint32_t _mwait_hint = 0;
    __asm__ volatile("monitor" :: "a"(&_mwait_hint), "c"(0), "d"(0) : "memory");
    __asm__ volatile("mwait"   :: "a"(0),             "c"(0)         : "memory");
    kputs("MWAIT returned\n");
    return 0;
}

static int cmd_date(int argc, char **argv)
{
    (void)argc; (void)argv;
    rtc_time_t t; rtc_read(&t);
    char buf[20]; rtc_fmt(&t, buf, sizeof(buf));
    kprintf("%s UTC\n", buf);
    return 0;
}

static int cmd_uptime(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint32_t ms = sys_now();
    uint32_t s = ms/1000, m = s/60, h = m/60;
    kprintf("up %u:%02u:%02u  (%u s,  %u ms)\n", h, m%60, s%60, s, ms);
    return 0;
}

static int cmd_hpet(int argc, char **argv)
{
    (void)argc; (void)argv;
    volatile uint64_t *hpet_cap    = (volatile uint64_t *)0xFED00000UL;
    volatile uint64_t *hpet_cfg    = (volatile uint64_t *)0xFED00010UL;
    volatile uint64_t *hpet_cnt    = (volatile uint64_t *)0xFED000F0UL;

    uint64_t cap = *hpet_cap;
    uint32_t period_fs = (uint32_t)(cap >> 32);
    if (!period_fs) {
        kputs("  HPET: not present or not mapped at 0xFED00000\n");
        return 1;
    }

    uint64_t cfg = *hpet_cfg;
    if (!(cfg & 1)) {
        *hpet_cfg = cfg | 1;
    }

    uint64_t counter = *hpet_cnt;
    uint8_t  timers  = (uint8_t)((cap >> 8) & 0x1F) + 1;
    uint8_t  rev     = (uint8_t)(cap & 0xFF);

    kprintf("  HPET @ 0xFED00000\n");
    kprintf("  rev         : %u\n",  rev);
    kprintf("  num_timers  : %u\n",  timers);
    kprintf("  period_fs   : %u  (~%u MHz)\n",
            period_fs,
            (uint32_t)(1000000000000000ULL / (uint64_t)period_fs / 1000000ULL));
    kprintf("  counter     : 0x%016llx\n", (unsigned long long)counter);
    return 0;
}

extern char   g_dmesg[];
extern size_t g_dmesg_pos;

static int cmd_dmesg(int argc, char **argv)
{
    (void)argc; (void)argv;
    g_dmesg[g_dmesg_pos] = '\0';
    kputs(g_dmesg);
    return 0;
}

static int cmd_panic(int argc, char **argv)
{
    const char *reason = (argc >= 2) ? argv[1] : "user-triggered";
    kprintf("\n\n*** [!!] KERNEL PANIC: %s\n", reason);
    kstack_trace();
    kputs("*** System halted.\n");
    for (;;) __asm__ volatile("cli; hlt" ::: "memory");
}

static int cmd_hexdump(int argc, char **argv)
{
    if (argc < 3) { kputs("usage: hexdump <addr_hex> <len>\n"); return 1; }
    uintptr_t addr = 0; uint32_t len = 0;
    const char *p = argv[1];
    if (p[0]=='0'&&p[1]=='x') p+=2;
    for (; *p; p++) {
        addr <<= 4;
        if (*p>='0'&&*p<='9') addr|=*p-'0';
        else if (*p>='a'&&*p<='f') addr|=*p-'a'+10;
        else if (*p>='A'&&*p<='F') addr|=*p-'A'+10;
    }
    for (const char *q = argv[2]; *q >= '0' && *q <= '9'; q++) len=len*10+(*q-'0');
    if (len > 4096) len = 4096;
    static const char hx[] = "0123456789abcdef";
    char line[80];
    for (uint32_t off = 0; off < len; off += 16) {
        uint32_t pos = 0;
        uintptr_t a = addr+off;
        for (int s = 60; s >= 0; s -= 4) { line[pos++] = hx[(a>>s)&0xF]; }
        line[pos++] = ':'; line[pos++] = ' ';
        for (uint32_t k = 0; k < 16 && off+k < len; k++) {
            uint8_t b = *(volatile uint8_t *)(addr+off+k);
            line[pos++] = hx[b>>4]; line[pos++] = hx[b&0xF]; line[pos++] = ' ';
        }
        line[pos++] = '\n'; line[pos] = '\0';
        kputs(line);
    }
    return 0;
}

static int cmd_symaddr(int argc, char **argv)
{
    if (argc < 2) { kputs("usage: symaddr <symbol>\n"); return 1; }
    const char *sym = argv[1];
    while (*sym == ' ' || *sym == '\t') sym++;
    size_t slen = strlen(sym);
    while (slen > 0 && (sym[slen-1] == ' ' || sym[slen-1] == '\t')) slen--;

    uintptr_t addr = ksym_find(sym);
    if (addr) {
        kprintf("  %s = 0x%016llx\n", sym, (unsigned long long)addr);
        return 0;
    }

#define TRYADDR(fn) \
    if (slen == sizeof(#fn)-1 && __builtin_memcmp(sym, #fn, slen) == 0) { \
        kprintf("  %s = 0x%016llx\n", #fn, (unsigned long long)(uintptr_t)&(fn)); \
        return 0; \
    }
    TRYADDR(kmalloc)
    TRYADDR(kfree)
    TRYADDR(sched_yield)
    TRYADDR(kprintf)
    TRYADDR(vfs_open)
    TRYADDR(vfs_read)
    TRYADDR(vfs_write)
    TRYADDR(sched_thread_create)
    TRYADDR(cpufreq_init)
    TRYADDR(numa_init)
    TRYADDR(kmalloc_init)
    TRYADDR(sched_current)
    TRYADDR(vfs_close)
    TRYADDR(vfs_stat)
    TRYADDR(vfs_mkdir)
    TRYADDR(blkdev_read)
    TRYADDR(blkdev_write)
    TRYADDR(sched_thread_count)
    TRYADDR(sched_block)
    TRYADDR(sched_unblock)
#undef TRYADDR
    kprintf("  symbol '%s' not in stub table (ksym_find also returned 0)\n", sym);
    return 1;
}

static int cmd_breakpoint(int argc, char **argv)
{
    if (argc < 2) { kputs("usage: breakpoint <addr_hex>\n"); return 1; }
    uintptr_t addr = 0; const char *p = argv[1];
    if (p[0]=='0'&&p[1]=='x') p+=2;
    for (; *p; p++) {
        addr<<=4;
        if (*p>='0'&&*p<='9') addr|=*p-'0';
        else if (*p>='a'&&*p<='f') addr|=*p-'a'+10;
        else if (*p>='A'&&*p<='F') addr|=*p-'A'+10;
    }
    hwb_set(0, addr, HWB_EXEC, HWB_1B);
    kprintf("HW breakpoint set at 0x%llx\n", (unsigned long long)addr);
    return 0;
}

static int cmd_stack(int argc, char **argv)
{
    (void)argc; (void)argv;
    kstack_trace();
    return 0;
}

static int cmd_log(int argc, char **argv)
{
    if (argc < 3) { kputs("usage: log <info|warn|err|ok> <msg>\n"); return 1; }
    if (argv[1][0]=='i')      klog_info("ksh", argv[2]);
    else if (argv[1][0]=='w') klog_warn("ksh", argv[2]);
    else if (argv[1][0]=='e') klog_fail("ksh", argv[2]);
    else                      klog_ok("ksh", argv[2]);
    return 0;
}

static int cmd_physmap(int argc, char **argv)
{
    (void)argc; (void)argv;
    kputs("  physical memory map (NUMA SRAT):\n");
    uint32_t n = numa_node_count();
    int any = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint64_t base = g_numa_nodes[i].mem_base;
        uint64_t len  = g_numa_nodes[i].mem_len;
        if (!len || len == ~(uint64_t)0) continue;
        kprintf("  node %u  [0x%016llx - 0x%016llx]  %llu MiB\n",
                i,
                (unsigned long long)base,
                (unsigned long long)(base + len),
                (unsigned long long)(len / (1024 * 1024)));
        any = 1;
    }
    if (!any) kputs("  (no NUMA regions; full E820 map not stored post-boot)\n");
    return 0;
}

static int cmd_spurious(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint64_t sw = irq_spurious_count();
    kprintf("  spurious IRQs (software counter): %llu\n", (unsigned long long)sw);
    uint64_t apic_base_msr = rdmsr64(0x1B);
    if (!(apic_base_msr & (1u << 10))) {
        volatile uint32_t *apic = (volatile uint32_t *)((uintptr_t)(apic_base_msr & 0xFFFFF000ULL));
        uint32_t svr = apic[0xF0 / 4];
        kprintf("  APIC spurious vector reg: 0x%08x  vector=0x%02x  enabled=%u\n",
                svr, svr & 0xFF, (svr >> 8) & 1);
    } else {
        uint64_t svr = rdmsr64(0x80F);
        kprintf("  x2APIC SVR: 0x%llx  vector=0x%02x  enabled=%u\n",
                (unsigned long long)svr, (uint32_t)(svr & 0xFF), (uint32_t)((svr >> 8) & 1));
    }
    return 0;
}

static int cmd_barrier(int argc, char **argv)
{
    (void)argc; (void)argv;
    __asm__ volatile("mfence" ::: "memory");
    kputs("mfence issued\n");
    return 0;
}

static int cmd_lat(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint64_t t0 = rdtsc_now();
    __asm__ volatile("cpuid" ::: "eax","ebx","ecx","edx");
    uint64_t t1 = rdtsc_now();
    kprintf("cpuid latency: %llu cycles\n", (unsigned long long)(t1 - t0));
    return 0;
}

static int cmd_ktrace(int argc, char **argv)
{
    (void)argc; (void)argv;
    ktrace_dump();
    kstack_trace();
    return 0;
}

static int cmd_strace(int argc, char **argv)
{
    (void)argc; (void)argv;
    kputs("strace: syscall tracing requires userspace ELF loader\n");
    kputs("  SYSCALL/SYSRET vector is wired; kposixz layer is initialized\n");
    kputs("  No userspace process active to trace\n");
    return 1;
}

static int cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (i > 1) kputs(" ");
        kputs(argv[i]);
    }
    kputs("\n");
    return 0;
}

static int cmd_clear(int argc, char **argv)
{
    (void)argc; (void)argv;
    kputs("\033[2J\033[H");
    return 0;
}

static int cmd_history(int argc, char **argv)
{
    (void)argc; (void)argv;
    for (int i = 0; i < g_hist_n; i++)
        kprintf("  %3d  %s\n", i+1, g_hist[i]);
    return 0;
}

static int cmd_alias(int argc, char **argv)
{
    if (argc == 1) {
        for (int i = 0; i < g_nalias; i++)
            kprintf("  alias %s='%s'\n", g_alias_name[i], g_alias_val[i]);
        return 0;
    }
    if (argc < 3) { kputs("usage: alias <name> <expansion>\n"); return 1; }
    for (int i = 0; i < g_nalias; i++) {
        if (strcmp(g_alias_name[i], argv[1]) == 0) {
            strlcpy(g_alias_val[i], argv[2], sizeof(g_alias_val[i]));
            kprintf("alias updated: %s='%s'\n", argv[1], argv[2]);
            return 0;
        }
    }
    if (g_nalias >= NALIAS) { kputs("alias: table full\n"); return 1; }
    strlcpy(g_alias_name[g_nalias], argv[1], sizeof(g_alias_name[g_nalias]));
    strlcpy(g_alias_val[g_nalias],  argv[2], sizeof(g_alias_val[g_nalias]));
    g_nalias++;
    kprintf("alias set: %s='%s'\n", argv[1], argv[2]);
    return 0;
}

static int cmd_unalias(int argc, char **argv)
{
    if (argc < 2) { kputs("usage: unalias <name>\n"); return 1; }
    for (int i = 0; i < g_nalias; i++) {
        if (strcmp(g_alias_name[i], argv[1]) == 0) {
            for (int j = i; j < g_nalias - 1; j++) {
                strlcpy(g_alias_name[j], g_alias_name[j+1], sizeof(g_alias_name[j]));
                strlcpy(g_alias_val[j],  g_alias_val[j+1],  sizeof(g_alias_val[j]));
            }
            g_nalias--;
            return 0;
        }
    }
    kprintf("unalias: %s not found\n", argv[1]);
    return 1;
}

static int cmd_sleep(int argc, char **argv)
{
    if (argc < 2) { kputs("usage: sleep <ms>\n"); return 1; }
    uint32_t ms = 0;
    for (const char *p = argv[1]; *p >= '0' && *p <= '9'; p++) ms=ms*10+(*p-'0');
    tsc_delay_ms(ms);
    return 0;
}

static int cmd_repeat(int argc, char **argv)
{
    if (argc < 3) { kputs("usage: repeat <n> <cmd...>\n"); return 1; }
    uint32_t n = 0;
    for (const char *p = argv[1]; *p >= '0' && *p <= '9'; p++) n=n*10+(*p-'0');
    char line[KSH_LINE_MAX];
    size_t li = 0;
    for (int i = 2; i < argc; i++) {
        if (i>2 && li < KSH_LINE_MAX-1) line[li++] = ' ';
        for (const char *s = argv[i]; *s && li < KSH_LINE_MAX-1; s++) line[li++] = *s;
    }
    line[li] = '\0';
    for (uint32_t k = 0; k < n; k++) ksh_dispatch(line);
    return 0;
}

static int cmd_syscall(int argc, char **argv)
{
    (void)argc; (void)argv;
    kputs("syscall: SYSCALL/SYSRET vector wired (syscall_init ran at boot)\n");
    kputs("  kposixz layer initialized; awaiting ELF loader\n");
    kputs("  no userspace process to issue syscalls from\n");
    return 0;
}

static int cmd_signal(int argc, char **argv)
{
    if (argc >= 3) {
        uint32_t tid = (uint32_t)katoi(argv[1]);
        int sig = katoi(argv[2]);
        sched_thread_t *t = sched_get_thread_by_tid(tid);
        if (!t) { kprintf("signal: tid %u not found\n", tid); return 1; }
        sched_thread_signal(t, sig);
        kprintf("signal: sent %d to tid %u\n", sig, tid);
        return 0;
    }
    kputs("signal: POSIX signal delivery requires userspace process model\n");
    kputs("  kernel threads: signal <tid> <signum> to unblock a blocked thread\n");
    return 0;
}

static int cmd_fork(int argc, char **argv)
{
    (void)argc; (void)argv;
    kputs("fork: not implemented (requires userspace process model)\n");
    return 1;
}

static int cmd_exec(int argc, char **argv)
{
    if (argc >= 2) {
        vfs_stat_t st;
        int rc = vfs_stat(argv[1], &st);
        if (rc < 0) {
            kprintf("exec: %s not found on VFS (err %d)\n", argv[1], rc);
            return 1;
        }
        kprintf("exec: %s found on VFS (%llu bytes)\n", argv[1],
                (unsigned long long)st.size);
        kputs("exec: ELF userspace loader not yet wired\n");
        return 1;
    }
    kputs("exec: ELF userspace loader not yet wired\n");
    return 1;
}

static int cmd_posixtest(int argc, char **argv)
{
    (void)argc; (void)argv;
    kposixz_run_tests();
    return 0;
}

static int cmd_testhda(int argc, char **argv)
{
    (void)argc; (void)argv;
    sound_status_t st = sound_available() ? SOUND_OK : SOUND_ERR_NOT_FOUND;
    if (st == SOUND_OK) {
        kputs("HDA: initialized and ready\n");
        sound_pcm_format_t fmt = { .sample_rate = 44100, .channels = 2,
                                   .bits = 16, .is_float = 0 };
        sound_stream_t *s = sound_open_output(&fmt, NULL, NULL);
        if (s) {
            kputs("HDA: output stream opened OK\n");
            sound_stream_close(s);
        } else {
            kputs("HDA: output stream open failed\n");
        }
    } else if (st == SOUND_ERR_NOT_FOUND) {
        kputs("HDA: no Intel HDA controller detected\n");
    } else {
        kprintf("HDA: error -- %s\n", sound_strerror(st));
    }
    return 0;
}

static int cmd_version(int argc, char **argv)
{
    (void)argc; (void)argv;
    kprintf("%s %s %s %s\n", KOBALT_OS, KOBALT_VERSION, KOBALT_BUILD, KOBALT_ARCH);
    kputs("Copyright (C) 2026  Abhranil Dasgupta\n");
    kputs("Licensed under the GNU General Public License v3.0-only\n");
    return 0;
}

static int cmd_exit(int argc, char **argv)
{
    (void)argc; (void)argv;
    kputs("System halting...\n");
    for (;;) __asm__ volatile("cli; hlt" ::: "memory");
}

static int cmd_reboot(int argc, char **argv)
{
    (void)argc; (void)argv;
    kputs("rebooting...\n");
    __asm__ volatile("outb %0, %1" :: "a"((uint8_t)0xFE), "Nd"((uint16_t)0x64));
    for (;;) __asm__ volatile("cli; hlt");
    __builtin_unreachable();
}

static int cmd_halt(int argc, char **argv)
{
    (void)argc; (void)argv;
    kputs("halting.\n");
    for (;;) __asm__ volatile("cli; hlt");
    __builtin_unreachable();
}

static int cmd_blkread(int argc, char **argv)
{
    if (argc < 3) { kputs("usage: blkread <dev_idx> <lba> [count=1]\n"); return 1; }
    unsigned int dev_idx=0, count=1; unsigned long long lba=0;
    for (const char *p = argv[1]; *p>='0'&&*p<='9'; p++) dev_idx=dev_idx*10+(*p-'0');
    for (const char *p = argv[2]; *p>='0'&&*p<='9'; p++) lba=lba*10+(*p-'0');
    if (argc>=4) { count=0; for (const char *p=argv[3]; *p>='0'&&*p<='9'; p++) count=count*10+(*p-'0'); }
    if (count>8) { kputs("blkread: max 8\n"); return 1; }
    blkdev_t *bd = blkdev_get(dev_idx);
    if (!bd) { kputs("blkread: invalid dev\n"); return 1; }
    static uint8_t rbuf[8*4096] __attribute__((aligned(4096)));
    if (blkdev_read(bd, lba, count, rbuf) != 0) { klog_fail("blkread","read failed"); return 1; }
    static const char hx[] = "0123456789abcdef";
    char line[64]; uint32_t bc = count*bd->sector_size;
    for (uint32_t off = 0; off < bc; off += 16) {
        uint32_t pos=0;
        line[pos++]=hx[(off>>12)&0xF]; line[pos++]=hx[(off>>8)&0xF];
        line[pos++]=hx[(off>>4)&0xF];  line[pos++]=hx[off&0xF];
        line[pos++]=':'; line[pos++]=' ';
        for (uint32_t k=0; k<16&&off+k<bc; k++) {
            uint8_t b=rbuf[off+k]; line[pos++]=hx[b>>4]; line[pos++]=hx[b&0xF]; line[pos++]=' ';
        }
        line[pos++]='\n'; line[pos]='\0'; kputs(line);
    }
    return 0;
}

static int cmd_blkwrite(int argc, char **argv)
{
    if (argc < 3) { kputs("usage: blkwrite <dev_idx> <lba>\n"); return 1; }
    unsigned int dev_idx=0; unsigned long long lba=0;
    for (const char *p=argv[1]; *p>='0'&&*p<='9'; p++) dev_idx=dev_idx*10+(*p-'0');
    for (const char *p=argv[2]; *p>='0'&&*p<='9'; p++) lba=lba*10+(*p-'0');
    blkdev_t *bd = blkdev_get(dev_idx);
    if (!bd) { kputs("blkwrite: invalid dev\n"); return 1; }
    static uint8_t wbuf[4096] __attribute__((aligned(4096)));
    for (uint32_t i=0; i<bd->sector_size; i++) wbuf[i]=(uint8_t)(i&0xFF);
    if (blkdev_write(bd, lba, 1, wbuf)==0) klog_ok("blkwrite","sector written (ramp pattern)");
    else klog_fail("blkwrite","write failed");
    return 0;
}

static int cmd_fatfs(int argc, char **argv);

static const ksh_cmd_t g_cmds[] = {
    { "ps",          "list threads",                                  cmd_ps          },
    { "kill",        "kill <tid>",                                    cmd_kill        },
    { "nice",        "nice <tid> <niceval>",                         cmd_nice        },
    { "renice",      "alias for nice",                               cmd_renice      },
    { "sched",       "scheduler info",                               cmd_sched       },
    { "top",         "live thread view",                             cmd_top         },
    { "wait",        "wait <tid>",                                   cmd_wait        },
    { "meminfo",     "slab memory info",                             cmd_meminfo     },
    { "vmstat",      "vm statistics",                                cmd_vmstat      },
    { "mslab",       "slab cache table",                             cmd_mslab       },
    { "pmap",        "pmap <tid>",                                   cmd_pmap        },
    { "oom",         "trigger OOM reclaim",                          cmd_oom         },
    { "numa",        "NUMA topology",                                cmd_numa        },
    { "mount",       "mount [dev path type]",                        cmd_mount       },
    { "ls",          "ls [path]",                                    cmd_ls          },
    { "cat",         "cat <path>",                                   cmd_cat         },
    { "write",       "write <path> <data>",                         cmd_write       },
    { "stat",        "stat <path>",                                  cmd_stat        },
    { "file",        "file <path>",                                  cmd_file        },
    { "sync",        "flush VFS caches",                             cmd_sync        },
    { "truncate",    "truncate <path> <size>",                       cmd_truncate    },
    { "rm",          "rm <path>",                                    cmd_rm          },
    { "mkdir",       "mkdir <path>",                                 cmd_mkdir       },
    { "fsck",        "fsck [dev_idx]",                               cmd_fsck        },
    { "mkfs",        "mkfs <vol_idx>",                               cmd_mkfs        },
    { "blkdump",     "blkdump <dev> <lba>",                         cmd_blkdump     },
    { "blkdev",      "block device list",                            cmd_blkdev      },
    { "lsblk",       "list block devices",                           cmd_lsblk       },
    { "blkread",     "blkread <dev> <lba> [n]",                     cmd_blkread     },
    { "blkwrite",    "blkwrite <dev> <lba>",                        cmd_blkwrite    },
    { "dd",          "dd <src> <dst> <slba> <dlba> [n]",            cmd_dd          },
    { "partinfo",    "partinfo [dev_idx]",                           cmd_partinfo    },
    { "cpuinfo",     "CPU info",                                     cmd_cpuinfo     },
    { "cpuid",       "cpuid [leaf]",                                 cmd_cpuid       },
    { "msr",         "msr <addr> [value]",                          cmd_msr         },
    { "rdtsc",       "read TSC",                                     cmd_rdtsc       },
    { "x2apic",      "x2APIC status",                               cmd_x2apic      },
    { "apictimer",   "APIC timer info",                              cmd_apictimer   },
    { "tsc",         "TSC frequency",                               cmd_tsc         },
    { "ioapic",      "IOAPIC routing table",                        cmd_ioapic      },
    { "pci",         "list PCI devices",                             cmd_pci         },
    { "acpi",        "ACPI tables [table]",                         cmd_acpi        },
    { "uname",       "uname [-a]",                                   cmd_uname       },
    { "amx",         "Intel AMX status/test",                        cmd_amx         },
    { "irq",         "IRQ info",                                     cmd_irq         },
    { "ipi",         "ipi <cpu>",                                    cmd_ipi         },
    { "nmi",         "NMI counts",                                   cmd_nmi         },
    { "except",      "exception info",                               cmd_except      },
    { "cpu",         "per-CPU info",                                 cmd_cpu         },
    { "migrate",     "migrate <tid> <cpu>",                         cmd_migrate     },
    { "affinity",    "affinity <tid> <mask_hex>",                   cmd_affinity    },
    { "tlbshoot",    "TLB shootdown",                               cmd_tlbshoot    },
    { "tykid",       "tykid status | check <driver>",               cmd_tykid       },
    { "ifconfig",    "ifconfig [up|down]",                           cmd_ifconfig    },
    { "ping",        "ping <ip>",                                    cmd_ping        },
    { "arp",         "ARP cache",                                    cmd_arp         },
    { "route",       "routing table",                               cmd_route       },
    { "netstat",     "TCP/UDP sockets",                              cmd_netstat     },
    { "tcpdump",     "tcpdump [count]",                              cmd_tcpdump     },
    { "nc",          "nc <ip> <port>",                               cmd_nc          },
    { "lsusb",       "list USB devices",                             cmd_lsusb       },
    { "usbdesc",     "usbdesc <dev>",                                cmd_usbdesc     },
    { "usbtest",     "usbtest <dev>",                                cmd_usbtest     },
    { "bench",       "memory benchmark",                             cmd_bench       },
    { "spectre",     "Spectre mitigation status",                   cmd_spectre     },
    { "meltdown",    "Meltdown status",                              cmd_meltdown    },
    { "smep",        "SMEP/SMAP status",                             cmd_smep        },
    { "smap",        "SMEP/SMAP status",                             cmd_smap        },
    { "nx",          "NX/XD bit status",                             cmd_nx          },
    { "kaslr",       "KASLR status",                                 cmd_kaslr       },
    { "acpipm",      "ACPI power management info",                   cmd_acpipm      },
    { "cpufreq",     "frequency per-cpu",                            cmd_cpufreq     },
    { "mwait",       "enter MWAIT C1",                               cmd_mwait       },
    { "date",        "current date/time (UTC)",                      cmd_date        },
    { "uptime",      "system uptime",                               cmd_uptime      },
    { "hpet",        "HPET counter",                                 cmd_hpet        },
    { "dmesg",       "kernel log",                                   cmd_dmesg       },
    { "panic",       "panic [reason]",                               cmd_panic       },
    { "hexdump",     "hexdump <addr_hex> <len>",                     cmd_hexdump     },
    { "symaddr",     "symaddr <name>",                               cmd_symaddr     },
    { "breakpoint",  "breakpoint <addr_hex>",                        cmd_breakpoint  },
    { "stack",       "stack trace",                                  cmd_stack       },
    { "log",         "log <info|warn|err|ok> <msg>",                 cmd_log         },
    { "physmap",     "physical memory map",                          cmd_physmap     },
    { "spurious",    "spurious IRQ info",                            cmd_spurious    },
    { "barrier",     "mfence",                                       cmd_barrier     },
    { "lat",         "CPUID latency",                               cmd_lat         },
    { "ktrace",      "kernel trace dump + stack",                    cmd_ktrace      },
    { "strace",      "syscall trace info",                           cmd_strace      },
    { "exec",        "exec <elf>",                                   cmd_exec        },
    { "fork",        "fork info",                                    cmd_fork        },
    { "syscall",     "syscall status",                               cmd_syscall     },
    { "signal",      "signal [tid signum]",                         cmd_signal      },
    { "posixtest",   "POSIX test suite (kposixz)",                   cmd_posixtest   },
    { "echo",        "echo [args]",                                  cmd_echo        },
    { "clear",       "clear screen",                                 cmd_clear       },
    { "history",     "command history",                              cmd_history     },
    { "alias",       "alias [name value]",                           cmd_alias       },
    { "unalias",     "unalias <name>",                               cmd_unalias     },
    { "sleep",       "sleep <ms>",                                   cmd_sleep       },
    { "repeat",      "repeat <n> <cmd...>",                         cmd_repeat      },
    { "fatfs",       "fatfs <stat|ls|cat|write|mkfs|mount|umount>",  cmd_fatfs       },
    { "testhda",     "test Intel HDA",                               cmd_testhda     },
    { "version",     "kernel version",                               cmd_version     },
    { "reboot",      "reboot system",                               cmd_reboot      },
    { "halt",        "halt system",                                  cmd_halt        },
    { "exit",        "halt system",                                  cmd_exit        },
    { "help",        "this list",                                    cmd_help        },
    { NULL, NULL, NULL },
};

static int cmd_help(int argc, char **argv)
{
    (void)argc; (void)argv;
    for (int i = 0; g_cmds[i].name; i++)
        kprintf("  %-14s  %s\n", g_cmds[i].name, g_cmds[i].brief);
    return 0;
}

static int cmd_fatfs(int argc, char **argv)
{
    if (argc < 2) {
        kputs("usage: fatfs <sub-command> [args...]\n");
        kputs("  stat   <vol>              show volume info\n");
        kputs("  ls     <vol> [path]       list directory\n");
        kputs("  cat    <vol> <path>       print file\n");
        kputs("  write  <vol> <path> <txt> write file\n");
        kputs("  mkfs   <vol>              format FAT32\n");
        kputs("  mount  <vol>              mount volume\n");
        kputs("  umount <vol>              unmount volume\n");
        return 1;
    }
    const char *sub = argv[1];
    int vol = -1;
    if (argc >= 3) {
        const char *p = argv[2]; vol = 0;
        while (*p >= '0' && *p <= '9') vol = vol*10+(*p++-'0');
        if (vol < 0 || vol >= FATFS_KOBALT_MAX_VOLS) {
            kprintf("fatfs: invalid volume '%s'\n", argv[2]); return 1;
        }
    }
    char vp[4] = "0:"; if (vol >= 0) vp[0] = (char)('0'+vol);

    if (sub[0]=='s' && sub[1]=='t' && !sub[4]) {
        if (vol < 0) { kputs("fatfs stat: missing volume\n"); return 1; }
        if (!fatfs_kobalt_is_mounted(vol)) { kprintf("fatfs: volume %d not mounted\n", vol); return 1; }
        FATFS *fsp = NULL; DWORD fre = 0;
        char rp[4] = { vp[0], ':', '/', '\0' };
        if (f_getfree(rp, &fre, &fsp) != FR_OK) { kputs("fatfs: f_getfree failed\n"); return 1; }
        const char *ft = "unknown";
        if      (fsp->fs_type==FS_FAT12) ft="FAT12";
        else if (fsp->fs_type==FS_FAT16) ft="FAT16";
        else if (fsp->fs_type==FS_FAT32) ft="FAT32";
        uint64_t total = ((uint64_t)(fsp->n_fatent-2)*fsp->csize*512)/1024;
        uint64_t freek = ((uint64_t)fre*fsp->csize*512)/1024;
        char label[24]="(none)"; DWORD vsn=0; f_getlabel(rp, label, &vsn);
        kprintf("\n  Volume %d - %s\n  Label: %s  VSN: %08x\n  Total: %llu KiB  Free: %llu KiB\n\n",
                vol, ft, label, (unsigned)vsn,
                (unsigned long long)total, (unsigned long long)freek);
        return 0;
    }
    if (sub[0]=='l' && sub[1]=='s' && !sub[2]) {
        if (vol<0||!fatfs_kobalt_is_mounted(vol)) { kprintf("fatfs: volume %d not mounted\n", vol); return 1; }
        char path[128] = { vp[0], ':', '/', '\0' };
        if (argc>=4) {
            path[0]=vp[0]; path[1]=':'; path[2]='\0';
            size_t b=(argv[3][0]!='/')?3:2;
            if (argv[3][0]!='/') { path[2]='/'; path[3]='\0'; }
            size_t i=0;
            while (b+i<sizeof(path)-1&&argv[3][i]) { path[b+i]=argv[3][i]; i++; }
            path[b+i]='\0';
        }
        DIR dir; FILINFO fi;
        if (f_opendir(&dir, path) != FR_OK) { kprintf("fatfs ls: cannot open '%s'\n", path); return 1; }
        kprintf("\n  Directory of %s\n\n  Attr  Size        Name\n  ----  ----------  ----\n", path);
        while (1) {
            if (f_readdir(&dir,&fi)!=FR_OK||fi.fname[0]=='\0') break;
            char at[5]="----";
            if (fi.fattrib&AM_DIR) at[0]='D'; if (fi.fattrib&AM_RDO) at[1]='R';
            if (fi.fattrib&AM_HID) at[2]='H'; if (fi.fattrib&AM_SYS) at[3]='S';
            if (fi.fattrib&AM_DIR) kprintf("  %s  %-10s  %s/\n", at, "<DIR>", fi.fname);
            else kprintf("  %s  %-10lu  %s\n", at, (unsigned long)fi.fsize, fi.fname);
        }
        f_closedir(&dir); kputc('\n'); return 0;
    }
    if (sub[0]=='c' && sub[1]=='a' && !sub[3]) {
        if (vol<0||argc<4||!fatfs_kobalt_is_mounted(vol)) { kputs("usage: fatfs cat <vol> <path>\n"); return 1; }
        char path[128]; path[0]=vp[0]; path[1]=':'; path[2]='\0';
        const char *up=argv[3];
        size_t b=(up[0]!='/')?3:2, i=0;
        if (up[0]!='/') { path[2]='/'; path[3]='\0'; }
        while (b+i<sizeof(path)-1&&up[i]) { path[b+i]=up[i]; i++; } path[b+i]='\0';
        FIL fil;
        if (f_open(&fil,path,FA_READ)!=FR_OK) { kprintf("fatfs cat: cannot open '%s'\n", path); return 1; }
        static uint8_t fb[512]; UINT br;
        while (1) { if (f_read(&fil,fb,sizeof(fb),&br)!=FR_OK||br==0) break; for (UINT k=0;k<br;k++) kputc((char)fb[k]); }
        f_close(&fil); return 0;
    }
    if (sub[0]=='w' && sub[1]=='r' && !sub[5]) {
        if (vol<0||argc<5||!fatfs_kobalt_is_mounted(vol)) { kputs("usage: fatfs write <vol> <path> <text>\n"); return 1; }
        char path[128]; path[0]=vp[0]; path[1]=':'; path[2]='\0';
        const char *up=argv[3];
        size_t b=(up[0]!='/')?3:2, i=0;
        if (up[0]!='/') { path[2]='/'; path[3]='\0'; }
        while (b+i<sizeof(path)-1&&up[i]) { path[b+i]=up[i]; i++; } path[b+i]='\0';
        FIL fil; FRESULT fr=f_open(&fil,path,FA_CREATE_ALWAYS|FA_WRITE);
        if (fr!=FR_OK) { kprintf("fatfs write: cannot create '%s' (fr=%d)\n", path, (int)fr); return 1; }
        const char *text=argv[4]; UINT bw; size_t tl=strlen(text);
        f_write(&fil,text,(UINT)tl,&bw); char nl='\n'; UINT nb; f_write(&fil,&nl,1,&nb);
        f_close(&fil);
        kprintf("fatfs: wrote %lu bytes to %s\n", (unsigned long)bw, path);
        return 0;
    }
    if (sub[0]=='m'&&sub[1]=='k'&&sub[2]=='f') {
        if (vol < 0) { kputs("usage: fatfs mkfs <vol>\n"); return 1; }
        char path[4] = { vp[0], ':', '/', '\0' };
        MKFS_PARM opt; __builtin_memset(&opt, 0, sizeof(opt)); opt.fmt = FM_FAT32;
        static uint8_t work[4096];
        kprintf("fatfs mkfs: formatting volume %d...\n", vol);
        FRESULT fr2 = f_mkfs(path, &opt, work, sizeof(work));
        if (fr2 != FR_OK) { kprintf("fatfs mkfs: failed (fr=%d)\n", (int)fr2); return 1; }
        kprintf("fatfs mkfs: volume %d formatted as FAT32\n", vol);
        return 0;
    }
    if (sub[0]=='m'&&sub[1]=='o'&&sub[2]=='u'&&!sub[5]) {
        if (vol<0) { kputs("fatfs mount: missing volume\n"); return 1; }
        int rc = fatfs_kobalt_mount(vol, vol);
        kprintf("fatfs: volume %d %s\n", vol, rc==0?"mounted":"mount failed"); return 0;
    }
    if (sub[0]=='u'&&sub[1]=='m') {
        if (vol<0) { kputs("fatfs umount: missing volume\n"); return 1; }
        fatfs_kobalt_unmount(vol);
        kprintf("fatfs: volume %d unmounted\n", vol); return 0;
    }
    kprintf("fatfs: unknown sub-command '%s'\n", sub);
    return 1;
}

static void split(char *line, int *argc, char **argv, int max)
{
    *argc = 0;
    char *p = line;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (*argc < max) argv[(*argc)++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
}

static void hist_push(const char *line)
{
    if (!*line) return;
    if (g_hist_n > 0 && strcmp(g_hist[(g_hist_n-1) % KSH_HIST_MAX], line) == 0) return;
    if (g_hist_n < KSH_HIST_MAX) {
        strlcpy(g_hist[g_hist_n], line, sizeof(g_hist[g_hist_n]));
        g_hist_n++;
    } else {
        for (int i = 0; i < KSH_HIST_MAX - 1; i++)
            strlcpy(g_hist[i], g_hist[i+1], sizeof(g_hist[i]));
        strlcpy(g_hist[KSH_HIST_MAX-1], line, sizeof(g_hist[KSH_HIST_MAX-1]));
    }
    g_hist_pos = g_hist_n;
}

static const char *alias_lookup(const char *name)
{
    for (int i = 0; i < g_nalias; i++)
        if (strcmp(g_alias_name[i], name) == 0)
            return g_alias_val[i];
    return NULL;
}

void ksh_dispatch(const char *input)
{
    while (*input == ' ' || *input == '\t') input++;
    if (!*input || *input == '#') return;

    char buf[KSH_LINE_MAX];
    strlcpy(buf, input, sizeof(buf));

    int argc = 0;
    char *argv[KSH_MAX_ARGS];
    split(buf, &argc, argv, KSH_MAX_ARGS);
    if (!argc) return;

    const char *expanded = alias_lookup(argv[0]);
    if (expanded) {
        char abuf[KSH_LINE_MAX];
        strlcpy(abuf, expanded, sizeof(abuf));
        for (int i = 1; i < argc; i++) {
            size_t al = strlen(abuf);
            if (al + 1 + strlen(argv[i]) < sizeof(abuf) - 1) {
                abuf[al] = ' ';
                strlcpy(abuf + al + 1, argv[i], sizeof(abuf) - al - 1);
            }
        }
        ksh_dispatch(abuf);
        return;
    }

    for (int i = 0; g_cmds[i].name; i++) {
        if (strcmp(argv[0], g_cmds[i].name) == 0) {
            g_cmds[i].handler(argc, argv);
            return;
        }
    }
    kprintf("ksh: %s: command not found\n", argv[0]);
}

static void ksh_readline(char *buf, size_t n)
{
    size_t pos = 0;
    buf[0] = '\0';
    for (;;) {
        int c = kbd_getc();
        if (c <= 0) { sched_yield(); continue; }
        if (c == '\r' || c == '\n') { kputs("\n"); buf[pos] = '\0'; return; }
        if (c == 127 || c == '\b') {
            if (pos > 0) { pos--; kputs("\b \b"); }
            continue;
        }
        if (c == 0x1B) {
            int c2 = kbd_getc();
            if (c2 == '[') {
                int c3 = kbd_getc();
                if (c3 == 'A' && g_hist_pos > 0) {
                    g_hist_pos--;
                    while (pos > 0) { kputs("\b \b"); pos--; }
                    strlcpy(buf, g_hist[g_hist_pos], n);
                    pos = strlen(buf);
                    kputs(buf);
                } else if (c3 == 'B' && g_hist_pos < g_hist_n - 1) {
                    g_hist_pos++;
                    while (pos > 0) { kputs("\b \b"); pos--; }
                    strlcpy(buf, g_hist[g_hist_pos], n);
                    pos = strlen(buf);
                    kputs(buf);
                }
            }
            continue;
        }
        if (c == 3) { pos=0; buf[0]='\0'; kputs("^C\n"); return; }
        if (pos < n - 1) {
            buf[pos++] = (char)c;
            char ch[2] = {(char)c, '\0'};
            kputs(ch);
        }
    }
}

__attribute__((noreturn))
void ksh_run(void)
{
    static char line[KSH_LINE_MAX];
    kputs("\nType 'help' for available commands.\n\n");
    for (;;) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("$$");
        vga_reset_color();
        vga_puts(" ");
        uart_puts("\033[32m$$\033[0m ");
        ksh_readline(line, sizeof(line));
        const char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') continue;
        hist_push(p);
        ksh_dispatch(p);
    }
}