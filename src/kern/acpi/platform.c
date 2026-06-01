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

#include <uacpi/kernel_api.h>
#include "../../inc/kmalloc.h"
#include "../../inc/spinlock.h"
#include "../../inc/irq.h"
#include "../../inc/pci.h"
#include "../../inc/kernel.h"
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include <string.h>

static int rsdp_sig(const uint8_t *p)
{
    return p[0]=='R' && p[1]=='S' && p[2]=='D' && p[3]==' ' &&
           p[4]=='P' && p[5]=='T' && p[6]=='R' && p[7]==' ';
}

static uint8_t bsum(const void *d, size_t n)
{
    const uint8_t *p = d;
    uint8_t s = 0;
    for (size_t i = 0; i < n; i++) s += p[i];
    return s;
}

static const void *scan_rsdp(uintptr_t start, uintptr_t end)
{
    for (uintptr_t a = start; a < end; a += 16) {
        const uint8_t *p = (const uint8_t *)a;
        if (!rsdp_sig(p)) continue;
        if (bsum(p, 20))  continue;
        if (p[15] >= 2 && bsum(p, 36)) continue;
        return p;
    }
    return NULL;
}

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out)
{
    uint16_t seg = *(volatile uint16_t *)0x040Eu;
    const void *r = NULL;
    if (seg) r = scan_rsdp((uintptr_t)seg << 4, ((uintptr_t)seg << 4) + 1024);
    if (!r)  r = scan_rsdp(0xE0000u, 0x100000u);
    if (!r)  return UACPI_STATUS_NOT_FOUND;
    *out = (uacpi_phys_addr)(uintptr_t)r;
    return UACPI_STATUS_OK;
}

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len)
{
    (void)len;
    return (void *)(uintptr_t)addr;
}

void uacpi_kernel_unmap(void *addr, uacpi_size len)
{
    (void)addr; (void)len;
}

void *uacpi_kernel_alloc(uacpi_size sz)
{
    return kmalloc(sz);
}

void *uacpi_kernel_alloc_zeroed(uacpi_size sz)
{
    void *p = kmalloc(sz);
    if (p) memset(p, 0, sz);
    return p;
}

void uacpi_kernel_free(void *p)
{
    kfree(p);
}

void uacpi_kernel_log(uacpi_log_level lvl, const uacpi_char *msg)
{
    (void)lvl;
    uart_puts(msg);
}

uacpi_thread_id uacpi_kernel_get_thread_id(void)
{
    return (uacpi_thread_id)1;
}

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uacpi_u64)hi << 32) | lo;
}

void uacpi_kernel_stall(uacpi_u8 usec)
{
    for (volatile uint32_t i = 0; i < (uint32_t)usec * 1000u; i++);
}

void uacpi_kernel_sleep(uacpi_u64 msec)
{
    for (volatile uint64_t i = 0; i < msec * 1000000ULL; i++);
}

uacpi_handle uacpi_kernel_create_mutex(void)
{
    spinlock_t *l = kmalloc(sizeof *l);
    if (l) spinlock_init(l);
    return l;
}

void uacpi_kernel_free_mutex(uacpi_handle h)
{
    kfree(h);
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle h, uacpi_u16 timeout)
{
    (void)timeout;
    spin_lock((spinlock_t *)h);
    return UACPI_STATUS_OK;
}

void uacpi_kernel_release_mutex(uacpi_handle h)
{
    spin_unlock((spinlock_t *)h);
}

typedef struct { atomic_uint count; } kevent_t;

uacpi_handle uacpi_kernel_create_event(void)
{
    kevent_t *e = kmalloc(sizeof *e);
    if (e) atomic_store(&e->count, 0u);
    return e;
}

void uacpi_kernel_free_event(uacpi_handle h)
{
    kfree(h);
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle h, uacpi_u16 timeout)
{
    kevent_t *e = h;
    if (timeout == 0xFFFFu) {
        while (!atomic_load(&e->count))
            __asm__ volatile("pause");
        atomic_fetch_sub(&e->count, 1u);
        return UACPI_TRUE;
    }
    if (atomic_load(&e->count)) {
        atomic_fetch_sub(&e->count, 1u);
        return UACPI_TRUE;
    }
    return UACPI_FALSE;
}

void uacpi_kernel_signal_event(uacpi_handle h)
{
    atomic_fetch_add(&((kevent_t *)h)->count, 1u);
}

void uacpi_kernel_reset_event(uacpi_handle h)
{
    atomic_store(&((kevent_t *)h)->count, 0u);
}

uacpi_handle uacpi_kernel_create_spinlock(void)
{
    spinlock_t *l = kmalloc(sizeof *l);
    if (l) spinlock_init(l);
    return l;
}

void uacpi_kernel_free_spinlock(uacpi_handle h)
{
    kfree(h);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle h)
{
    return (uacpi_cpu_flags)spin_lock_irqsave((spinlock_t *)h);
}

void uacpi_kernel_unlock_spinlock(uacpi_handle h, uacpi_cpu_flags f)
{
    spin_unlock_irqrestore((spinlock_t *)h, (uint64_t)f);
}

uacpi_status uacpi_kernel_schedule_work(uacpi_work_type t,
                                        uacpi_work_handler fn,
                                        uacpi_handle ctx)
{
    (void)t;
    fn(ctx);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_wait_for_work_completion(void)
{
    return UACPI_STATUS_OK;
}

typedef struct {
    uacpi_interrupt_handler fn;
    uacpi_handle            ctx;
    uint8_t                 irq;
} kirq_t;

static void uacpi_irq_dispatch(uint8_t irq, void *arg)
{
    (void)irq;
    kirq_t *k = arg;
    k->fn(k->ctx);
}

uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, uacpi_interrupt_handler fn,
    uacpi_handle ctx, uacpi_handle *out)
{
    kirq_t *k = kmalloc(sizeof *k);
    if (!k) return UACPI_STATUS_OUT_OF_MEMORY;
    k->fn  = fn;
    k->ctx = ctx;
    k->irq = (uint8_t)irq;
    irq_register((uint8_t)irq,
                 (uint8_t)(IRQ_IOAPIC_VECTOR_BASE + irq),
                 uacpi_irq_dispatch, k, "uacpi");
    *out = k;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler fn, uacpi_handle h)
{
    (void)fn;
    kfree(h);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size len,
                                 uacpi_handle *out)
{
    (void)len;
    *out = (uacpi_handle)(uintptr_t)base;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle h) { (void)h; }

static inline uint16_t io_port(uacpi_handle h, uacpi_size off)
{
    return (uint16_t)((uintptr_t)h + off);
}

uacpi_status uacpi_kernel_io_read8(uacpi_handle h, uacpi_size off,
                                   uacpi_u8 *v)
{
    uint16_t p = io_port(h, off);
    __asm__ volatile("inb %w1, %b0" : "=a"(*v) : "Nd"(p));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle h, uacpi_size off,
                                    uacpi_u16 *v)
{
    uint16_t p = io_port(h, off);
    __asm__ volatile("inw %w1, %w0" : "=a"(*v) : "Nd"(p));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle h, uacpi_size off,
                                    uacpi_u32 *v)
{
    uint16_t p = io_port(h, off);
    __asm__ volatile("inl %w1, %0" : "=a"(*v) : "Nd"(p));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle h, uacpi_size off,
                                    uacpi_u8 v)
{
    uint16_t p = io_port(h, off);
    __asm__ volatile("outb %b0, %w1" :: "a"(v), "Nd"(p));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle h, uacpi_size off,
                                     uacpi_u16 v)
{
    uint16_t p = io_port(h, off);
    __asm__ volatile("outw %w0, %w1" :: "a"(v), "Nd"(p));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle h, uacpi_size off,
                                     uacpi_u32 v)
{
    uint16_t p = io_port(h, off);
    __asm__ volatile("outl %0, %w1" :: "a"(v), "Nd"(p));
    return UACPI_STATUS_OK;
}

typedef struct { uint8_t bus, dev, fn; } pci_bdf_t;

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address addr,
                                          uacpi_handle *out)
{
    pci_bdf_t *a = kmalloc(sizeof *a);
    if (!a) return UACPI_STATUS_OUT_OF_MEMORY;
    a->bus = (uint8_t)addr.bus;
    a->dev = (uint8_t)addr.device;
    a->fn  = (uint8_t)addr.function;
    *out = a;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle h)
{
    kfree(h);
}

#define PCI_TMP(h) \
    pci_bdf_t *_b = (h); \
    kobalt_pci_dev_t _d = { .bus = _b->bus, .device = _b->dev, \
                            .function = _b->fn }

uacpi_status uacpi_kernel_pci_read8(uacpi_handle h, uacpi_size off,
                                    uacpi_u8 *v)
{
    PCI_TMP(h);
    *v = pci_read_config8(&_d, (uint16_t)off);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle h, uacpi_size off,
                                     uacpi_u16 *v)
{
    PCI_TMP(h);
    *v = pci_read_config16(&_d, (uint16_t)off);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle h, uacpi_size off,
                                     uacpi_u32 *v)
{
    PCI_TMP(h);
    *v = pci_read_config32(&_d, (uint16_t)off);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle h, uacpi_size off,
                                     uacpi_u8 v)
{
    PCI_TMP(h);
    pci_write_config8(&_d, (uint16_t)off, v);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write16(uacpi_handle h, uacpi_size off,
                                      uacpi_u16 v)
{
    PCI_TMP(h);
    pci_write_config16(&_d, (uint16_t)off, v);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write32(uacpi_handle h, uacpi_size off,
                                      uacpi_u32 v)
{
    PCI_TMP(h);
    pci_write_config32(&_d, (uint16_t)off, v);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *r)
{
    switch (r->type) {
    case UACPI_FIRMWARE_REQUEST_TYPE_BREAKPOINT:
        __asm__ volatile("int3");
        break;
    case UACPI_FIRMWARE_REQUEST_TYPE_FATAL:
        uart_puts("uACPI: firmware fatal\n");
        __asm__ volatile("cli; hlt");
        break;
    default:
        return UACPI_STATUS_UNIMPLEMENTED;
    }
    return UACPI_STATUS_OK;
}
