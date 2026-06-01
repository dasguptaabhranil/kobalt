/* Copyright (c) 2026  Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <arch/cc.h>
#include <kernel.h>     /* inb() / outb() */

/* Global errno required by lwIP socket layer (set_errno macro) */
int errno = 0;

/* ---------------------------------------------------------------------------
 * PIT-calibrated sys_now()
 *
 * lwIP's entire timer engine -- DHCP retransmit, ARP ageing, TCP keepalive --
 * depends on sys_now() returning milliseconds that track real wall-clock time.
 *
 * The old code hardcoded a 2.5 GHz TSC divisor.  On a QEMU/KVM host running
 * at 3-4 GHz the returned ms values were wrong by up to 60 %.  Under QEMU TCG
 * (software emulation), rdtsc barely advances during tight C loops, making
 * sys_check_timeouts() see near-zero elapsed time and never fire any lwIP
 * timer -- so DHCP retransmits never happen and a missed first DISCOVER is
 * never re-sent.
 *
 * Fix: measure the TSC against PIT channel 2 for 10 ms at boot.  This gives
 * a calibrated ticks-per-ms value that is correct on any host speed and under
 * both KVM and TCG.  Call tsc_calibrate() once from kmain() before lwip_init.
 * ------------------------------------------------------------------------- */

#define PIT_HZ          1193182UL
#define CAL_MS          10UL
#define PIT_CAL_TICKS   ((PIT_HZ * CAL_MS) / 1000UL)   /* 11931 */

/* Safe fallback: 2.5 GHz.  Overwritten by tsc_calibrate(). */
static uint32_t g_tsc_per_ms = 2500000u;

static inline uint64_t rdtsc64(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/*
 * tsc_calibrate() -- call once in kmain() before lwip_init().
 *
 * Programs PIT channel 2 in one-shot mode (mode 0) for CAL_MS milliseconds,
 * measures the elapsed TSC ticks, and stores ticks-per-ms in g_tsc_per_ms.
 * PIT channel 2 is the only channel safe to use here (channel 0 drives the
 * legacy IRQ0 timer; channel 1 is reserved on modern hardware).
 */
void tsc_calibrate(void)
{
    /* Gate channel 2 on; keep the PC speaker silent. */
    outb(0x61, (inb(0x61) & 0xFC) | 0x01);

    /* Channel 2, lo/hi byte, mode 0 (interrupt on terminal count). */
    outb(0x43, 0xB0);
    outb(0x42, (uint8_t)(PIT_CAL_TICKS & 0xFF));
    outb(0x42, (uint8_t)(PIT_CAL_TICKS >> 8));

    uint64_t t0 = rdtsc64();

    /* Spin until OUT2 (bit 5 of port 0x61) goes high -- counter expired. */
    while (!(inb(0x61) & 0x20))
        __asm__ volatile("pause");

    uint64_t t1 = rdtsc64();

    uint32_t measured = (uint32_t)((t1 - t0) / CAL_MS);

    /* Sanity check: accept 100 MHz - 10 GHz only. */
    if (measured > 100000u && measured < 10000000u)
        g_tsc_per_ms = measured;
}

uint32_t sys_now(void) {
    uint32_t low, high;
    __asm__ volatile ("rdtsc" : "=a"(low), "=d"(high));
    uint64_t tsc = ((uint64_t)high << 32) | low;
    /* Return TSC shifted down to ~ms range */
    return (uint32_t)(tsc >> 20); 
}

sys_prot_t sys_arch_protect(void)
{
    return 0;
}

void sys_arch_unprotect(sys_prot_t pval)
{
    (void)pval;
}

/* ===========================================================================
 * NO_SYS=0 sys_arch implementation — single-core cooperative stubs
 *
 * lwIP's socket/netconn layer requires NO_SYS=0 and a sys_arch that
 * provides semaphores, mutexes, and mailboxes.
 *
 * Kobalt is single-core and calls lwIP cooperatively (no preemption while
 * inside lwIP), so:
 *   - Semaphores are a single volatile counter polled with cpu_relax().
 *   - Mutexes are a single volatile flag (always "unlocked" after each op
 *     since we never re-enter lwIP).
 *   - Mailboxes are a fixed-size ring buffer.
 *
 * The SYS_ARCH_TIMEOUT / SYS_ARCH_NOWAIT sentinel values follow the lwIP
 * contract: return SYS_ARCH_TIMEOUT if the wait timed out, 0 if signalled.
 * =========================================================================*/

#include <lwip/sys.h>
#include <lwip/opt.h>

/* lwip/sys.h needs these from arch/cc.h — included transitively already */

/* ── Timeout polling budget ─────────────────────────────────────────────────
 * Each "iteration" below burns roughly 1 µs on a 1 GHz+ core (pause + check).
 * 1000 iterations ≈ 1 ms.  lwIP passes timeout_ms in milliseconds.
 */
#define SYS_ARCH_ITERS_PER_MS  1000u

static inline void cpu_relax_arch(void) {
    __asm__ volatile("pause" ::: "memory");
}

/* ── Semaphore ──────────────────────────────────────────────────────────────
 * A semaphore is a volatile u32 count.
 * sem_new initialises to initial_count.
 * sem_signal increments; sem_wait decrements (spinning until non-zero).
 */
err_t sys_sem_new(sys_sem_t *sem, u8_t initial_count)
{
    sem->c = initial_count;
    return ERR_OK;
}

void sys_sem_free(sys_sem_t *sem)
{
    sem->c = 0;
}

void sys_sem_signal(sys_sem_t *sem)
{
    __atomic_add_fetch(&sem->c, 1, __ATOMIC_RELEASE);
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout_ms)
{
    u32_t iters = 0;
    u32_t limit = timeout_ms ? (timeout_ms * SYS_ARCH_ITERS_PER_MS) : 0xFFFFFFFFu;

    while (1) {
        u32_t v = __atomic_load_n(&sem->c, __ATOMIC_ACQUIRE);
        if (v > 0) {
            __atomic_sub_fetch(&sem->c, 1, __ATOMIC_ACQUIRE);
            return 0;   /* success */
        }
        cpu_relax_arch();
        if (++iters >= limit)
            return SYS_ARCH_TIMEOUT;
    }
}

int sys_sem_valid(sys_sem_t *sem)
{
    return 1;   /* our sems are always embedded structs, never NULL */
}

void sys_sem_set_invalid(sys_sem_t *sem)
{
    sem->c = 0;
}

/* ── Mutex ──────────────────────────────────────────────────────────────────
 * Since Kobalt never re-enters lwIP from an interrupt while lwIP is running,
 * a mutex is trivially always unlocked from the perspective of any second
 * locker.  We implement it as a sem with count 1.
 */
err_t sys_mutex_new(sys_mutex_t *mutex)
{
    mutex->c = 1;
    return ERR_OK;
}

void sys_mutex_free(sys_mutex_t *mutex)
{
    mutex->c = 0;
}

void sys_mutex_lock(sys_mutex_t *mutex)
{
    /* spin until we decrement from 1 to 0 */
    while (1) {
        u32_t v = __atomic_load_n(&mutex->c, __ATOMIC_ACQUIRE);
        if (v > 0 && __atomic_compare_exchange_n(
                &mutex->c, &v, v - 1, 0,
                __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
            return;
        cpu_relax_arch();
    }
}

void sys_mutex_unlock(sys_mutex_t *mutex)
{
    __atomic_store_n(&mutex->c, 1, __ATOMIC_RELEASE);
}

int sys_mutex_valid(sys_mutex_t *mutex)
{
    return 1;
}

void sys_mutex_set_invalid(sys_mutex_t *mutex)
{
    mutex->c = 0;
}

/* ── Mailbox ────────────────────────────────────────────────────────────────
 * Fixed ring buffer of void* pointers.
 * SYS_MBOX_SIZE is defined in lwipopts.h (we set it to 32).
 */
err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
    (void)size;
    mbox->head  = 0;
    mbox->tail  = 0;
    mbox->count = 0;
    return ERR_OK;
}

void sys_mbox_free(sys_mbox_t *mbox)
{
    mbox->head = mbox->tail = mbox->count = 0;
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
    /* Block (spin) until space is available — in practice never blocks
     * because the kernel pumps lwIP cooperatively and the mbox drains
     * before it fills. */
    while (mbox->count >= SYS_MBOX_SIZE)
        cpu_relax_arch();

    mbox->buf[mbox->tail] = msg;
    mbox->tail = (mbox->tail + 1) % SYS_MBOX_SIZE;
    __atomic_add_fetch(&mbox->count, 1, __ATOMIC_RELEASE);
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
    if (mbox->count >= SYS_MBOX_SIZE)
        return ERR_MEM;
    sys_mbox_post(mbox, msg);
    return ERR_OK;
}

err_t sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg)
{
    return sys_mbox_trypost(mbox, msg);
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout_ms)
{
    u32_t iters = 0;
    u32_t limit = timeout_ms ? (timeout_ms * SYS_ARCH_ITERS_PER_MS) : 0xFFFFFFFFu;

    while (__atomic_load_n(&mbox->count, __ATOMIC_ACQUIRE) == 0) {
        cpu_relax_arch();
        if (++iters >= limit)
            return SYS_ARCH_TIMEOUT;
    }

    if (msg)
        *msg = mbox->buf[mbox->head];
    mbox->head = (mbox->head + 1) % SYS_MBOX_SIZE;
    __atomic_sub_fetch(&mbox->count, 1, __ATOMIC_ACQUIRE);
    return 0;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
    if (__atomic_load_n(&mbox->count, __ATOMIC_ACQUIRE) == 0)
        return SYS_MBOX_EMPTY;

    if (msg)
        *msg = mbox->buf[mbox->head];
    mbox->head = (mbox->head + 1) % SYS_MBOX_SIZE;
    __atomic_sub_fetch(&mbox->count, 1, __ATOMIC_ACQUIRE);
    return 0;
}

int sys_mbox_valid(sys_mbox_t *mbox)
{
    return 1;
}

void sys_mbox_set_invalid(sys_mbox_t *mbox)
{
    mbox->count = 0;
}

/* ── Thread stub (unused with NO_SYS=0 cooperative model) ─────────────────*/
sys_thread_t sys_thread_new(const char *name, lwip_thread_fn fn,
                            void *arg, int stacksize, int prio)
{
    (void)name; (void)fn; (void)arg; (void)stacksize; (void)prio;
    return 0;
}

/* ── sys_init ────────────────────────────────────────────────────────────── */
void sys_init(void)
{
    /* Nothing needed — tsc_calibrate() is called separately from kmain(). */
}