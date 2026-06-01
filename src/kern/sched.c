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

#include <sched.h>
#include <runqueue.h>
#include <hrtimer.h>
#include <apic_timer.h>
#include "../arch/x86_64/idt.h"
#include "../arch/x86_64/gdt.h"
#include <kmalloc.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <kssh.h>
#include <amx_sched.h>

extern void sched_context_switch(sched_thread_t *prev, sched_thread_t *next);
extern void sched_thread_enter(void);
extern void sched_ipi_entry(void);

#include <ipi.h>
#define SCHED_IPI_VECTOR 0xee

_Static_assert(offsetof(sched_thread_t, kernel_rsp) == 0,
               "kernel_rsp must be at offset 0");
_Static_assert(offsetof(sched_thread_t, kstack_top) == 8,
               "kstack_top must be at offset 8");

#define CLI_SAVE(f)    __asm__ volatile ("pushfq; pop %0; cli" : "=rm"(f) :: "memory")
#define STI_RESTORE(f) __asm__ volatile ("push %0; popfq"     :: "rm"(f)  : "memory", "cc")

runqueue_t rqs[SCHED_MAX_CPUS];

static volatile int sched_up;
static uint64_t g_sched_ticks;

static uint32_t next_tid = 1;
static inline uint32_t alloc_tid(void)
{
    return __atomic_fetch_add(&next_tid, 1, __ATOMIC_RELAXED);
}

static sched_thread_t *g_tlist;
static spinlock_t      g_tlist_lock;

static void tlist_add(sched_thread_t *t)
{
    uint64_t f = spin_lock_irqsave(&g_tlist_lock);
    t->list_next = g_tlist;
    g_tlist = t;
    spin_unlock_irqrestore(&g_tlist_lock, f);
}

static void tlist_remove(sched_thread_t *t)
{
    uint64_t f = spin_lock_irqsave(&g_tlist_lock);
    sched_thread_t **p = &g_tlist;
    while (*p && *p != t) p = &(*p)->list_next;
    if (*p) *p = t->list_next;
    spin_unlock_irqrestore(&g_tlist_lock, f);
}

#define IDLE_STACK_SZ  0x2000U
static uint8_t        idle_stacks[SCHED_MAX_CPUS][IDLE_STACK_SZ] __attribute__((aligned(16)));
static sched_thread_t idle_threads[SCHED_MAX_CPUS];
static sched_thread_t boot_thread;

static inline uint64_t slice_ms(uint32_t weight)
{
    uint64_t ms = (uint64_t)SLICE_VTIME * weight / NICE_0_LOAD;
    return ms ? ms : 1;
}

static int thread_cmp(struct rb_node *a, struct rb_node *b)
{
    sched_thread_t *ta = rb_entry(a, sched_thread_t, rb);
    sched_thread_t *tb = rb_entry(b, sched_thread_t, rb);
    if (ta->vdeadline != tb->vdeadline)
        return (ta->vdeadline < tb->vdeadline) ? -1 : 1;
    return (ta->tid < tb->tid) ? -1 : (ta->tid > tb->tid) ? 1 : 0;
}

static void rq_insert(runqueue_t *rq, sched_thread_t *t)
{
    rb_insert(&rq->tree, &t->rb, thread_cmp);
    rq->nr_running++;
}

static void rq_remove(runqueue_t *rq, sched_thread_t *t)
{
    rb_erase(&rq->tree, &t->rb);
    rq->nr_running--;
}

static int64_t rq_min_vr(runqueue_t *rq)
{
    int64_t min = INT64_MAX;

    if (rq->current && rq->current != rq->idle)
        min = rq->current->vruntime;

    for (struct rb_node *n = rb_first(&rq->tree); n; n = rb_next(n)) {
        int64_t vr = rb_entry(n, sched_thread_t, rb)->vruntime;
        if (vr < min) min = vr;
    }
    return (min == INT64_MAX) ? rq->min_vruntime : min;
}

static void enqueue(runqueue_t *rq, sched_thread_t *t)
{
    int64_t mvr = rq_min_vr(rq);
    if (mvr > rq->min_vruntime) rq->min_vruntime = mvr;
    t->vstart    = (t->vruntime > rq->min_vruntime) ? t->vruntime : rq->min_vruntime;
    t->vdeadline = t->vstart + SLICE_VTIME;
    t->state     = THREAD_READY;
    rq_insert(rq, t);
}

static sched_thread_t *pick_next(runqueue_t *rq)
{
    if (!rq->nr_running) return rq->idle;

    int64_t mvr = rq->min_vruntime;

    for (struct rb_node *n = rb_first(&rq->tree); n; n = rb_next(n)) {
        sched_thread_t *t = rb_entry(n, sched_thread_t, rb);
        if (t->vstart <= mvr) return t;
    }

    sched_thread_t *best = NULL;
    for (struct rb_node *n = rb_first(&rq->tree); n; n = rb_next(n)) {
        sched_thread_t *t = rb_entry(n, sched_thread_t, rb);
        if (!best || t->vruntime < best->vruntime ||
            (t->vruntime == best->vruntime && t->tid < best->tid))
            best = t;
    }
    return best ? best : rq->idle;
}

static void do_switch(runqueue_t *rq, sched_thread_t *prev, sched_thread_t *next)
{
    next->state = THREAD_RUNNING;
    next->cpu   = (uint32_t)(rq - rqs);
    rq->current = next;
    spin_unlock(&rq->lock);

    amx_context_switch_out(prev);
    amx_context_switch_in(next);

    gdt_tss_set_rsp0(next->kstack_top);
    hrtimer_arm(slice_ms(next->weight) * 1000000ULL);
    sched_context_switch(prev, next);

    sched_thread_t *dead = rq->corpse;
    if (dead) {
        rq->corpse = NULL;
        if (dead->kstack_base) kfree(dead->kstack_base);
        kfree(dead);
    }
}

static void frame_init(sched_thread_t *t, thread_fn_t fn, void *arg)
{
    uint64_t *sp = (uint64_t *)t->kstack_top;
    *--sp = (uint64_t)(uintptr_t)sched_thread_enter;
    *--sp = (uint64_t)(uintptr_t)fn;
    *--sp = 0ULL;
    *--sp = (uint64_t)(uintptr_t)arg;
    *--sp = 0ULL; *--sp = 0ULL; *--sp = 0ULL;
    t->kernel_rsp = (uint64_t)(uintptr_t)sp;
}

static void load_balance(uint32_t dst_cpu)
{
    runqueue_t *dst = &rqs[dst_cpu];
    uint32_t src_cpu = UINT32_MAX, max_nr = 1;

    for (uint32_t i = 0; i < SCHED_MAX_CPUS; i++) {
        if (i == dst_cpu || !rqs[i].idle) continue;
        if (rqs[i].nr_running > max_nr) { max_nr = rqs[i].nr_running; src_cpu = i; }
    }
    if (src_cpu == UINT32_MAX) return;

    runqueue_t *lo = (dst_cpu < src_cpu) ? dst : &rqs[src_cpu];
    runqueue_t *hi = (dst_cpu < src_cpu) ? &rqs[src_cpu] : dst;

    if (!spin_trylock(&lo->lock)) return;
    if (!spin_trylock(&hi->lock)) { spin_unlock(&lo->lock); return; }

    runqueue_t *src = &rqs[src_cpu];
    if (src->nr_running > 1) {
        struct rb_node *n = src->tree.root;
        while (n && n->right) n = n->right;
        if (n) {
            sched_thread_t *t = rb_entry(n, sched_thread_t, rb);
            rq_remove(src, t);
            t->cpu       = dst_cpu;
            t->vstart    = (t->vruntime > dst->min_vruntime) ?
                            t->vruntime : dst->min_vruntime;
            t->vdeadline = t->vstart + SLICE_VTIME;
            t->state     = THREAD_READY;
            rq_insert(dst, t);
        }
    }

    spin_unlock(&hi->lock);
    spin_unlock(&lo->lock);
}

static void idle_fn(void *arg)
{
    (void)arg;
    uint32_t last_balance = 0;
    for (;;) {
        __asm__ volatile ("sti; hlt" ::: "memory");
        uint32_t now = (uint32_t)apic_timer_uptime_ms();
        if ((now - last_balance) >= 16u) {
            load_balance(cpu_id());
            last_balance = now;
        }
    }
}

static void init_idle(uint32_t cpu)
{
    static const char *names[] = {
        "idle/0","idle/1","idle/2","idle/3","idle/4","idle/5","idle/6","idle/7",
        "idle/8","idle/9","idle/10","idle/11","idle/12","idle/13","idle/14","idle/15"
    };
    sched_thread_t *idle = &idle_threads[cpu];
    memset(idle, 0, sizeof(*idle));
    idle->tid        = alloc_tid();
    idle->name       = names[cpu < SCHED_MAX_CPUS ? cpu : 0];
    idle->state      = THREAD_READY;
    idle->slice      = SCHED_DEFAULT_QUANTUM;
    idle->weight     = WEIGHT_DEFAULT;
    idle->inv_weight = NICE_0_LOAD / WEIGHT_DEFAULT;
    idle->cpu        = cpu;
    idle->kstack_top = (uint64_t)(uintptr_t)(idle_stacks[cpu] + IDLE_STACK_SZ);
    frame_init(idle, idle_fn, NULL);
    rqs[cpu].idle = idle;
}

void sched_init(uint64_t bsp_stack_top)
{
    memset(rqs, 0, sizeof(rqs));

    sched_thread_t *boot = &boot_thread;
    memset(boot, 0, sizeof(*boot));
    boot->tid        = alloc_tid();
    boot->name       = "boot";
    boot->state      = THREAD_RUNNING;
    boot->slice      = SCHED_DEFAULT_QUANTUM;
    boot->weight     = WEIGHT_DEFAULT;
    boot->inv_weight = NICE_0_LOAD / WEIGHT_DEFAULT;
    boot->vdeadline  = SLICE_VTIME;
    boot->kstack_top = bsp_stack_top;
    boot->cpu        = 0;

    rqs[0].current = boot;
    tlist_add(boot);
    init_idle(0);
    tlist_add(&idle_threads[0]);
    gdt_tss_set_rsp0(boot->kstack_top);

    idt_set_gate(SCHED_IPI_VECTOR,
                 (uintptr_t)sched_ipi_entry,
                 GDT_KCODE64, IDT_GATE_INTERRUPT, IDT_IST_NONE);

    __atomic_store_n(&sched_up, 1, __ATOMIC_RELEASE);
}

void sched_ap_init(uint32_t cpu)
{
    runqueue_t *rq = &rqs[cpu];
    memset(rq, 0, sizeof(*rq));
    init_idle(cpu);
    tlist_add(&idle_threads[cpu]);

    sched_thread_t *idle = rq->idle;
    idle->state = THREAD_RUNNING;
    rq->current = idle;
    gdt_tss_set_rsp0(idle->kstack_top);

    idt_set_gate(SCHED_IPI_VECTOR,
                 (uintptr_t)sched_ipi_entry,
                 GDT_KCODE64, IDT_GATE_INTERRUPT, IDT_IST_NONE);
}

sched_thread_t *sched_thread_create(const char *name, thread_fn_t fn,
                                    void *arg, uint32_t weight)
{
    if (!weight) weight = WEIGHT_DEFAULT;

    void *kstack = kmalloc(SCHED_KSTACK_SIZE);
    if (!kstack) return NULL;

    sched_thread_t *t = kmalloc(sizeof(*t));
    if (!t) { kfree(kstack); return NULL; }

    memset(t, 0, sizeof(*t));
    t->tid         = alloc_tid();
    t->name        = name;
    t->slice       = SCHED_DEFAULT_QUANTUM;
    t->weight      = weight;
    t->inv_weight  = NICE_0_LOAD / weight;
    t->kstack_base = kstack;
    t->kstack_size = SCHED_KSTACK_SIZE;
    t->kstack_top  = (uint64_t)(uintptr_t)((uint8_t *)kstack + SCHED_KSTACK_SIZE);
    frame_init(t, fn, arg);

    uint32_t best_cpu = UINT32_MAX, best_nr = UINT32_MAX;
    for (uint32_t i = 0; i < SCHED_MAX_CPUS; i++) {
        if (!rqs[i].idle) continue;
        if (rqs[i].nr_running < best_nr) { best_nr = rqs[i].nr_running; best_cpu = i; }
    }
    if (best_cpu == UINT32_MAX) best_cpu = 0;
    t->cpu = best_cpu;

    runqueue_t *rq = &rqs[best_cpu];
    uint64_t flags;
    CLI_SAVE(flags);
    spin_lock(&rq->lock);
    enqueue(rq, t);
    spin_unlock(&rq->lock);
    STI_RESTORE(flags);
    tlist_add(t);

    if (best_cpu != cpu_id())
        ipi_send_single(best_cpu, SCHED_IPI_VECTOR);

    return t;
}

void sched_thread_exit(void)
{
    uint64_t flags;
    CLI_SAVE(flags);
    (void)flags;

    runqueue_t     *rq    = this_rq();
    spin_lock(&rq->lock);
    sched_thread_t *dying = rq->current;
    dying->state = THREAD_ZOMBIE;

    sched_thread_t *next = pick_next(rq);

    if (dying->kstack_base) {
        tlist_remove(dying);
        amx_thread_free(dying);
        rq->corpse = dying;
    }

    if (next != rq->idle)
        rq_remove(rq, next);
    do_switch(rq, dying, next);
    __builtin_unreachable();
}

void sched_kill_current(void)
{
    sched_thread_t *cur = this_rq()->current;
    if (cur->fault_fn) cur->fault_fn(cur);
    sched_thread_exit();
}

void sched_yield(void)
{
    uint64_t flags;
    CLI_SAVE(flags);

    runqueue_t     *rq   = this_rq();
    spin_lock(&rq->lock);
    sched_thread_t *cur  = rq->current;
    sched_thread_t *next = pick_next(rq);

    if (next == rq->idle || next == cur) {
        cur->vdeadline = cur->vruntime + SLICE_VTIME;
        spin_unlock(&rq->lock);
        STI_RESTORE(flags);
        return;
    }

    cur->vruntime = cur->vdeadline;

    enqueue(rq, cur);
    rq_remove(rq, next);
    do_switch(rq, cur, next);
    STI_RESTORE(flags);
}

void sched_block(void)
{
    uint64_t flags;
    CLI_SAVE(flags);

    runqueue_t     *rq   = this_rq();
    spin_lock(&rq->lock);
    sched_thread_t *cur  = rq->current;
    sched_thread_t *next = pick_next(rq);
    cur->state = THREAD_BLOCKED;
    if (next != rq->idle)
        rq_remove(rq, next);
    do_switch(rq, cur, next);
    STI_RESTORE(flags);
}

void sched_unblock(sched_thread_t *t)
{
    if (!t || t->state != THREAD_BLOCKED) return;

    runqueue_t *rq = &rqs[t->cpu];
    spin_lock(&rq->lock);
    if (t->state == THREAD_BLOCKED)
        enqueue(rq, t);
    int target_idle = (rq->current == rq->idle);
    spin_unlock(&rq->lock);

    if (target_idle) {
        if (t->cpu != cpu_id())
            ipi_send_single(t->cpu, SCHED_IPI_VECTOR);
        else
            hrtimer_arm(HRTIMER_MIN_NS);
    }
}

sched_thread_t *sched_current(void)
{
    return this_rq()->current;
}

void sched_ipi_c_handler(void)
{
    apic_send_eoi();
    if (__atomic_load_n(&sched_up, __ATOMIC_ACQUIRE))
        kssh_tick_notify(apic_timer_ticks());
    sched_tick();
}

void sched_tick(void)
{
    if (!__atomic_load_n(&sched_up, __ATOMIC_ACQUIRE))
        return;

    runqueue_t     *rq  = this_rq();
    spin_lock(&rq->lock);
    sched_thread_t *cur = rq->current;
    if (!cur) { spin_unlock(&rq->lock); return; }

    __atomic_fetch_add(&g_sched_ticks, 1, __ATOMIC_RELAXED);
    cur->vruntime = cur->vdeadline;

    if (cur != rq->idle) {
        int64_t mvr = rq_min_vr(rq);
        if (mvr > rq->min_vruntime) rq->min_vruntime = mvr;
    }

    if (cur == rq->idle) {
        sched_thread_t *next = pick_next(rq);
        if (next != rq->idle) {
            rq->idle->state = THREAD_READY;
            rq_remove(rq, next);
            do_switch(rq, rq->idle, next);
        } else {
            spin_unlock(&rq->lock);
            hrtimer_arm(slice_ms(WEIGHT_DEFAULT) * 1000000ULL);
        }
        return;
    }

    sched_thread_t *next = pick_next(rq);
    if (next == rq->idle) {
        cur->vdeadline = cur->vruntime + SLICE_VTIME;
        spin_unlock(&rq->lock);
        hrtimer_arm(slice_ms(cur->weight) * 1000000ULL);
        return;
    }

    enqueue(rq, cur);
    rq_remove(rq, next);
    do_switch(rq, cur, next);
}

uint32_t sched_thread_count(void)
{
    uint32_t n = 0;
    uint64_t f = spin_lock_irqsave(&g_tlist_lock);
    for (sched_thread_t *t = g_tlist; t; t = t->list_next)
        n++;
    spin_unlock_irqrestore(&g_tlist_lock, f);
    return n;
}

sched_thread_t *sched_get_thread(uint32_t idx)
{
    uint64_t f = spin_lock_irqsave(&g_tlist_lock);
    sched_thread_t *t = g_tlist;
    for (uint32_t i = 0; t && i < idx; i++)
        t = t->list_next;
    spin_unlock_irqrestore(&g_tlist_lock, f);
    return t;
}

sched_thread_t *sched_get_thread_by_tid(uint32_t tid)
{
    uint64_t f = spin_lock_irqsave(&g_tlist_lock);
    sched_thread_t *t = g_tlist;
    while (t && t->tid != tid)
        t = t->list_next;
    spin_unlock_irqrestore(&g_tlist_lock, f);
    return t;
}

uint32_t        sched_thread_get_tid(sched_thread_t *t)   { return t->tid; }
const char     *sched_thread_get_name(sched_thread_t *t)  { return t->name; }
uint32_t        sched_thread_get_state(sched_thread_t *t) { return (uint32_t)t->state; }

sched_thread_t *sched_thread_get(uint32_t idx)  { return sched_get_thread(idx); }
sched_thread_t *sched_thread_find(uint32_t tid) { return sched_get_thread_by_tid(tid); }
uint64_t        sched_tick_count(void)          { return __atomic_load_n(&g_sched_ticks, __ATOMIC_RELAXED); }

void sched_thread_signal(sched_thread_t *t, int sig)
{
    (void)sig;
    if (t && t->state == THREAD_BLOCKED)
        sched_unblock(t);
}

void sched_set_priority(sched_thread_t *t, int prio)
{
    if (!t) return;
    if (prio < -20) prio = -20;
    if (prio >  19) prio =  19;
    static const uint32_t ptw[40] = {
        88761,71755,56483,46273,36291,29154,23254,18705,14949,11916,
         9548, 7620, 6100, 4904, 3906, 3121, 2501, 1991, 1586, 1277,
         1024,  820,  655,  526,  423,  335,  272,  215,  172,  137,
          110,   87,   70,   56,   45,   36,   29,   23,   18,   15,
    };
    uint32_t w = ptw[prio + 20];
    t->weight     = w;
    t->inv_weight = NICE_0_LOAD / w;
}

void sched_migrate(sched_thread_t *t, uint32_t cpu)
{
    if (!t || cpu >= SCHED_MAX_CPUS || !rqs[cpu].idle) return;

    runqueue_t *src = &rqs[t->cpu];
    runqueue_t *dst = &rqs[cpu];
    if (src == dst) return;

    runqueue_t *lo = (t->cpu < cpu) ? src : dst;
    runqueue_t *hi = (t->cpu < cpu) ? dst : src;

    uint64_t flags;
    CLI_SAVE(flags);
    spin_lock(&lo->lock);
    spin_lock(&hi->lock);

    if (t->state == THREAD_READY) {
        rq_remove(src, t);
        t->cpu       = cpu;
        t->vstart    = (t->vruntime > dst->min_vruntime) ? t->vruntime : dst->min_vruntime;
        t->vdeadline = t->vstart + SLICE_VTIME;
        rq_insert(dst, t);
    } else {
        t->cpu = cpu;
    }

    spin_unlock(&hi->lock);
    spin_unlock(&lo->lock);
    STI_RESTORE(flags);
}

void sched_set_affinity(sched_thread_t *t, uint64_t mask) { (void)t; (void)mask; }
uint64_t sched_get_affinity(sched_thread_t *t) { (void)t; return ~0ULL; }
