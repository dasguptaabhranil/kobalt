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

#include "procfs.h"
#include <kernel.h>
#include <kmalloc.h>
#include <sched.h>
#include <smp.h>
#include <string.h>

#define KOBALT_OS       "Kobalt"
#define KOBALT_VERSION  "1.0.0-b"
#define KOBALT_BUILD    "#1 SMP"
#define KOBALT_ARCH     "x86_64"
#define KOBALT_HOSTNAME "kobalt"

extern uint32_t sys_now(void);

static ssize_t fill_version(char *buf, size_t sz, void *arg)
{
    (void)arg;
    return (ssize_t)ksnprintf(buf, sz,
        "%s %s %s %s %s\n",
        KOBALT_OS, KOBALT_VERSION, KOBALT_BUILD, KOBALT_ARCH, KOBALT_HOSTNAME);
}

static ssize_t fill_uptime(char *buf, size_t sz, void *arg)
{
    (void)arg;
    uint32_t ms   = sys_now();
    uint32_t secs = ms / 1000u;
    uint32_t frac = (ms % 1000u) / 10u;
    return (ssize_t)ksnprintf(buf, sz, "%u.%02u 0.00\n", secs, frac);
}

static ssize_t fill_cmdline(char *buf, size_t sz, void *arg)
{
    (void)arg;
    return (ssize_t)ksnprintf(buf, sz, "\n");
}

static ssize_t fill_cpuinfo(char *buf, size_t sz, void *arg)
{
    (void)arg;
    uint32_t ncpus = smp_cpu_count();
    size_t   pos   = 0;
    for (uint32_t i = 0; i < ncpus && pos < sz - 1; i++) {
        pos += (size_t)ksnprintf(buf + pos, sz - pos,
            "processor\t: %u\n"
            "vendor_id\t: KobaltArch\n"
            "model name\t: Kobalt x86_64\n"
            "cpu MHz\t\t: 0.000\n"
            "cache size\t: 0 KB\n"
            "cpu cores\t: %u\n"
            "\n",
            i, ncpus);
    }
    return (ssize_t)pos;
}

static ssize_t fill_meminfo(char *buf, size_t sz, void *arg)
{
    (void)arg;
    size_t total_kb = 0, free_kb = 0;
    for (unsigned int i = 0; i < 9; i++) {
        size_t t = 0, f = 0;
        kmalloc_stats(i, &t, &f);
        total_kb += t;
        free_kb  += f;
    }
    total_kb /= 1024;
    free_kb  /= 1024;

    return (ssize_t)ksnprintf(buf, sz,
        "MemTotal:       %8zu kB\n"
        "MemFree:        %8zu kB\n"
        "MemAvailable:   %8zu kB\n"
        "Buffers:               0 kB\n"
        "Cached:                0 kB\n"
        "SwapTotal:             0 kB\n"
        "SwapFree:              0 kB\n",
        total_kb, free_kb, free_kb);
}

static ssize_t fill_mounts(char *buf, size_t sz, void *arg)
{
    (void)arg;
    return (ssize_t)ksnprintf(buf, sz,
        "procfs /proc procfs ro 0 0\n"
        "tmpfs /tmp tmpfs rw 0 0\n"
        "devfs /dev devfs rw 0 0\n"
        "flatfs / flatfs rw 0 0\n");
}

static ssize_t fill_stat(char *buf, size_t sz, void *arg)
{
    (void)arg;
    uint32_t ncpus = smp_cpu_count();
    size_t pos = 0;

    pos += (size_t)ksnprintf(buf + pos, sz - pos,
        "cpu  0 0 0 0 0 0 0 0 0 0\n");

    for (uint32_t i = 0; i < ncpus && pos < sz - 1; i++)
        pos += (size_t)ksnprintf(buf + pos, sz - pos,
            "cpu%u 0 0 0 0 0 0 0 0 0 0\n", i);

    pos += (size_t)ksnprintf(buf + pos, sz - pos,
        "intr 0\n"
        "ctxt 0\n"
        "btime 0\n"
        "processes %u\n"
        "procs_running 1\n"
        "procs_blocked 0\n",
        sched_thread_count());

    return (ssize_t)pos;
}

static ssize_t fill_loadavg(char *buf, size_t sz, void *arg)
{
    (void)arg;
    uint32_t n = sched_thread_count();
    return (ssize_t)ksnprintf(buf, sz, "0.00 0.00 0.00 1/%u 0\n", n ? n : 1);
}

static ssize_t fill_tid_status(char *buf, size_t sz, void *arg)
{
    uint32_t tid = (uint32_t)(uintptr_t)arg;
    sched_thread_t *t = sched_get_thread_by_tid(tid);
    if (!t)
        return (ssize_t)ksnprintf(buf, sz, "Name:\t(gone)\nState:\tZ\nPid:\t%u\n", tid);

    const char *state_str = "S";
    uint32_t    st        = sched_thread_get_state(t);
    if      (st == SCHED_STATE_RUNNING)  state_str = "R";
    else if (st == SCHED_STATE_RUNNABLE) state_str = "R";
    else if (st == SCHED_STATE_BLOCKED)  state_str = "S";
    else if (st == SCHED_STATE_DEAD)     state_str = "Z";

    return (ssize_t)ksnprintf(buf, sz,
        "Name:\t%s\n"
        "State:\t%s\n"
        "Pid:\t%u\n"
        "PPid:\t0\n"
        "Threads:\t1\n"
        "VmRSS:\t0 kB\n",
        sched_thread_get_name(t),
        state_str,
        tid);
}

static ssize_t fill_tid_comm(char *buf, size_t sz, void *arg)
{
    uint32_t tid = (uint32_t)(uintptr_t)arg;
    sched_thread_t *t = sched_get_thread_by_tid(tid);
    if (!t)
        return (ssize_t)ksnprintf(buf, sz, "(gone)\n");
    return (ssize_t)ksnprintf(buf, sz, "%s\n", sched_thread_get_name(t));
}

static ssize_t fill_tid_wchan(char *buf, size_t sz, void *arg)
{
    (void)arg;
    return (ssize_t)ksnprintf(buf, sz, "0\n");
}

proc_node_t *procfs_open_tid_file(uint32_t tid, const char *fname)
{
    proc_fill_t fill = NULL;
    if      (strcmp(fname, "status") == 0) fill = fill_tid_status;
    else if (strcmp(fname, "comm")   == 0) fill = fill_tid_comm;
    else if (strcmp(fname, "wchan")  == 0) fill = fill_tid_wchan;
    if (!fill) return NULL;

    proc_node_t *n = kmalloc(sizeof(*n));
    if (!n) return NULL;
    memset(n, 0, sizeof(*n));
    n->ino       = PROC_INO_TID_BASE + tid;
    n->type      = PROC_DT_FILE;
    n->transient = 1;
    n->fill      = fill;
    n->arg       = (void *)(uintptr_t)tid;
    return n;
}

void procfs_populate(procfs_t *pfs)
{
    proc_node_t *r = pfs->root;

    procfs_mkfile(pfs, r, "version", fill_version, NULL);
    procfs_mkfile(pfs, r, "uptime",  fill_uptime,  NULL);
    procfs_mkfile(pfs, r, "cmdline", fill_cmdline, NULL);
    procfs_mkfile(pfs, r, "cpuinfo", fill_cpuinfo, NULL);
    procfs_mkfile(pfs, r, "meminfo", fill_meminfo, NULL);
    procfs_mkfile(pfs, r, "mounts",  fill_mounts,  NULL);
    procfs_mkfile(pfs, r, "stat",    fill_stat,    NULL);
    procfs_mkfile(pfs, r, "loadavg", fill_loadavg, NULL);

    pfs->n_static = procfs_child_count(r);
}
