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

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../inc/xhci.h"
#include "../inc/kmalloc.h"
#include "../inc/kernel.h"

int xhci_ring_init(xhci_ring_t *r)
{
    r->trbs = kmalloc(sizeof(xhci_trb_t) * XHCI_RING_SIZE);
    if (!r->trbs) return -1;
    memset(r->trbs, 0, sizeof(xhci_trb_t) * XHCI_RING_SIZE);
    r->enq = r->deq = 0;
    r->cycle = 1;

    xhci_trb_t *lnk = &r->trbs[XHCI_RING_SIZE - 1];
    lnk->param_lo = (uint32_t)(uintptr_t)r->trbs;
    lnk->param_hi = (uint32_t)((uintptr_t)r->trbs >> 32);
    lnk->control  = TRB_TYPE(TRB_TYPE_LINK) | TRB_LINK_TC | r->cycle;
    return 0;
}

void xhci_ring_free(xhci_ring_t *r)
{
    if (r->trbs) { kfree(r->trbs); r->trbs = NULL; }
    r->enq = r->deq = r->cycle = 0;
}

int xhci_ring_enqueue(xhci_ring_t *r, const xhci_trb_t *trb, int unused)
{
    (void)unused;
    if (!r->trbs) return -1;
    if (r->enq >= XHCI_RING_SIZE - 1) r->enq = 0;

    xhci_trb_t t = *trb;
    t.control = (t.control & ~TRB_CYCLE) | r->cycle;
    r->trbs[r->enq] = t;

    __asm__ volatile("mfence" ::: "memory");

    if (++r->enq >= XHCI_RING_SIZE - 1) {
        xhci_trb_t *lnk = &r->trbs[XHCI_RING_SIZE - 1];
        lnk->control = (lnk->control & ~TRB_CYCLE) | r->cycle;
        r->cycle ^= 1u;
        r->enq = 0;
    }
    return 0;
}

int xhci_evt_ring_init(xhci_ctrl_t *xc)
{
    xhci_ring_t *er = &xc->evt_ring;
    er->trbs = kmalloc(sizeof(xhci_trb_t) * XHCI_EVENT_RING_SIZE);
    if (!er->trbs) return -1;
    memset(er->trbs, 0, sizeof(xhci_trb_t) * XHCI_EVENT_RING_SIZE);
    er->enq = er->deq = 0;
    er->cycle = 1;

    xc->erst = kmalloc(sizeof(xhci_erst_entry_t) * XHCI_ERST_SEGMENTS);
    if (!xc->erst) { kfree(er->trbs); er->trbs = NULL; return -1; }
    memset(xc->erst, 0, sizeof(xhci_erst_entry_t) * XHCI_ERST_SEGMENTS);
    xc->erst[0].base     = (uint64_t)(uintptr_t)er->trbs;
    xc->erst[0].seg_size = XHCI_EVENT_RING_SIZE;
    return 0;
}

void xhci_evt_ring_advance(xhci_ctrl_t *xc)
{
    uint64_t erdp = (uint64_t)(uintptr_t)&xc->evt_ring.trbs[xc->evt_ring.deq]
                  | XHCI_ERDP_EHB;
    *(volatile uint32_t *)(xc->rt_base + XHCI_RT_ERDP(0))     = (uint32_t)erdp;
    *(volatile uint32_t *)(xc->rt_base + XHCI_RT_ERDP(0) + 4) = (uint32_t)(erdp >> 32);
}
