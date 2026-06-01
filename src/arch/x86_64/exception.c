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

#include "exception.h"
#include "idt.h"
#include "gdt.h"
#include "../../inc/kernel.h"
#include <sched.h>
#include <ktrace.h>

static const char *exc_names[] = {
    "#DE", "#DB", "#NMI", "#BP", "#OF", "#BR", "#UD", "#NM",
    "#DF", "#CSO","#TS", "#NP", "#SS", "#GP", "#PF", "#RSV",
    "#MF", "#AC", "#MC", "#XM", "#VE", "#CP", "#22", "#23",
    "#24", "#25", "#26", "#27", "#HV", "#VC", "#SX", "#31",
};

static void dump_regs(pt_regs_t *r)
{
    kprintf("  RIP=%016llx  CS=%04llx  RFLAGS=%016llx\n",
            r->rip, r->cs, r->rflags);
    kprintf("  RSP=%016llx  SS=%04llx\n", r->rsp, r->ss);
    kprintf("  RAX=%016llx  RBX=%016llx  RCX=%016llx\n",
            r->rax, r->rbx, r->rcx);
    kprintf("  RDX=%016llx  RSI=%016llx  RDI=%016llx\n",
            r->rdx, r->rsi, r->rdi);
    kprintf("  R8 =%016llx  R9 =%016llx  R10=%016llx\n",
            r->r8,  r->r9,  r->r10);
    kprintf("  R11=%016llx  R12=%016llx  R13=%016llx\n",
            r->r11, r->r12, r->r13);
    kprintf("  R14=%016llx  R15=%016llx  RBP=%016llx\n",
            r->r14, r->r15, r->rbp);
}

static __attribute__((noreturn)) void exc_fatal(pt_regs_t *r)
{
    const char *name = (r->vector < 32) ? exc_names[r->vector] : "???";
    kprintf("\n*** EXCEPTION %llu %s  ec=%016llx\n",
            r->vector, name, r->error_code);
    dump_regs(r);
    kstack_trace();
    for (;;) __asm__ volatile ("cli; hlt" ::: "memory");
}

static void handle_bp(pt_regs_t *r)
{
    kprintf("[dbg] #BP at RIP=%016llx\n", r->rip);

}

static void handle_gp(pt_regs_t *r)
{

    if (r->cs & 3) {
        kprintf("[exc] #GP in user thread (ec=%016llx RIP=%016llx) -- killing\n",
                r->error_code, r->rip);
        sched_kill_current();
    }
    exc_fatal(r);
}

static void handle_ud(pt_regs_t *r)
{
    if (r->cs & 3) {
        kprintf("[exc] #UD in user thread (RIP=%016llx) -- killing\n", r->rip);
        sched_kill_current();
    }
    exc_fatal(r);
}

static void handle_df(pt_regs_t *r)
{

    kprintf("\n*** DOUBLE FAULT\n");
    dump_regs(r);
    for (;;) __asm__ volatile ("cli; hlt" ::: "memory");
}

static void handle_ac(pt_regs_t *r)
{
    if (r->cs & 3) {
        kprintf("[exc] #AC in user thread (RIP=%016llx) -- killing\n", r->rip);
        sched_kill_current();
    }
    exc_fatal(r);
}

void exception_dispatch(pt_regs_t *r)
{
    switch (r->vector) {
    case 1:

        break;
    case 3:  handle_bp(r);  break;
    case 6:  handle_ud(r);  break;
    case 8:  handle_df(r);  break;
    case 13: handle_gp(r);  break;
    case 17: handle_ac(r);  break;
    default: exc_fatal(r);  break;
    }
}

void exception_init(void)
{
    static void (*stubs[])(void) = {
        exc_entry_0,  exc_entry_1,  exc_entry_2,  exc_entry_3,
        exc_entry_4,  exc_entry_5,  exc_entry_6,  exc_entry_7,
        exc_entry_8,  exc_entry_9,  exc_entry_10, exc_entry_11,
        exc_entry_12, exc_entry_13, exc_entry_14, exc_entry_15,
        exc_entry_16, exc_entry_17, exc_entry_18, exc_entry_19,
        exc_entry_20, exc_entry_21, exc_entry_22, exc_entry_23,
        exc_entry_24, exc_entry_25, exc_entry_26, exc_entry_27,
        exc_entry_28, exc_entry_29, exc_entry_30, exc_entry_31,
    };

    uint8_t types[32];
    for (int i = 0; i < 32; i++) types[i] = IDT_GATE_INTERRUPT;
    types[3] = IDT_GATE_TRAP;
    types[4] = IDT_GATE_TRAP;

    uint8_t ist[32] = {0};
    ist[2]  = IDT_IST_NMI;
    ist[8]  = IDT_IST_DF;
    ist[18] = IDT_IST_MCE;

    for (unsigned v = 0; v < 32; v++)
        idt_set_gate((uint8_t)v, (uintptr_t)stubs[v],
                     GDT_KCODE64, types[v], ist[v]);
}
