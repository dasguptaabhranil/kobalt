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
/* Portions derived from OpenBSD igc_i225.h, BSD-3-Clause */

#ifndef _IGC_I225_H_
#define _IGC_I225_H_

struct igc_hw;

bool igc_get_flash_presence_i225(struct igc_hw *);
int  igc_update_flash_i225(struct igc_hw *);
int  igc_update_nvm_checksum_i225(struct igc_hw *);
int  igc_validate_nvm_checksum_i225(struct igc_hw *);
int  igc_write_nvm_srwr_i225(struct igc_hw *, uint16_t, uint16_t, uint16_t *);
int  igc_read_nvm_srrd_i225(struct igc_hw *, uint16_t, uint16_t, uint16_t *);
int  igc_set_flsw_flash_burst_counter_i225(struct igc_hw *, uint32_t);
int  igc_write_erase_flash_command_i225(struct igc_hw *, uint32_t, uint32_t);
int  igc_check_for_link_i225(struct igc_hw *);
int  igc_acquire_swfw_sync_i225(struct igc_hw *, uint16_t);
void igc_release_swfw_sync_i225(struct igc_hw *, uint16_t);
int  igc_set_ltr_i225(struct igc_hw *, bool);
int  igc_init_hw_i225(struct igc_hw *);
int  igc_setup_copper_link_i225(struct igc_hw *);
int  igc_set_eee_i225(struct igc_hw *, bool, bool, bool);

#define ID_LED_DEFAULT_I225 \
	((ID_LED_OFF1_ON2 << 8) | (ID_LED_DEF1_DEF2 << 4) | (ID_LED_OFF1_OFF2))

#define ID_LED_DEFAULT_I225_SERDES \
	((ID_LED_DEF1_DEF2 << 8) | (ID_LED_DEF1_DEF2 << 4) | (ID_LED_OFF1_ON2))

#define NVM_INIT_CTRL_2_DEFAULT_I225  0x7243
#define NVM_INIT_CTRL_4_DEFAULT_I225  0x00C1
#define NVM_LED_1_CFG_DEFAULT_I225    0x0184
#define NVM_LED_0_2_CFG_DEFAULT_I225  0x200C

#define IGC_MRQC_ENABLE_RSS_4Q        0x00000002
#define IGC_MRQC_ENABLE_VMDQ          0x00000003
#define IGC_MRQC_ENABLE_VMDQ_RSS_2Q   0x00000005
#define IGC_MRQC_RSS_FIELD_IPV4_UDP   0x00400000
#define IGC_MRQC_RSS_FIELD_IPV6_UDP   0x00800000
#define IGC_MRQC_RSS_FIELD_IPV6_UDP_EX 0x01000000
#define IGC_I225_SHADOW_RAM_SIZE      4096
#define IGC_I225_ERASE_CMD_OPCODE     0x02000000
#define IGC_I225_WRITE_CMD_OPCODE     0x01000000
#define IGC_FLSWCTL_DONE              0x40000000
#define IGC_FLSWCTL_CMDV              0x10000000

#define IGC_SRRCTL_BSIZEHDRSIZE_MASK             0x00000F00
#define IGC_SRRCTL_DESCTYPE_LEGACY               0x00000000
#define IGC_SRRCTL_DESCTYPE_HDR_SPLIT            0x04000000
#define IGC_SRRCTL_DESCTYPE_HDR_SPLIT_ALWAYS     0x0A000000
#define IGC_SRRCTL_DESCTYPE_HDR_REPLICATION      0x06000000
#define IGC_SRRCTL_DESCTYPE_HDR_REPLICATION_LARGE_PKT  0x08000000
#define IGC_SRRCTL_DESCTYPE_MASK                 0x0E000000
#define IGC_SRRCTL_DROP_EN                       0x80000000
#define IGC_SRRCTL_BSIZEPKT_MASK                 0x0000007F
#define IGC_SRRCTL_BSIZEHDR_MASK                 0x00003F00

#define IGC_RXDADV_RSSTYPE_MASK       0x0000000F
#define IGC_RXDADV_RSSTYPE_SHIFT      12
#define IGC_RXDADV_HDRBUFLEN_MASK     0x7FE0
#define IGC_RXDADV_HDRBUFLEN_SHIFT    5
#define IGC_RXDADV_SPLITHEADER_EN     0x00001000
#define IGC_RXDADV_SPH                0x8000
#define IGC_RXDADV_STAT_TS            0x10000
#define IGC_RXDADV_ERR_HBO            0x00800000

#define IGC_RXDADV_RSSTYPE_NONE        0x00000000
#define IGC_RXDADV_RSSTYPE_IPV4_TCP    0x00000001
#define IGC_RXDADV_RSSTYPE_IPV4        0x00000002
#define IGC_RXDADV_RSSTYPE_IPV6_TCP    0x00000003
#define IGC_RXDADV_RSSTYPE_IPV6_EX     0x00000004
#define IGC_RXDADV_RSSTYPE_IPV6        0x00000005
#define IGC_RXDADV_RSSTYPE_IPV6_TCP_EX 0x00000006
#define IGC_RXDADV_RSSTYPE_IPV4_UDP    0x00000007
#define IGC_RXDADV_RSSTYPE_IPV6_UDP    0x00000008
#define IGC_RXDADV_RSSTYPE_IPV6_UDP_EX 0x00000009

#define IGC_RXDADV_PKTTYPE_ILMASK   0x000000F0
#define IGC_RXDADV_PKTTYPE_TLMASK   0x00000F00
#define IGC_RXDADV_PKTTYPE_NONE     0x00000000
#define IGC_RXDADV_PKTTYPE_IPV4     0x00000010
#define IGC_RXDADV_PKTTYPE_IPV4_EX  0x00000020
#define IGC_RXDADV_PKTTYPE_IPV6     0x00000040
#define IGC_RXDADV_PKTTYPE_IPV6_EX  0x00000080
#define IGC_RXDADV_PKTTYPE_TCP      0x00000100
#define IGC_RXDADV_PKTTYPE_UDP      0x00000200
#define IGC_RXDADV_PKTTYPE_SCTP     0x00000400
#define IGC_RXDADV_PKTTYPE_NFS      0x00000800
#define IGC_RXDADV_PKTTYPE_IPSEC_ESP  0x00001000
#define IGC_RXDADV_PKTTYPE_IPSEC_AH   0x00002000
#define IGC_RXDADV_PKTTYPE_LINKSEC    0x00004000
#define IGC_RXDADV_PKTTYPE_ETQF       0x00008000
#define IGC_RXDADV_PKTTYPE_ETQF_MASK  0x00000070
#define IGC_RXDADV_PKTTYPE_ETQF_SHIFT 4

#endif /* _IGC_I225_H_ */
