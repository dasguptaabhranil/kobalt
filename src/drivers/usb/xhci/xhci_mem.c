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

extern int  xhci_ring_init(xhci_ring_t *r);
extern void xhci_ring_free(xhci_ring_t *r);

int xhci_mem_init(xhci_ctrl_t *xc)
{
    size_t sz = sizeof(uint64_t) * ((size_t)xc->max_slots + 1u);
    xc->dcbaa = kmalloc(sz);
    if (!xc->dcbaa) return -1;
    memset(xc->dcbaa, 0, sz);
    memset(xc->dev_ctx,    0, sizeof(xc->dev_ctx));
    memset(xc->xfer_rings, 0, sizeof(xc->xfer_rings));
    return 0;
}

void xhci_mem_free(xhci_ctrl_t *xc)
{
    for (int s = 1; s <= xc->max_slots; s++) {
        if (xc->dev_ctx[s]) { kfree(xc->dev_ctx[s]); xc->dev_ctx[s] = NULL; }
        for (int e = 0; e < XHCI_MAX_EP; e++) {
            if (xc->xfer_rings[s][e]) {
                xhci_ring_free(xc->xfer_rings[s][e]);
                kfree(xc->xfer_rings[s][e]);
                xc->xfer_rings[s][e] = NULL;
            }
        }
    }
    if (xc->dcbaa)         { kfree(xc->dcbaa);         xc->dcbaa = NULL; }
    if (xc->evt_ring.trbs) { kfree(xc->evt_ring.trbs); xc->evt_ring.trbs = NULL; }
    if (xc->erst)          { kfree(xc->erst);           xc->erst  = NULL; }
}

int xhci_alloc_slot_ctx(xhci_ctrl_t *xc, uint8_t slot)
{
    if (!slot || slot > xc->max_slots) return -1;
    if (xc->dev_ctx[slot]) return 0;

    xhci_dev_ctx_t *dc = kmalloc(sizeof(xhci_dev_ctx_t));
    if (!dc) return -1;
    memset(dc, 0, sizeof(*dc));
    xc->dev_ctx[slot] = dc;
    xc->dcbaa[slot]   = (uint64_t)(uintptr_t)dc;
    return 0;
}

void xhci_free_slot_ctx(xhci_ctrl_t *xc, uint8_t slot)
{
    if (!slot || slot > xc->max_slots || !xc->dev_ctx[slot]) return;
    for (int e = 0; e < XHCI_MAX_EP; e++) {
        if (xc->xfer_rings[slot][e]) {
            xhci_ring_free(xc->xfer_rings[slot][e]);
            kfree(xc->xfer_rings[slot][e]);
            xc->xfer_rings[slot][e] = NULL;
        }
    }
    kfree(xc->dev_ctx[slot]);
    xc->dev_ctx[slot] = NULL;
    xc->dcbaa[slot]   = 0;
}

int xhci_alloc_xfer_ring(xhci_ctrl_t *xc, uint8_t slot, uint8_t ep_id)
{
    if (!slot || slot > xc->max_slots || !ep_id || ep_id >= XHCI_MAX_EP) return -1;
    if (xc->xfer_rings[slot][ep_id]) return 0;

    xhci_ring_t *r = kmalloc(sizeof(*r));
    if (!r) return -1;
    memset(r, 0, sizeof(*r));
    if (xhci_ring_init(r) < 0) { kfree(r); return -1; }
    xc->xfer_rings[slot][ep_id] = r;
    return 0;
}

xhci_input_ctx_t *xhci_alloc_input_ctx(void)
{
    xhci_input_ctx_t *ic = kmalloc(sizeof(xhci_input_ctx_t));
    if (ic) memset(ic, 0, sizeof(*ic));
    return ic;
}

void xhci_fill_ep_ctx(xhci_input_ctx_t *ic, uint8_t ep_id,
                       uint8_t ep_type, uint16_t mps, uint8_t interval,
                       xhci_ring_t *ring, uint8_t max_burst)
{
    if (!ic || !ring || !ep_id || ep_id >= XHCI_MAX_EP) return;

    xhci_ep_ctx_t *ep = &ic->ep[ep_id - 1];
    memset(ep, 0, sizeof(*ep));

    ep->dw0 = (uint32_t)interval << 16;
    ep->dw1 = ((uint32_t)ep_type << 3)
            | ((uint32_t)max_burst << 8)
            | ((uint32_t)mps << 16)
            | (3U << 1);

    uint64_t deq = (uint64_t)(uintptr_t)ring->trbs;
    ep->tr_deq_lo = (uint32_t)(deq & ~0xFULL) | EP_CTX_DCS;
    ep->tr_deq_hi = (uint32_t)(deq >> 32);
    ep->dw4       = (ep_type == EP_TYPE_CTRL) ? 8u : (uint32_t)mps;

    ic->ctrl.add_flags |= (1U << (ep_id + 1));
}
