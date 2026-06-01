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
#include "../inc/usb_core.h"
#include "../inc/kernel.h"
#include "../inc/pci.h"
#include "../inc/kmalloc.h"
#include "../../../inc/spinlock.h"

static xhci_ctrl_t *g_ctrls[XHCI_MAX_CONTROLLERS];
static int g_nctrl;

extern int  xhci_ring_init(xhci_ring_t *r);
extern void xhci_ring_free(xhci_ring_t *r);
extern int  xhci_ring_enqueue(xhci_ring_t *r, const xhci_trb_t *trb, int unused);
extern int  xhci_evt_ring_init(xhci_ctrl_t *xc);
extern int  xhci_mem_init(xhci_ctrl_t *xc);
extern void xhci_mem_free(xhci_ctrl_t *xc);
extern int  xhci_alloc_slot_ctx(xhci_ctrl_t *xc, uint8_t slot);
extern int  xhci_alloc_xfer_ring(xhci_ctrl_t *xc, uint8_t slot, uint8_t ep_id);
extern void xhci_free_slot_ctx(xhci_ctrl_t *xc, uint8_t slot);

static inline uint32_t cap32(const xhci_ctrl_t *xc, uint32_t off)
    { return *(volatile uint32_t *)(xc->mmio + off); }
static inline uint8_t  cap8 (const xhci_ctrl_t *xc, uint32_t off)
    { return *(volatile uint8_t  *)(xc->mmio + off); }
static inline uint32_t op32 (const xhci_ctrl_t *xc, uint32_t off)
    { return *(volatile uint32_t *)(xc->op_base + off); }
static inline void wop32(xhci_ctrl_t *xc, uint32_t off, uint32_t v)
    { *(volatile uint32_t *)(xc->op_base + off) = v; }
static inline void wop64(xhci_ctrl_t *xc, uint32_t off, uint64_t v) {
    *(volatile uint32_t *)(xc->op_base + off)     = (uint32_t)v;
    *(volatile uint32_t *)(xc->op_base + off + 4) = (uint32_t)(v >> 32);
}
static inline uint32_t rt32(const xhci_ctrl_t *xc, uint32_t off)
    { return *(volatile uint32_t *)(xc->rt_base + off); }
static inline void wrt32(xhci_ctrl_t *xc, uint32_t off, uint32_t v)
    { *(volatile uint32_t *)(xc->rt_base + off) = v; }
static inline void wrt64(xhci_ctrl_t *xc, uint32_t off, uint64_t v) {
    *(volatile uint32_t *)(xc->rt_base + off)     = (uint32_t)v;
    *(volatile uint32_t *)(xc->rt_base + off + 4) = (uint32_t)(v >> 32);
}
static inline void wdb(xhci_ctrl_t *xc, uint8_t slot, uint32_t v)
    { *(volatile uint32_t *)(xc->db_base + (uint32_t)slot * 4) = v; }

static void udelay(uint32_t us)
{
    volatile uint32_t n = us * 400u;
    while (n--) __asm__ volatile("pause");
}
static void mdelay(uint32_t ms) { while (ms--) udelay(1000); }

int xhci_ctrl_reset(xhci_ctrl_t *xc)
{
    wop32(xc, XHCI_OP_USBCMD, op32(xc, XHCI_OP_USBCMD) & ~XHCI_USBCMD_RS);
    int t = 100;
    while (!(op32(xc, XHCI_OP_USBSTS) & XHCI_USBSTS_HCH)) {
        mdelay(1); if (!--t) { klog_fail("xhci", "HC did not halt"); return -1; }
    }
    wop32(xc, XHCI_OP_USBCMD, op32(xc, XHCI_OP_USBCMD) | XHCI_USBCMD_HCRST);
    t = 500;
    while (op32(xc, XHCI_OP_USBCMD) & XHCI_USBCMD_HCRST) {
        mdelay(1); if (!--t) { klog_fail("xhci", "HC reset timed out"); return -1; }
    }
    t = 100;
    while (op32(xc, XHCI_OP_USBSTS) & XHCI_USBSTS_CNR) {
        mdelay(1); if (!--t) { klog_fail("xhci", "CNR stuck"); return -1; }
    }
    return 0;
}

static int ctrl_start(xhci_ctrl_t *xc)
{
    wop32(xc, XHCI_OP_CONFIG,
          (op32(xc, XHCI_OP_CONFIG) & ~XHCI_CONFIG_MAXSLOTSEN_MASK) | xc->max_slots);
    wop64(xc, XHCI_OP_DCBAAP, (uint64_t)(uintptr_t)xc->dcbaa);
    wop64(xc, XHCI_OP_CRCR,
          ((uint64_t)(uintptr_t)xc->cmd_ring.trbs & XHCI_CRCR_PTR_MASK) | XHCI_CRCR_RCS);

    wrt32(xc, XHCI_RT_ERSTSZ(0), XHCI_ERST_SEGMENTS);
    wrt64(xc, XHCI_RT_ERSTBA(0), (uint64_t)(uintptr_t)xc->erst);
    wrt64(xc, XHCI_RT_ERDP(0),   (uint64_t)(uintptr_t)xc->evt_ring.trbs);
    wrt32(xc, XHCI_RT_IMOD(0), 0);
    wrt32(xc, XHCI_RT_IMAN(0), rt32(xc, XHCI_RT_IMAN(0)) | XHCI_IMAN_IE);

    wop32(xc, XHCI_OP_USBCMD,
          op32(xc, XHCI_OP_USBCMD) | XHCI_USBCMD_RS | XHCI_USBCMD_INTE);
    int t = 50;
    while (op32(xc, XHCI_OP_USBSTS) & XHCI_USBSTS_HCH) {
        mdelay(1); if (!--t) { klog_fail("xhci", "HC did not start"); return -1; }
    }
    return 0;
}

void xhci_poll(xhci_ctrl_t *xc)
{
    xhci_ring_t *er = &xc->evt_ring;
    for (;;) {
        xhci_trb_t *trb = &er->trbs[er->deq];
        if ((trb->control & TRB_CYCLE) != er->cycle) break;

        uint8_t type = (uint8_t)TRB_TYPE_GET(trb->control);
        if (type == TRB_TYPE_CMD_CMPL || type == TRB_TYPE_XFER_EVENT) {
            xc->last_cmd_cmpl = *trb;
            xc->cmd_pending   = 0;
        } else if (type == TRB_TYPE_PSC_EVENT) {
            uint8_t port = (uint8_t)(trb->param_lo >> 24);
            usb_port_changed(xc, port, op32(xc, XHCI_OP_PORTSC(port - 1)));
        }

        if (++er->deq >= XHCI_EVENT_RING_SIZE) { er->deq = 0; er->cycle ^= 1; }

        uint64_t erdp = (uint64_t)(uintptr_t)&er->trbs[er->deq] | XHCI_ERDP_EHB;
        wrt64(xc, XHCI_RT_ERDP(0), erdp);

        uint32_t sts = op32(xc, XHCI_OP_USBSTS);
        if (sts & XHCI_USBSTS_EINT) wop32(xc, XHCI_OP_USBSTS, XHCI_USBSTS_EINT);
    }
}

static int submit_cmd(xhci_ctrl_t *xc, const xhci_trb_t *cmd, uint8_t *code_out)
{
    spin_lock(&xc->lock);
    uint64_t pa = (uint64_t)(uintptr_t)&xc->cmd_ring.trbs[xc->cmd_ring.enq];
    xhci_ring_enqueue(&xc->cmd_ring, cmd, 0);
    wdb(xc, 0, 0);

    int found = 0, t = 5000;
    while (t-- > 0) {
        xhci_poll(xc);
        if (!xc->cmd_pending) {
            uint64_t ccpa = ((uint64_t)xc->last_cmd_cmpl.param_lo |
                             ((uint64_t)xc->last_cmd_cmpl.param_hi << 32)) & ~0xFULL;
            if (ccpa == pa) { found = 1; break; }
        }
        udelay(1000);
    }
    spin_unlock(&xc->lock);

    if (!found) { klog_fail("xhci", "command timeout"); return -1; }
    uint8_t code = TRB_CMPL_CODE(xc->last_cmd_cmpl.status);
    if (code_out) *code_out = code;
    return code == TRB_CMPL_SUCCESS ? 0 : -1;
}

int xhci_enable_slot(xhci_ctrl_t *xc, uint8_t *out)
{
    xhci_trb_t cmd = {0};
    cmd.control = TRB_TYPE(TRB_TYPE_ENABLE_SLOT);
    if (submit_cmd(xc, &cmd, NULL) < 0) return -1;
    uint8_t slot = TRB_EVT_SLOT(xc->last_cmd_cmpl.control);
    if (!slot || slot > xc->max_slots) return -1;
    if (xhci_alloc_slot_ctx(xc, slot)    < 0) return -1;
    if (xhci_alloc_xfer_ring(xc, slot, 1) < 0) return -1;
    if (out) *out = slot;
    return 0;
}

int xhci_disable_slot(xhci_ctrl_t *xc, uint8_t slot)
{
    xhci_trb_t cmd = {0};
    cmd.control = TRB_TYPE(TRB_TYPE_DISABLE_SLOT) | TRB_SLOT(slot);
    int r = submit_cmd(xc, &cmd, NULL);
    xhci_free_slot_ctx(xc, slot);
    return r;
}

int xhci_address_device(xhci_ctrl_t *xc, uint8_t slot, xhci_input_ctx_t *ic, int bsr)
{
    xhci_trb_t cmd = {0};
    cmd.param_lo = (uint32_t)(uintptr_t)ic;
    cmd.param_hi = (uint32_t)((uintptr_t)ic >> 32);
    cmd.control  = TRB_TYPE(TRB_TYPE_ADDRESS_DEV) | TRB_SLOT(slot) | (bsr ? (1U<<9) : 0);
    return submit_cmd(xc, &cmd, NULL);
}

int xhci_configure_ep(xhci_ctrl_t *xc, uint8_t slot, xhci_input_ctx_t *ic)
{
    xhci_trb_t cmd = {0};
    cmd.param_lo = (uint32_t)(uintptr_t)ic;
    cmd.param_hi = (uint32_t)((uintptr_t)ic >> 32);
    cmd.control  = TRB_TYPE(TRB_TYPE_CONFIG_EP) | TRB_SLOT(slot);
    return submit_cmd(xc, &cmd, NULL);
}

int xhci_evaluate_ctx(xhci_ctrl_t *xc, uint8_t slot, xhci_input_ctx_t *ic)
{
    xhci_trb_t cmd = {0};
    cmd.param_lo = (uint32_t)(uintptr_t)ic;
    cmd.param_hi = (uint32_t)((uintptr_t)ic >> 32);
    cmd.control  = TRB_TYPE(TRB_TYPE_EVAL_CTX) | TRB_SLOT(slot);
    return submit_cmd(xc, &cmd, NULL);
}

int xhci_reset_ep(xhci_ctrl_t *xc, uint8_t slot, uint8_t ep_id)
{
    xhci_trb_t cmd = {0};
    cmd.control = TRB_TYPE(TRB_TYPE_RESET_EP) | TRB_SLOT(slot) | TRB_EP(ep_id);
    return submit_cmd(xc, &cmd, NULL);
}

int xhci_stop_ep(xhci_ctrl_t *xc, uint8_t slot, uint8_t ep_id)
{
    xhci_trb_t cmd = {0};
    cmd.control = TRB_TYPE(TRB_TYPE_STOP_EP) | TRB_SLOT(slot) | TRB_EP(ep_id);
    return submit_cmd(xc, &cmd, NULL);
}

static int wait_xfer(xhci_ctrl_t *xc, uint8_t slot, uint8_t ep_id)
{
    for (int t = 5000; t-- > 0; ) {
        xhci_poll(xc);
        if (!xc->cmd_pending) {
            uint8_t es = TRB_EVT_SLOT(xc->last_cmd_cmpl.control);
            uint8_t ee = TRB_EVT_EP(xc->last_cmd_cmpl.control);
            uint8_t et = (uint8_t)TRB_TYPE_GET(xc->last_cmd_cmpl.control);
            if (et == TRB_TYPE_XFER_EVENT && es == slot && ee == ep_id) {
                uint8_t code = TRB_CMPL_CODE(xc->last_cmd_cmpl.status);
                xc->cmd_pending = 1;
                return (code == TRB_CMPL_SUCCESS || code == TRB_CMPL_SHORT_PACKET) ? 0 : -1;
            }
        }
        udelay(1000);
    }
    klog_fail("xhci", "transfer timeout");
    return -1;
}

int xhci_ctrl_transfer(xhci_ctrl_t *xc, uint8_t slot,
                        const uint8_t *setup, void *buf, uint16_t len, int dir_in)
{
    xhci_ring_t *ring = xc->xfer_rings[slot][1];
    if (!ring) return -1;

    xhci_trb_t trb = {0};
    memcpy(&trb.param_lo, setup, 4);
    memcpy(&trb.param_hi, setup + 4, 4);
    trb.status  = 8;
    trb.control = TRB_TYPE(TRB_TYPE_SETUP) | TRB_IDT |
                  (uint32_t)((len ? (dir_in ? TRB_SETUP_TRT_IN : TRB_SETUP_TRT_OUT)
                                  : TRB_SETUP_TRT_NO_DATA) << 16);
    xhci_ring_enqueue(ring, &trb, 0);

    if (len && buf) {
        xhci_trb_t dt = {0};
        dt.param_lo = (uint32_t)(uintptr_t)buf;
        dt.param_hi = (uint32_t)((uintptr_t)buf >> 32);
        dt.status   = len;
        dt.control  = TRB_TYPE(TRB_TYPE_DATA) | TRB_IOC | (dir_in ? (1U<<16) : 0);
        xhci_ring_enqueue(ring, &dt, 0);
    }

    xhci_trb_t st = {0};
    st.control = TRB_TYPE(TRB_TYPE_STATUS) | TRB_IOC |
                 ((!len || !dir_in) ? (1U<<16) : 0);
    xhci_ring_enqueue(ring, &st, 0);

    xc->cmd_pending = 1;
    wdb(xc, slot, 1);
    return wait_xfer(xc, slot, 1);
}

int xhci_bulk_transfer(xhci_ctrl_t *xc, uint8_t slot,
                        uint8_t ep_id, void *buf, uint32_t len)
{
    xhci_ring_t *ring = xc->xfer_rings[slot][ep_id];
    if (!ring) return -1;

    uint8_t  *p   = buf;
    uint32_t  rem = len;
    while (rem) {
        uint32_t n = rem > 65536u ? 65536u : rem;
        xhci_trb_t trb = {0};
        trb.param_lo = (uint32_t)(uintptr_t)p;
        trb.param_hi = (uint32_t)((uintptr_t)p >> 32);
        trb.status   = n;
        trb.control  = TRB_TYPE(TRB_TYPE_NORMAL) | TRB_IOC;
        xhci_ring_enqueue(ring, &trb, 0);
        p += n; rem -= n;
    }
    xc->cmd_pending = 1;
    wdb(xc, slot, ep_id);
    return wait_xfer(xc, slot, ep_id);
}

int xhci_intr_transfer(xhci_ctrl_t *xc, uint8_t slot,
                        uint8_t ep_id, void *buf, uint32_t len)
{
    return xhci_bulk_transfer(xc, slot, ep_id, buf, len);
}

static void enum_port(xhci_ctrl_t *xc, uint8_t port)
{
    uint32_t ps = op32(xc, XHCI_OP_PORTSC(port - 1));
    if (!(ps & XHCI_PORTSC_CCS)) return;

    uint32_t clr = ps & (XHCI_PORTSC_CSC | XHCI_PORTSC_PEC |
                          XHCI_PORTSC_PRC | XHCI_PORTSC_PLC |
                          XHCI_PORTSC_CEC | XHCI_PORTSC_WRC);
    wop32(xc, XHCI_OP_PORTSC(port - 1), clr);

    if (!(ps & XHCI_PORTSC_PED)) {
        uint32_t rst = (ps & ~(XHCI_PORTSC_CSC | XHCI_PORTSC_PEC |
                                XHCI_PORTSC_PRC | XHCI_PORTSC_PLC |
                                XHCI_PORTSC_CEC | XHCI_PORTSC_WRC)) | XHCI_PORTSC_PR;
        wop32(xc, XHCI_OP_PORTSC(port - 1), rst);
        for (int t = 200; t-- > 0; ) {
            mdelay(1);
            ps = op32(xc, XHCI_OP_PORTSC(port - 1));
            if (ps & XHCI_PORTSC_PRC) break;
        }
        wop32(xc, XHCI_OP_PORTSC(port - 1), ps | XHCI_PORTSC_PRC);
        mdelay(10);
        ps = op32(xc, XHCI_OP_PORTSC(port - 1));
    }

    if (!(ps & XHCI_PORTSC_PED)) return;

    char msg[48];
    ksnprintf(msg, sizeof(msg), "port %u: connected spd=%u",
              port, (unsigned)XHCI_PORT_SPEED(ps));
    klog_info("xhci", msg);
    usb_port_changed(xc, port, ps);
}

static int probe_pci(pci_device_t *pdev)
{
    if (g_nctrl >= XHCI_MAX_CONTROLLERS) { klog_warn("xhci", "too many controllers"); return -1; }

    xhci_ctrl_t *xc = kmalloc(sizeof(*xc));
    if (!xc) return -1;
    memset(xc, 0, sizeof(*xc));
    xc->lock = SPINLOCK_INIT;

    pci_write_config16(pdev, 0x04, pci_read_config16(pdev, 0x04) | (1U<<1) | (1U<<2));

    uint32_t bl = pci_read_config32(pdev, 0x10) & ~0xFU;
    uint32_t bh = pci_read_config32(pdev, 0x14);
    xc->mmio = (uintptr_t)(((uint64_t)bh << 32) | bl);

    uint8_t clen = cap8(xc, XHCI_CAP_CAPLENGTH);
    xc->op_base = xc->mmio + clen;

    uint32_t sp1 = cap32(xc, XHCI_CAP_HCSPARAMS1);
    uint32_t cp1 = cap32(xc, XHCI_CAP_HCCPARAMS1);
    xc->max_slots = (uint8_t)XHCI_HCSP1_MAXSLOTS(sp1);
    xc->max_ports = (uint8_t)XHCI_HCSP1_MAXPORTS(sp1);
    xc->max_intrs = (uint16_t)XHCI_HCSP1_MAXINTRS(sp1);
    xc->ctx_size  = (uint8_t)XHCI_HCCP1_CSZ(cp1);
    xc->db_base   = xc->mmio + (cap32(xc, XHCI_CAP_DBOFF)  & ~3U);
    xc->rt_base   = xc->mmio + (cap32(xc, XHCI_CAP_RTSOFF) & ~0x1FU);

    if (xhci_ctrl_reset(xc)    < 0) goto fail;
    if (xhci_mem_init(xc)      < 0) goto fail;
    if (xhci_evt_ring_init(xc) < 0) goto fail;
    if (xhci_ring_init(&xc->cmd_ring) < 0) goto fail;
    if (ctrl_start(xc)         < 0) { xhci_mem_free(xc); goto fail; }

    xc->idx = g_nctrl;
    g_ctrls[g_nctrl++] = xc;

    {
        uint32_t ver = cap32(xc, XHCI_CAP_HCIVERSION);
        char msg[64];
        ksnprintf(msg, sizeof(msg), "xHCI v%x.%02x: %u slots %u ports %s ctx",
                  (ver >> 8) & 0xFF, ver & 0xFF,
                  xc->max_slots, xc->max_ports,
                  xc->ctx_size ? "64B" : "32B");
        klog_ok("xhci", msg);
    }
    for (uint8_t p = 1; p <= xc->max_ports; p++) enum_port(xc, p);
    return 0;

fail:
    kfree(xc);
    return -1;
}

int xhci_init(void)
{
    int found = 0;
    uint32_t n = pci_count();
    for (uint32_t i = 0; i < n; i++) {
        pci_device_t *d = pci_get_device(i);
        if (!d) continue;
        if (d->class_code != XHCI_PCI_CLASS   ) continue;
        if (d->subclass   != XHCI_PCI_SUBCLASS) continue;
        if (d->prog_if    != XHCI_PCI_PROGIF  ) continue;
        if (probe_pci(d) == 0) found++;
    }
    if (!found) { klog_info("xhci", "no xHCI controller found"); return -1; }
    return found;
}

xhci_ctrl_t *xhci_get_ctrl(int idx)
    { return (idx >= 0 && idx < g_nctrl) ? g_ctrls[idx] : NULL; }

int xhci_ctrl_count(void) { return g_nctrl; }
