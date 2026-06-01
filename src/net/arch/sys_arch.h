/* Copyright (C) 2026  Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * src/net/arch/sys_arch.h — lwIP sys_arch type definitions for Kobalt.
 *
 * lwIP includes this file (via <arch/sys_arch.h>) to get the concrete
 * types for sys_sem_t, sys_mutex_t, sys_mbox_t, and sys_thread_t.
 *
 * All synchronisation primitives are trivial single-core stubs — Kobalt
 * calls lwIP cooperatively and never re-enters it from an interrupt while
 * it is running.
 */

#ifndef KOBALT_SYS_ARCH_H
#define KOBALT_SYS_ARCH_H

#include <lwip/opt.h>   /* SYS_MBOX_SIZE, u32_t, u8_t */

/* ── Semaphore ────────────────────────────────────────────────────────────── */
typedef struct {
    volatile unsigned int c;    /* count */
} sys_sem_t;

/* ── Mutex ────────────────────────────────────────────────────────────────── */
typedef struct {
    volatile unsigned int c;    /* 1 = unlocked, 0 = locked */
} sys_mutex_t;

/* ── Mailbox ──────────────────────────────────────────────────────────────── */
#ifndef SYS_MBOX_SIZE
#define SYS_MBOX_SIZE   32
#endif

typedef struct {
    void                *buf[SYS_MBOX_SIZE];
    volatile unsigned int head;
    volatile unsigned int tail;
    volatile unsigned int count;
} sys_mbox_t;

/* ── Thread ───────────────────────────────────────────────────────────────── */
typedef unsigned int sys_thread_t;

/* sys_prot_t — defined in arch/cc.h as uintptr_t, do not redefine here.   */
/* SYS_MBOX_EMPTY and SYS_ARCH_TIMEOUT — defined in lwip/sys.h, not here. */

#endif /* KOBALT_SYS_ARCH_H */