/* Copyright (C) 2026 Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
/* Portions derived from OpenBSD igc_base.h, BSD-3-Clause */

#ifndef _IGC_BASE_H_
#define _IGC_BASE_H_

#include "igc_hw.h"

int  igc_init_hw_base(struct igc_hw *hw);
void igc_power_down_phy_copper_base(struct igc_hw *hw);
int  igc_acquire_phy_base(struct igc_hw *hw);
void igc_release_phy_base(struct igc_hw *hw);

union igc_adv_tx_desc {
	struct {
		uint64_t buffer_addr;
		uint32_t cmd_type_len;
		uint32_t olinfo_status;
	} read;
	struct {
		uint64_t rsvd;
		uint32_t nxtseq_seed;
		uint32_t status;
	} wb;
};

struct igc_adv_tx_context_desc {
	uint32_t vlan_macip_lens;
	union {
		uint32_t launch_time;
		uint32_t seqnum_seed;
	};
	uint32_t type_tucmd_mlhl;
	uint32_t mss_l4len_idx;
};

#define IGC_ADVTXD_DTALEN_MASK          0x0000FFFF
#define IGC_ADVTXD_DTYP_CTXT            0x00200000
#define IGC_ADVTXD_DTYP_DATA            0x00300000
#define IGC_ADVTXD_DCMD_EOP             0x01000000
#define IGC_ADVTXD_DCMD_IFCS            0x02000000
#define IGC_ADVTXD_DCMD_RS              0x08000000
#define IGC_ADVTXD_DCMD_DDTYP_ISCSI    0x10000000
#define IGC_ADVTXD_DCMD_DEXT            0x20000000
#define IGC_ADVTXD_DCMD_VLE             0x40000000
#define IGC_ADVTXD_DCMD_TSE             0x80000000
#define IGC_ADVTXD_MAC_LINKSEC          0x00040000
#define IGC_ADVTXD_MAC_TSTAMP           0x00080000
#define IGC_ADVTXD_STAT_SN_CRC          0x00000002
#define IGC_ADVTXD_IDX_SHIFT            4
#define IGC_ADVTXD_POPTS_ISCO_1ST       0x00000000
#define IGC_ADVTXD_POPTS_ISCO_MDL       0x00000800
#define IGC_ADVTXD_POPTS_ISCO_LAST      0x00001000
#define IGC_ADVTXD_POPTS_ISCO_FULL      0x00001800
#define IGC_ADVTXD_POPTS_IPSEC          0x00000400
#define IGC_ADVTXD_PAYLEN_SHIFT         14
#define IGC_ADVTXD_PAYLEN_MASK          0xFFFFD000

#define IGC_ADVTXD_MACLEN_SHIFT         9
#define IGC_ADVTXD_VLAN_SHIFT           16
#define IGC_ADVTXD_TUCMD_IPV4           0x00000400
#define IGC_ADVTXD_TUCMD_IPV6           0x00000000
#define IGC_ADVTXD_TUCMD_L4T_UDP        0x00000000
#define IGC_ADVTXD_TUCMD_L4T_TCP        0x00000800
#define IGC_ADVTXD_TUCMD_L4T_SCTP       0x00001000
#define IGC_ADVTXD_TUCMD_IPSEC_TYPE_ESP 0x00002000
#define IGC_ADVTXD_TUCMD_IPSEC_ENCRYPT_EN  0x00004000
#define IGC_ADVTXD_TUCMD_MKRREQ         0x00002000
#define IGC_ADVTXD_L4LEN_SHIFT          8
#define IGC_ADVTXD_MSS_SHIFT            16
#define IGC_ADVTXD_IPSEC_SA_INDEX_MASK  0x000000FF
#define IGC_ADVTXD_IPSEC_ESP_LEN_MASK   0x000000FF

#define IGC_RAR_ENTRIES_BASE            16

union igc_adv_rx_desc {
	struct {
		uint64_t pkt_addr;
		uint64_t hdr_addr;
	} read;
	struct {
		struct {
			union {
				uint32_t data;
				struct {
					uint16_t pkt_info;
					uint16_t hdr_info;
				} hs_rss;
			} lo_dword;
			union {
				uint32_t rss;
				struct {
					uint16_t ip_id;
					uint16_t csum;
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			uint32_t status_error;
			uint16_t length;
			uint16_t vlan;
		} upper;
	} wb;
};

#define IGC_TXDCTL_QUEUE_ENABLE  0x02000000
#define IGC_RXDCTL_QUEUE_ENABLE  0x02000000

#define IGC_SRRCTL_BSIZEPKT_SHIFT       10
#define IGC_SRRCTL_BSIZEHDRSIZE_SHIFT   2
#define IGC_SRRCTL_DESCTYPE_ADV_ONEBUF  0x02000000

#endif /* _IGC_BASE_H_ */
