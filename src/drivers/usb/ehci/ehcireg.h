

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

#ifndef _DEV_PCI_EHCIREG_H_
#define _DEV_PCI_EHCIREG_H_

#include <stdint.h>

#define PCI_CBMEM		0x10

#define PCI_INTERFACE_EHCI	0x20

#define PCI_USBREV		0x60
#define  PCI_USBREV_MASK	0xff
#define  PCI_USBREV_PRE_1_0	0x00
#define  PCI_USBREV_1_0		0x10
#define  PCI_USBREV_1_1		0x11
#define  PCI_USBREV_2_0		0x20

#define PCI_EHCI_FLADJ		0x61

#define PCI_EHCI_PORTWAKECAP	0x62

#define EHCI_EC_LEGSUP		0x01

#define EHCI_EECP_NEXT(x)	(((x) >> 8) & 0xff)
#define EHCI_EECP_ID(x)		((x) & 0xff)

#define EHCI_LEGSUP_LEGSUP	0x00
#define  EHCI_LEGSUP_OSOWNED	0x01000000
#define  EHCI_LEGSUP_BIOSOWNED	0x00010000
#define PCI_LEGSUP_USBLEGCTLSTS	0x04

#define EHCI_CAPLENGTH		0x00

#define EHCI_HCIVERSION		0x02

#define EHCI_HCSPARAMS		0x04
#define  EHCI_HCS_DEBUGPORT(x)	(((x) >> 20) & 0xf)
#define  EHCI_HCS_P_INDICATOR(x) ((x) & 0x10000)
#define  EHCI_HCS_N_CC(x)	(((x) >> 12) & 0xf)
#define  EHCI_HCS_N_PCC(x)	(((x) >> 8) & 0xf)
#define  EHCI_HCS_PRR(x)	((x) & 0x80)
#define  EHCI_HCS_PPC(x)	((x) & 0x10)
#define  EHCI_HCS_N_PORTS(x)	((x) & 0xf)

#define EHCI_HCCPARAMS		0x08
#define  EHCI_HCC_EECP(x)	(((x) >> 8) & 0xff)
#define  EHCI_HCC_IST(x)	(((x) >> 4) & 0xf)
#define  EHCI_HCC_ASPC(x)	((x) & 0x4)
#define  EHCI_HCC_PFLF(x)	((x) & 0x2)
#define  EHCI_HCC_64BIT(x)	((x) & 0x1)

#define EHCI_HCSP_PORTROUTE	0x0c

#define EHCI_USBCMD		0x00
#define  EHCI_CMD_ITC_M		0x00ff0000
#define   EHCI_CMD_ITC_1	0x00010000
#define   EHCI_CMD_ITC_2	0x00020000
#define   EHCI_CMD_ITC_4	0x00040000
#define   EHCI_CMD_ITC_8	0x00080000
#define   EHCI_CMD_ITC_16	0x00100000
#define   EHCI_CMD_ITC_32	0x00200000
#define   EHCI_CMD_ITC_64	0x00400000
#define  EHCI_CMD_ASPME		0x00000800
#define  EHCI_CMD_ASPMC		0x00000300
#define  EHCI_CMD_LHCR		0x00000080
#define  EHCI_CMD_IAAD		0x00000040
#define  EHCI_CMD_ASE		0x00000020
#define  EHCI_CMD_PSE		0x00000010
#define  EHCI_CMD_FLS_M		0x0000000c
#define  EHCI_CMD_FLS(x)	(((x) >> 2) & 3)
#define  EHCI_CMD_HCRESET	0x00000002
#define  EHCI_CMD_RS		0x00000001

#define EHCI_USBSTS		0x04
#define  EHCI_STS_ASS		0x00008000
#define  EHCI_STS_PSS		0x00004000
#define  EHCI_STS_REC		0x00002000
#define  EHCI_STS_HCH		0x00001000
#define  EHCI_STS_IAA		0x00000020
#define  EHCI_STS_HSE		0x00000010
#define  EHCI_STS_FLR		0x00000008
#define  EHCI_STS_PCD		0x00000004
#define  EHCI_STS_ERRINT	0x00000002
#define  EHCI_STS_INT		0x00000001
#define  EHCI_STS_INTRS(x)	((x) & 0x3f)

#define EHCI_NORMAL_INTRS (EHCI_STS_IAA | EHCI_STS_HSE | EHCI_STS_PCD | EHCI_STS_ERRINT | EHCI_STS_INT)

#define EHCI_USBINTR		0x08
#define EHCI_INTR_IAAE		0x00000020
#define EHCI_INTR_HSEE		0x00000010
#define EHCI_INTR_FLRE		0x00000008
#define EHCI_INTR_PCIE		0x00000004
#define EHCI_INTR_UEIE		0x00000002
#define EHCI_INTR_UIE		0x00000001

#define EHCI_FRINDEX		0x0c

#define EHCI_CTRLDSSEGMENT	0x10

#define EHCI_PERIODICLISTBASE	0x14
#define EHCI_ASYNCLISTADDR	0x18

#define EHCI_CONFIGFLAG		0x40
#define  EHCI_CONF_CF		0x00000001

#define EHCI_PORTSC(n)		(0x40+4*(n))
#define  EHCI_PS_WKOC_E		0x00400000
#define  EHCI_PS_WKDSCNNT_E	0x00200000
#define  EHCI_PS_WKCNNT_E	0x00100000
#define  EHCI_PS_PTC		0x000f0000
#define  EHCI_PS_PIC		0x0000c000
#define  EHCI_PS_PO		0x00002000
#define  EHCI_PS_PP		0x00001000
#define  EHCI_PS_LS		0x00000c00
#define  EHCI_PS_IS_LOWSPEED(x)	(((x) & EHCI_PS_LS) == 0x00000400)
#define  EHCI_PS_PR		0x00000100
#define  EHCI_PS_SUSP		0x00000080
#define  EHCI_PS_FPR		0x00000040
#define  EHCI_PS_OCC		0x00000020
#define  EHCI_PS_OCA		0x00000010
#define  EHCI_PS_PEC		0x00000008
#define  EHCI_PS_PE		0x00000004
#define  EHCI_PS_CSC		0x00000002
#define  EHCI_PS_CS		0x00000001
#define  EHCI_PS_CLEAR		(EHCI_PS_OCC|EHCI_PS_PEC|EHCI_PS_CSC)

#define EHCI_PORT_RESET_COMPLETE 2

#define EHCI_USBMODE		0x68
#define  EHCI_USBMODE_CM_M	0x00000003
#define  EHCI_USBMODE_CM_IDLE	0x00000000
#define  EHCI_USBMODE_CM_DEVICE	0x00000002
#define  EHCI_USBMODE_CM_HOST	0x00000003

#define EHCI_FLALIGN_ALIGN	0x1000

#define EHCI_PAGE_SIZE		0x1000
#define EHCI_PAGE(x)		((x) &~ 0xfff)
#define EHCI_PAGE_OFFSET(x)	((x) & 0xfff)

typedef uint32_t ehci_link_t;
#define EHCI_LINK_TERMINATE	0x00000001
#define EHCI_LINK_TYPE(x)	((x) & 0x00000006)
#define  EHCI_LINK_ITD		0x0
#define  EHCI_LINK_QH		0x2
#define  EHCI_LINK_SITD		0x4
#define  EHCI_LINK_FSTN		0x6
#define EHCI_LINK_ADDR(x)	((x) &~ 0x1f)

typedef uint32_t ehci_physaddr_t;
typedef uint32_t ehci_isoc_trans_t;
typedef uint32_t ehci_isoc_bufr_ptr_t;
#define	EHCI_BUFPTR_MASK	0xfffff000

#define EHCI_ITD_NTRANS		8
#define EHCI_ITD_NBUFFERS	7
struct ehci_itd {
	volatile ehci_link_t            itd_next;
	volatile ehci_isoc_trans_t      itd_ctl[8];
#define EHCI_ITD_GET_STATUS(x)	(((x) >> 28) & 0xf)
#define EHCI_ITD_SET_STATUS(x)	(((x) & 0xf) << 28)
#define EHCI_ITD_ACTIVE		0x80000000
#define EHCI_ITD_BUF_ERR	0x40000000
#define EHCI_ITD_BABBLE		0x20000000
#define EHCI_ITD_ERROR		0x10000000
#define EHCI_ITD_GET_LEN(x)	(((x) >> 16) & 0xfff)
#define EHCI_ITD_SET_LEN(x)	(((x) & 0xfff) << 16)
#define EHCI_ITD_IOC		0x8000
#define EHCI_ITD_GET_IOC(x)	(((x) >> 15) & 1)
#define EHCI_ITD_SET_IOC(x)	(((x) << 15) & EHCI_ITD_IOC)
#define EHCI_ITD_GET_PG(x)	(((x) >> 12) & 0x7)
#define EHCI_ITD_SET_PG(x)	(((x) & 0x7) << 12)
#define EHCI_ITD_GET_OFFS(x)	(((x) >> 0) & 0xfff)
#define EHCI_ITD_SET_OFFS(x)	(((x) & 0xfff) << 0)
	volatile ehci_isoc_bufr_ptr_t   itd_bufr[7];
#define EHCI_ITD_GET_ENDPT(x)	(((x) >> 8) & 0xf)
#define EHCI_ITD_SET_ENDPT(x)	(((x) & 0xf) << 8)
#define EHCI_ITD_GET_DADDR(x)	((x) & 0x7f)
#define EHCI_ITD_SET_DADDR(x)	((x) & 0x7f)
#define EHCI_ITD_GET_DIR(x)	(((x) >> 11) & 1)
#define EHCI_ITD_SET_DIR(x)	(((x) & 1) << 11)
#define EHCI_ITD_GET_MAXPKT(x)	((x) & 0x7ff)
#define EHCI_ITD_SET_MAXPKT(x)	((x) & 0x7ff)
#define EHCI_ITD_GET_MULTI(x)	((x) & 0x3)
#define EHCI_ITD_SET_MULTI(x)	((x) & 0x3)
	volatile ehci_isoc_bufr_ptr_t	itd_bufr_hi[7];
};
#define EHCI_ITD_ALIGN		32

struct ehci_sitd {
	volatile ehci_link_t		sitd_next;
	volatile uint32_t		sitd_endp;
#define EHCI_SITD_GET_ADDR(x)	(((x) >>  0) & 0x7f)
#define EHCI_SITD_SET_ADDR(x)	(x)
#define EHCI_SITD_GET_ENDPT(x)	(((x) >>  8) & 0xf)
#define EHCI_SITD_SET_ENDPT(x)	((x) <<  8)
#define EHCI_SITD_GET_HUBA(x)	(((x) >> 16) & 0x7f)
#define EHCI_SITD_SET_HUBA(x)	((x) << 16)
#define EHCI_SITD_GET_PORT(x)	(((x) >> 23) & 0x7f)
#define EHCI_SITD_SET_PORT(x)	((x) << 23)
#define EHCI_SITD_GET_DIR(x)	(((x) >> 31) & 0x1)
#define EHCI_SITD_SET_DIR(x)	((x) << 31)
	volatile uint32_t		sitd_sched;
#define EHCI_SITD_GET_SMASK(x)	(((x) >>  0) & 0xff)
#define EHCI_SITD_SET_SMASK(x)	((x) <<  0)
#define EHCI_SITD_GET_CMASK(x)	(((x) >>  8) & 0xff)
#define EHCI_SITD_SET_CMASK(x)	((x) <<  8)
	volatile uint32_t		sitd_trans;
#define  EHCI_SITD_IOC		0x80000000
#define  EHCI_SITD_ACTIVE	0x80
#define  EHCI_SITD_ERR		0x40
#define  EHCI_SITD_BUFERR	0x20
#define  EHCI_SITD_BABBLE	0x10
#define  EHCI_SITD_XACTERR	0x08
#define  EHCI_SITD_MISSEDMICRO	0x04
#define  EHCI_SITD_SPLITXSTATE	0x02
#define EHCI_SITD_GET_LEN(x)	(((x) >> 16) & 0x3ff)
#define EHCI_SITD_SET_LEN(x)	(((x) & 0x3ff) << 16)
#define EHCI_SITD_GET_PG(x)	(((x) >> 30) & 0x1)
#define EHCI_SITD_SET_PG(x)	((x) << 30)
	volatile ehci_physaddr_t	sitd_bufr[2];
#define EHCI_SITD_GET_TCOUNT(x)	(((x) >>  0) & 0x7)
#define EHCI_SITD_SET_TCOUNT(x)	((x) << 0)
#define EHCI_SITD_GET_TP(x)	(((x) >>  3) & 0x3)
#define EHCI_SITD_SET_TP(x)	((x) <<  3)
#define  EHCI_SITD_TP_ALL	0x0
#define  EHCI_SITD_TP_BEGIN	0x1
#define  EHCI_SITD_TP_MIDDLE	0x2
#define  EHCI_SITD_TP_END	0x3
	volatile ehci_link_t		sitd_back;
	volatile ehci_physaddr_t	sitd_bufr_hi[2];
};
#define EHCI_SITD_ALIGN		32

#define EHCI_QTD_NBUFFERS	5
struct ehci_qtd {
	ehci_link_t	qtd_next;
	ehci_link_t	qtd_altnext;
	uint32_t	qtd_status;
#define EHCI_QTD_GET_STATUS(x)	(((x) >>  0) & 0xff)
#define EHCI_QTD_SET_STATUS(x)	((x) << 0)
#define  EHCI_QTD_ACTIVE	0x80
#define  EHCI_QTD_HALTED	0x40
#define  EHCI_QTD_BUFERR	0x20
#define  EHCI_QTD_BABBLE	0x10
#define  EHCI_QTD_XACTERR	0x08
#define  EHCI_QTD_MISSEDMICRO	0x04
#define  EHCI_QTD_SPLITXSTATE	0x02
#define  EHCI_QTD_PINGSTATE	0x01
#define  EHCI_QTD_STATERRS	0x7c
#define EHCI_QTD_GET_PID(x)	(((x) >>  8) & 0x3)
#define EHCI_QTD_SET_PID(x)	((x) <<  8)
#define  EHCI_QTD_PID_OUT	0x0
#define  EHCI_QTD_PID_IN	0x1
#define  EHCI_QTD_PID_SETUP	0x2
#define EHCI_QTD_GET_CERR(x)	(((x) >> 10) &  0x3)
#define EHCI_QTD_SET_CERR(x)	((x) << 10)
#define EHCI_QTD_GET_C_PAGE(x)	(((x) >> 12) &  0x7)
#define EHCI_QTD_SET_C_PAGE(x)	((x) << 12)
#define EHCI_QTD_GET_IOC(x)	(((x) >> 15) &  0x1)
#define EHCI_QTD_IOC		0x00008000
#define EHCI_QTD_GET_BYTES(x)	(((x) >> 16) &  0x7fff)
#define EHCI_QTD_SET_BYTES(x)	((x) << 16)
#define EHCI_QTD_GET_TOGGLE(x)	(((x) >> 31) &  0x1)
#define EHCI_QTD_SET_TOGGLE(x)	((x) << 31)
#define EHCI_QTD_TOGGLE_MASK	0x80000000
	ehci_physaddr_t	qtd_buffer[EHCI_QTD_NBUFFERS];
	ehci_physaddr_t qtd_buffer_hi[EHCI_QTD_NBUFFERS];
};
#define EHCI_QTD_ALIGN		32

struct ehci_qh {
	ehci_link_t	qh_link;
	uint32_t	qh_endp;
#define EHCI_QH_GET_ADDR(x)	(((x) >>  0) & 0x7f)
#define EHCI_QH_SET_ADDR(x)	(x)
#define EHCI_QH_ADDRMASK	0x0000007f
#define EHCI_QH_GET_INACT(x)	(((x) >>  7) & 0x01)
#define EHCI_QH_INACT		0x00000080
#define EHCI_QH_GET_ENDPT(x)	(((x) >>  8) & 0x0f)
#define EHCI_QH_SET_ENDPT(x)	((x) <<  8)
#define EHCI_QH_GET_EPS(x)	(((x) >> 12) & 0x03)
#define EHCI_QH_SET_EPS(x)	((x) << 12)
#define  EHCI_QH_SPEED_FULL	0x0
#define  EHCI_QH_SPEED_LOW	0x1
#define  EHCI_QH_SPEED_HIGH	0x2
#define EHCI_QH_GET_DTC(x)	(((x) >> 14) & 0x01)
#define EHCI_QH_DTC		0x00004000
#define EHCI_QH_GET_HRECL(x)	(((x) >> 15) & 0x01)
#define EHCI_QH_HRECL		0x00008000
#define EHCI_QH_GET_MPL(x)	(((x) >> 16) & 0x7ff)
#define EHCI_QH_SET_MPL(x)	((x) << 16)
#define EHCI_QH_MPLMASK		0x07ff0000
#define EHCI_QH_GET_CTL(x)	(((x) >> 27) & 0x01)
#define EHCI_QH_CTL		0x08000000
#define EHCI_QH_GET_NRL(x)	(((x) >> 28) & 0x0f)
#define EHCI_QH_SET_NRL(x)	((x) << 28)
	uint32_t	qh_endphub;
#define EHCI_QH_GET_SMASK(x)	(((x) >>  0) & 0xff)
#define EHCI_QH_SET_SMASK(x)	((x) <<  0)
#define EHCI_QH_GET_CMASK(x)	(((x) >>  8) & 0xff)
#define EHCI_QH_SET_CMASK(x)	((x) <<  8)
#define EHCI_QH_GET_HUBA(x)	(((x) >> 16) & 0x7f)
#define EHCI_QH_SET_HUBA(x)	((x) << 16)
#define EHCI_QH_GET_PORT(x)	(((x) >> 23) & 0x7f)
#define EHCI_QH_SET_PORT(x)	((x) << 23)
#define EHCI_QH_GET_MULT(x)	(((x) >> 30) & 0x03)
#define EHCI_QH_SET_MULT(x)	((x) << 30)
	ehci_link_t	qh_curqtd;
	struct ehci_qtd	qh_qtd;
};
#define EHCI_QH_ALIGN		32

struct ehci_fstn {
	ehci_link_t	fstn_link;
	ehci_link_t	fstn_back;
};
#define EHCI_FSTN_ALIGN		32

#endif
