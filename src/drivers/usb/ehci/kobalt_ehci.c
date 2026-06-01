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

#include "kobalt_ehci.h"
#include "../../../inc/kmalloc.h"
#include "../../../inc/kernel.h"
#include "../../../inc/pci_msi.h"
#include <string.h>

#define OPREG(hc, off)    ((hc)->op[(off)/4])
#define CAPB(hc, off)     (*(volatile uint8_t *)((hc)->cap_base + (off)))
#define CAPW(hc, off)     (*(volatile uint16_t *)((hc)->cap_base + (off)))
#define CAPD(hc, off)     (*(volatile uint32_t *)((hc)->cap_base + (off)))

#define PORTSC(hc, n)     OPREG(hc, 0x44 + 4*(n))

#define EHCI_NULL         1u

#define QTD_STATUS_ACTIVE  (1u << 7)
#define QTD_STATUS_HALTED  (1u << 6)
#define QTD_STATUS_CERR(n) ((n) << 10)
#define QTD_PID_OUT        (0u << 8)
#define QTD_PID_IN         (1u << 8)
#define QTD_PID_SETUP      (2u << 8)
#define QTD_IOC            (1u << 15)
#define QTD_DT             (1u << 31)
#define QTD_BYTES(n)       ((n) << 16)
#define QTD_CPAGE(n)       ((n) << 12)

#define QH_H               (1u << 15)
#define QH_EPS_HS          (2u << 12)
#define QH_DTC             (1u << 14)
#define QH_RL(n)           ((n) << 28)
#define QH_MPS(n)          ((n) << 16)

#define ALIGN32(p)  ((void *)(((uintptr_t)(p) + 31) & ~31UL))
#define ALIGN4K(p)  ((void *)(((uintptr_t)(p) + 4095) & ~4095UL))

static ehci_qtd_t *alloc_qtd(void)
{
    void *raw = kmalloc(sizeof(ehci_qtd_t) + 32);
    if (!raw) return NULL;
    ehci_qtd_t *q = ALIGN32(raw);
    memset(q, 0, sizeof(*q));
    q->qtd_next    = EHCI_NULL;
    q->qtd_altnext = EHCI_NULL;
    return q;
}

static ehci_qh_t *alloc_qh(void)
{
    void *raw = kmalloc(sizeof(ehci_qh_t) + 32);
    if (!raw) return NULL;
    ehci_qh_t *q = ALIGN32(raw);
    memset(q, 0, sizeof(*q));
    return q;
}

static void fill_qtd(ehci_qtd_t *q, uint32_t pid, void *buf,
                     uint16_t len, int dt, int ioc)
{
    q->qtd_status  = QTD_STATUS_ACTIVE | QTD_STATUS_CERR(3) |
                     pid | QTD_BYTES(len) | QTD_CPAGE(0) |
                     (dt ? QTD_DT : 0) | (ioc ? QTD_IOC : 0);
    if (buf) {
        uintptr_t pa = (uintptr_t)buf;
        q->qtd_buffer[0]    = (uint32_t)(pa & 0xFFFFF000u);
        q->qtd_buffer_hi[0] = (uint32_t)(pa >> 32);

        q->qtd_buffer[0] |= pa & 0xFFFu;
        for (int i = 1; i < 5 && len > 0; i++) {
            pa += 0x1000;
            q->qtd_buffer[i]    = (uint32_t)(pa & 0xFFFFF000u);
            q->qtd_buffer_hi[i] = (uint32_t)(pa >> 32);
        }
    }
}

static void async_insert(kobalt_ehci_t *hc, ehci_qh_t *qh)
{
    ehci_qh_t *head = hc->qh_head;
    uint32_t head_pa = (uint32_t)(uintptr_t)head;
    uint32_t qh_pa   = (uint32_t)(uintptr_t)qh;

    qh->qh_link   = head->qh_link;
    head->qh_link = qh_pa | EHCI_LINK_QH;
    (void)head_pa;
}

static void async_remove(kobalt_ehci_t *hc, ehci_qh_t *qh)
{
    ehci_qh_t *head = hc->qh_head;
    uint32_t qh_pa = (uint32_t)(uintptr_t)qh;

    if ((head->qh_link & ~0x1Fu) == qh_pa)
        head->qh_link = qh->qh_link;
}

static int wait_qtd(ehci_qtd_t *q, int ms)
{
    for (int i = 0; i < ms * 100; i++) {
        for (volatile int x = 0; x < 10000; x++);
        if (!(q->qtd_status & QTD_STATUS_ACTIVE)) {
            if (q->qtd_status & QTD_STATUS_HALTED) return -1;
            return 0;
        }
    }
    return -1;
}

int kobalt_ehci_control(kobalt_ehci_t *hc, uint8_t addr, uint8_t ep,
                        uint16_t mps, ehci_setup_t *setup,
                        void *data, uint16_t len, int dir_in)
{
    ehci_qtd_t *tsetup = alloc_qtd();
    ehci_qtd_t *tdata  = len ? alloc_qtd() : NULL;
    ehci_qtd_t *tstat  = alloc_qtd();
    ehci_qh_t  *qh     = alloc_qh();
    if (!tsetup || !tstat || !qh) return -1;

    fill_qtd(tsetup, QTD_PID_SETUP, setup, 8, 0, 0);
    if (tdata)
        fill_qtd(tdata, dir_in ? QTD_PID_IN : QTD_PID_OUT, data, len, 1, 0);
    fill_qtd(tstat, dir_in ? QTD_PID_OUT : QTD_PID_IN, NULL, 0, 1, 1);

    if (tdata) {
        tsetup->qtd_next = (uint32_t)(uintptr_t)tdata;
        tdata->qtd_next  = (uint32_t)(uintptr_t)tstat;
    } else {
        tsetup->qtd_next = (uint32_t)(uintptr_t)tstat;
    }

    qh->qh_endp    = addr | ((uint32_t)ep << 8) | QH_EPS_HS | QH_DTC |
                     QH_MPS(mps) | QH_RL(8);
    qh->qh_endphub = (1u << 30);
    qh->qh_curqtd  = 0;
    qh->qh_qtd.qtd_next    = (uint32_t)(uintptr_t)tsetup;
    qh->qh_qtd.qtd_altnext = EHCI_NULL;
    qh->qh_qtd.qtd_status  = 0;

    async_insert(hc, qh);

    int r = wait_qtd(tstat, 500);

    async_remove(hc, qh);
    return r;
}

void kobalt_ehci_irq(kobalt_ehci_t *hc)
{
    uint32_t sts = OPREG(hc, EHCI_USBSTS);
    OPREG(hc, EHCI_USBSTS) = sts;

    if (sts & EHCI_STS_PCD)
        kobalt_ehci_poll(hc);
}

void kobalt_ehci_poll(kobalt_ehci_t *hc)
{
    for (uint8_t n = 0; n < hc->n_ports; n++) {
        uint32_t ps = PORTSC(hc, n);
        if (!(ps & EHCI_PS_CSC)) continue;

        PORTSC(hc, n) = (ps & ~EHCI_PS_CLEAR) | EHCI_PS_CSC;

        if (!(ps & EHCI_PS_CS)) continue;

        if ((ps & EHCI_PS_LS) == EHCI_PS_LS) continue;

        PORTSC(hc, n) = (PORTSC(hc, n) & ~EHCI_PS_PE) | EHCI_PS_PR;
        for (volatile int t = 0; t < 5000000; t++);
        PORTSC(hc, n) &= ~EHCI_PS_PR;
        for (volatile int t = 0; t < 500000; t++);

        if (!(PORTSC(hc, n) & EHCI_PS_PE)) continue;

        klog_ok("ehci", "HS device connected, enumerate here");

    }
}

static void ehci_irq_handler(int v, void *arg)
{
    (void)v;
    kobalt_ehci_irq((kobalt_ehci_t *)arg);
}

int kobalt_ehci_init(pci_device_t *pdev, kobalt_ehci_t *hc)
{
    memset(hc, 0, sizeof(*hc));
    hc->pdev     = pdev;
    hc->cap_base = pci_bar_base(pdev, 0);
    if (!hc->cap_base) return -1;

    pci_enable_device(pdev);

    uint8_t cap_len = CAPB(hc, 0);
    hc->op = (ehci_opreg_t)(hc->cap_base + cap_len);

    uint32_t hcs = CAPD(hc, 4);
    hc->n_ports  = (uint8_t)(hcs & 0xF);
    if (!hc->n_ports) hc->n_ports = 1;

    OPREG(hc, EHCI_USBCMD) &= ~EHCI_CMD_RS;
    for (int i = 0; i < 1000 && !(OPREG(hc, EHCI_USBSTS) & EHCI_STS_HCH); i++)
        for (volatile int x = 0; x < 10000; x++);

    OPREG(hc, EHCI_USBCMD) |= EHCI_CMD_HCRESET;
    for (int i = 0; i < 1000 && (OPREG(hc, EHCI_USBCMD) & EHCI_CMD_HCRESET); i++)
        for (volatile int x = 0; x < 10000; x++);

    OPREG(hc, EHCI_CTRLDSSEGMENT) = 0;

    void *fl_raw = kmalloc(1024*4 + 4096);
    hc->frame_list = ALIGN4K(fl_raw);
    for (int i = 0; i < 1024; i++)
        hc->frame_list[i] = EHCI_NULL;
    OPREG(hc, EHCI_PERIODICLISTBASE) = (uint32_t)(uintptr_t)hc->frame_list;

    hc->qh_head = alloc_qh();
    if (!hc->qh_head) return -1;
    uint32_t qh_pa = (uint32_t)(uintptr_t)hc->qh_head;
    hc->qh_head->qh_link           = qh_pa | EHCI_LINK_QH;
    hc->qh_head->qh_endp           = QH_H;
    hc->qh_head->qh_curqtd         = 0;
    hc->qh_head->qh_qtd.qtd_next   = EHCI_NULL;
    hc->qh_head->qh_qtd.qtd_altnext= EHCI_NULL;
    hc->qh_head->qh_qtd.qtd_status = QTD_STATUS_HALTED;
    OPREG(hc, EHCI_ASYNCLISTADDR)   = qh_pa;

    OPREG(hc, EHCI_USBINTR) = EHCI_INTR_UIE | EHCI_INTR_UEIE |
                               EHCI_INTR_PCIE | EHCI_INTR_HSEE;

    OPREG(hc, EHCI_USBCMD) = EHCI_CMD_RS | EHCI_CMD_ASE | EHCI_CMD_PSE |
                              (8u << 16);

    OPREG(hc, EHCI_CONFIGFLAG) = EHCI_CONF_CF;

    for (uint8_t n = 0; n < hc->n_ports; n++)
        PORTSC(hc, n) |= EHCI_PS_PP;

    hc->vec = pci_enable_msi(pdev, 1);
    if (hc->vec >= 0)
        msi_register_handler(hc->vec, ehci_irq_handler, hc);

    klog_ok("ehci", "controller started");
    return 0;
}
