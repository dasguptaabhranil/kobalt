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

#ifndef XHCI_H
#define XHCI_H

#include <stdint.h>
#include <stddef.h>
#include "../../../inc/spinlock.h"

#define XHCI_PCI_CLASS     0x0C
#define XHCI_PCI_SUBCLASS  0x03
#define XHCI_PCI_PROGIF    0x30

#define XHCI_CAP_CAPLENGTH   0x00
#define XHCI_CAP_HCIVERSION  0x02
#define XHCI_CAP_HCSPARAMS1  0x04
#define XHCI_CAP_HCSPARAMS2  0x08
#define XHCI_CAP_HCSPARAMS3  0x0C
#define XHCI_CAP_HCCPARAMS1  0x10
#define XHCI_CAP_DBOFF       0x14
#define XHCI_CAP_RTSOFF      0x18
#define XHCI_CAP_HCCPARAMS2  0x1C

#define XHCI_HCSP1_MAXSLOTS(v)  ((v) & 0xFF)
#define XHCI_HCSP1_MAXINTRS(v)  (((v) >> 8) & 0x7FF)
#define XHCI_HCSP1_MAXPORTS(v)  (((v) >> 24) & 0xFF)
#define XHCI_HCCP1_AC64(v)      ((v) & 1)
#define XHCI_HCCP1_CSZ(v)       (((v) >> 2) & 1)
#define XHCI_HCCP1_XECP(v)      (((v) >> 16) & 0xFFFF)

#define XHCI_OP_USBCMD   0x00
#define XHCI_OP_USBSTS   0x04
#define XHCI_OP_PAGESIZE 0x08
#define XHCI_OP_DNCTRL   0x14
#define XHCI_OP_CRCR     0x18
#define XHCI_OP_DCBAAP   0x30
#define XHCI_OP_CONFIG   0x38
#define XHCI_OP_PORTSC(n)    (0x400 + (n) * 0x10)
#define XHCI_OP_PORTPMSC(n)  (0x404 + (n) * 0x10)
#define XHCI_OP_PORTLI(n)    (0x408 + (n) * 0x10)

#define XHCI_USBCMD_RS     (1U << 0)
#define XHCI_USBCMD_HCRST  (1U << 1)
#define XHCI_USBCMD_INTE   (1U << 2)
#define XHCI_USBCMD_HSEE   (1U << 3)

#define XHCI_USBSTS_HCH  (1U << 0)
#define XHCI_USBSTS_HSE  (1U << 2)
#define XHCI_USBSTS_EINT (1U << 3)
#define XHCI_USBSTS_PCD  (1U << 4)
#define XHCI_USBSTS_CNR  (1U << 11)
#define XHCI_USBSTS_HCE  (1U << 12)

#define XHCI_PORTSC_CCS        (1U << 0)
#define XHCI_PORTSC_PED        (1U << 1)
#define XHCI_PORTSC_OCA        (1U << 3)
#define XHCI_PORTSC_PR         (1U << 4)
#define XHCI_PORTSC_PLS_MASK   (0xFU << 5)
#define XHCI_PORTSC_PP         (1U << 9)
#define XHCI_PORTSC_SPEED_MASK (0xFU << 10)
#define XHCI_PORTSC_LWS        (1U << 16)
#define XHCI_PORTSC_CSC        (1U << 17)
#define XHCI_PORTSC_PEC        (1U << 18)
#define XHCI_PORTSC_WRC        (1U << 19)
#define XHCI_PORTSC_OCC        (1U << 20)
#define XHCI_PORTSC_PRC        (1U << 21)
#define XHCI_PORTSC_PLC        (1U << 22)
#define XHCI_PORTSC_CEC        (1U << 23)
#define XHCI_PORTSC_WPR        (1U << 31)

#define XHCI_PORT_SPEED(sc)  (((sc) >> 10) & 0xF)
#define XHCI_SPEED_FS  1
#define XHCI_SPEED_LS  2
#define XHCI_SPEED_HS  3
#define XHCI_SPEED_SS  4
#define XHCI_SPEED_SSP 5

#define XHCI_CRCR_RCS      (1ULL << 0)
#define XHCI_CRCR_PTR_MASK (~0xFULL)

#define XHCI_CONFIG_MAXSLOTSEN_MASK 0xFF

#define XHCI_RT_MFINDEX  0x00
#define XHCI_RT_IR_BASE  0x20
#define XHCI_RT_IR_SIZE  0x20
#define XHCI_RT_IMAN(n)   (XHCI_RT_IR_BASE + (n)*XHCI_RT_IR_SIZE + 0x00)
#define XHCI_RT_IMOD(n)   (XHCI_RT_IR_BASE + (n)*XHCI_RT_IR_SIZE + 0x04)
#define XHCI_RT_ERSTSZ(n) (XHCI_RT_IR_BASE + (n)*XHCI_RT_IR_SIZE + 0x08)
#define XHCI_RT_ERSTBA(n) (XHCI_RT_IR_BASE + (n)*XHCI_RT_IR_SIZE + 0x10)
#define XHCI_RT_ERDP(n)   (XHCI_RT_IR_BASE + (n)*XHCI_RT_IR_SIZE + 0x18)

#define XHCI_IMAN_IP  (1U << 0)
#define XHCI_IMAN_IE  (1U << 1)

#define XHCI_ERDP_EHB      (1ULL << 3)
#define XHCI_ERDP_PTR_MASK (~0xFULL)

typedef struct __attribute__((packed)) {
    uint32_t param_lo;
    uint32_t param_hi;
    uint32_t status;
    uint32_t control;
} xhci_trb_t;

#define TRB_CYCLE       (1U << 0)
#define TRB_ENT         (1U << 1)
#define TRB_ISP         (1U << 2)
#define TRB_NS          (1U << 3)
#define TRB_CH          (1U << 4)
#define TRB_IOC         (1U << 5)
#define TRB_IDT         (1U << 6)
#define TRB_TYPE(t)     ((uint32_t)(t) << 10)
#define TRB_TYPE_GET(c) (((c) >> 10) & 0x3F)
#define TRB_SLOT(s)     ((uint32_t)(s) << 24)
#define TRB_EP(e)       ((uint32_t)(e) << 16)

#define TRB_TYPE_NORMAL       1
#define TRB_TYPE_SETUP        2
#define TRB_TYPE_DATA         3
#define TRB_TYPE_STATUS       4
#define TRB_TYPE_ISOCH        5
#define TRB_TYPE_LINK         6
#define TRB_TYPE_EVENT_DATA   7
#define TRB_TYPE_NOOP_XFER    8
#define TRB_TYPE_ENABLE_SLOT  9
#define TRB_TYPE_DISABLE_SLOT 10
#define TRB_TYPE_ADDRESS_DEV  11
#define TRB_TYPE_CONFIG_EP    12
#define TRB_TYPE_EVAL_CTX     13
#define TRB_TYPE_RESET_EP     14
#define TRB_TYPE_STOP_EP      15
#define TRB_TYPE_SET_TR_DEQ   16
#define TRB_TYPE_RESET_DEV    17
#define TRB_TYPE_NOOP_CMD     23
#define TRB_TYPE_XFER_EVENT   32
#define TRB_TYPE_CMD_CMPL     33
#define TRB_TYPE_PSC_EVENT    34

#define TRB_SETUP_TRT_NO_DATA 0
#define TRB_SETUP_TRT_OUT     2
#define TRB_SETUP_TRT_IN      3
#define TRB_LINK_TC           (1U << 1)

#define TRB_CMPL_SUCCESS      1
#define TRB_CMPL_SHORT_PACKET 13
#define TRB_CMPL_CODE(s)  (((s) >> 24) & 0xFF)
#define TRB_XFER_LEN(s)   ((s) & 0x1FFFF)
#define TRB_EVT_SLOT(c)   (((c) >> 24) & 0xFF)
#define TRB_EVT_EP(c)     (((c) >> 16) & 0x1F)

typedef struct __attribute__((packed)) {
    uint64_t base;
    uint16_t seg_size;
    uint16_t _rsvd[3];
} xhci_erst_entry_t;

typedef struct __attribute__((packed)) {
    uint32_t dw0;
    uint32_t dw1;
    uint32_t dw2;
    uint32_t dw3;
    uint32_t _rsvd[4];
} xhci_slot_ctx_t;

#define SLOT_CTX_DEV_ADDR(dw3)  ((dw3) & 0xFF)
#define SLOT_CTX_STATE(dw3)     (((dw3) >> 27) & 0x1F)

typedef struct __attribute__((packed)) {
    uint32_t dw0;
    uint32_t dw1;
    uint32_t tr_deq_lo;
    uint32_t tr_deq_hi;
    uint32_t dw4;
    uint32_t _rsvd[3];
} xhci_ep_ctx_t;

#define EP_CTX_DCS  (1U << 0)

#define EP_TYPE_INVALID   0
#define EP_TYPE_ISOCH_OUT 1
#define EP_TYPE_BULK_OUT  2
#define EP_TYPE_INTR_OUT  3
#define EP_TYPE_CTRL      4
#define EP_TYPE_ISOCH_IN  5
#define EP_TYPE_BULK_IN   6
#define EP_TYPE_INTR_IN   7

typedef struct __attribute__((packed)) {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t _rsvd[6];
} xhci_input_ctrl_ctx_t;

#define XHCI_MAX_EP   32

typedef struct __attribute__((aligned(64))) {
    xhci_slot_ctx_t slot;
    xhci_ep_ctx_t   ep[XHCI_MAX_EP - 1];
} xhci_dev_ctx_t;

typedef struct __attribute__((aligned(64))) {
    xhci_input_ctrl_ctx_t ctrl;
    xhci_slot_ctx_t       slot;
    xhci_ep_ctx_t         ep[XHCI_MAX_EP - 1];
} xhci_input_ctx_t;

#define XHCI_RING_SIZE        256
#define XHCI_ERST_SEGMENTS    1
#define XHCI_EVENT_RING_SIZE  256
#define XHCI_MAX_CONTROLLERS  4
#define XHCI_MAX_SLOTS        255
#define XHCI_MAX_PORTS        32

typedef struct {
    xhci_trb_t *trbs;
    uint32_t    enq;
    uint32_t    deq;
    uint8_t     cycle;
    uint8_t     _pad[3];
} xhci_ring_t;

struct xhci_ctrl;
typedef struct xhci_ctrl xhci_ctrl_t;

int          xhci_init(void);
int          xhci_ctrl_reset(xhci_ctrl_t *xc);
int          xhci_enable_slot(xhci_ctrl_t *xc, uint8_t *slot_out);
int          xhci_disable_slot(xhci_ctrl_t *xc, uint8_t slot);
int          xhci_address_device(xhci_ctrl_t *xc, uint8_t slot,
                                  xhci_input_ctx_t *ic, int bsr);
int          xhci_configure_ep(xhci_ctrl_t *xc, uint8_t slot,
                                xhci_input_ctx_t *ic);
int          xhci_evaluate_ctx(xhci_ctrl_t *xc, uint8_t slot,
                                xhci_input_ctx_t *ic);
int          xhci_reset_ep(xhci_ctrl_t *xc, uint8_t slot, uint8_t ep_id);
int          xhci_stop_ep(xhci_ctrl_t *xc, uint8_t slot, uint8_t ep_id);
int          xhci_ctrl_transfer(xhci_ctrl_t *xc, uint8_t slot,
                                 const uint8_t *setup, void *buf,
                                 uint16_t len, int dir_in);
int          xhci_bulk_transfer(xhci_ctrl_t *xc, uint8_t slot, uint8_t ep_id,
                                 void *buf, uint32_t len);
int          xhci_intr_transfer(xhci_ctrl_t *xc, uint8_t slot, uint8_t ep_id,
                                 void *buf, uint32_t len);
void         xhci_poll(xhci_ctrl_t *xc);
xhci_ctrl_t *xhci_get_ctrl(int idx);
int          xhci_ctrl_count(void);

struct xhci_ctrl {
    uintptr_t  mmio;
    uintptr_t  op_base;
    uintptr_t  rt_base;
    uintptr_t  db_base;
    uint8_t    max_slots;
    uint8_t    max_ports;
    uint16_t   max_intrs;
    uint8_t    ctx_size;
    uint64_t  *dcbaa;
    xhci_dev_ctx_t *dev_ctx[XHCI_MAX_SLOTS + 1];
    xhci_ring_t     cmd_ring;
    xhci_ring_t     evt_ring;
    xhci_erst_entry_t *erst;
    xhci_ring_t    *xfer_rings[XHCI_MAX_SLOTS + 1][XHCI_MAX_EP];
    xhci_trb_t      last_cmd_cmpl;
    volatile int    cmd_pending;
    int             idx;
    spinlock_t      lock;
};

static inline uint8_t xhci_ep_index(uint8_t ep_addr)
{
    uint8_t n   = ep_addr & 0x7F;
    uint8_t dir = (ep_addr & 0x80) ? 1 : 0;
    return (uint8_t)(n == 0 ? 1 : (2 * n + dir));
}

#endif
