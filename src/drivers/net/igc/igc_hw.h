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
/* Portions derived from OpenBSD igc_hw.h, BSD-3-Clause */

#ifndef _IGC_HW_H_
#define _IGC_HW_H_

#include "igc_osdep.h"
#include "igc_defines.h"
#include "igc_regs.h"

struct igc_hw;

#define IGC_FUNC_1  1

#define IGC_ALT_MAC_ADDRESS_OFFSET_LAN0  0
#define IGC_ALT_MAC_ADDRESS_OFFSET_LAN1  3

#define PCI_VENDOR_INTEL  0x8086

#define PCI_PRODUCT_INTEL_I220_V         0x0DC5
#define PCI_PRODUCT_INTEL_I221_V         0x0DC6
#define PCI_PRODUCT_INTEL_I225_BLANK_NVM 0x15FE
#define PCI_PRODUCT_INTEL_I225_I         0x15FD
#define PCI_PRODUCT_INTEL_I225_IT        0x0D9F
#define PCI_PRODUCT_INTEL_I225_K         0x3100
#define PCI_PRODUCT_INTEL_I225_K2        0x3101
#define PCI_PRODUCT_INTEL_I225_LM        0x15F2
#define PCI_PRODUCT_INTEL_I225_LMVP      0x5502
#define PCI_PRODUCT_INTEL_I225_V         0x15F3
#define PCI_PRODUCT_INTEL_I226_BLANK_NVM 0x125F
#define PCI_PRODUCT_INTEL_I226_IT        0x0DC7
#define PCI_PRODUCT_INTEL_I226_LM        0x125B
#define PCI_PRODUCT_INTEL_I226_LMVP      0x5503
#define PCI_PRODUCT_INTEL_I226_K         0x3102
#define PCI_PRODUCT_INTEL_I226_V         0x125C

enum igc_mac_type {
	igc_undefined = 0,
	igc_i225,
	igc_num_macs
};

enum igc_media_type {
	igc_media_type_unknown = 0,
	igc_media_type_copper = 1,
	igc_num_media_types
};

enum igc_nvm_type {
	igc_nvm_unknown = 0,
	igc_nvm_eeprom_spi,
	igc_nvm_flash_hw,
	igc_nvm_invm
};

enum igc_phy_type {
	igc_phy_unknown = 0,
	igc_phy_none,
	igc_phy_i225
};

enum igc_bus_type {
	igc_bus_type_unknown = 0,
	igc_bus_type_pci,
	igc_bus_type_pcix,
	igc_bus_type_pci_express,
	igc_bus_type_reserved
};

enum igc_bus_speed {
	igc_bus_speed_unknown = 0,
	igc_bus_speed_33,
	igc_bus_speed_66,
	igc_bus_speed_100,
	igc_bus_speed_120,
	igc_bus_speed_133,
	igc_bus_speed_2500,
	igc_bus_speed_5000,
	igc_bus_speed_reserved
};

enum igc_bus_width {
	igc_bus_width_unknown = 0,
	igc_bus_width_pcie_x1,
	igc_bus_width_pcie_x2,
	igc_bus_width_pcie_x4 = 4,
	igc_bus_width_pcie_x8 = 8,
	igc_bus_width_32,
	igc_bus_width_64,
	igc_bus_width_reserved
};

enum igc_fc_mode {
	igc_fc_none = 0,
	igc_fc_rx_pause,
	igc_fc_tx_pause,
	igc_fc_full,
	igc_fc_default = 0xFF
};

enum igc_ms_type {
	igc_ms_hw_default = 0,
	igc_ms_force_master,
	igc_ms_force_slave,
	igc_ms_auto
};

enum igc_smart_speed {
	igc_smart_speed_default = 0,
	igc_smart_speed_on,
	igc_smart_speed_off
};

struct igc_rx_desc {
	uint64_t buffer_addr;
	uint64_t length;
	uint16_t csum;
	uint8_t  status;
	uint8_t  errors;
	uint16_t special;
};

union igc_rx_desc_extended {
	struct {
		uint64_t buffer_addr;
		uint64_t reserved;
	} read;
	struct {
		struct {
			uint32_t mrq;
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

struct igc_tx_desc {
	uint64_t buffer_addr;
	union {
		uint32_t data;
		struct {
			uint16_t length;
			uint8_t  cso;
			uint8_t  cmd;
		} flags;
	} lower;
	union {
		uint32_t data;
		struct {
			uint8_t  status;
			uint8_t  css;
			uint16_t special;
		} fields;
	} upper;
};

struct igc_mac_operations {
	int  (*init_params)(struct igc_hw *);
	int  (*check_for_link)(struct igc_hw *);
	void (*clear_hw_cntrs)(struct igc_hw *);
	void (*clear_vfta)(struct igc_hw *);
	int  (*get_bus_info)(struct igc_hw *);
	void (*set_lan_id)(struct igc_hw *);
	int  (*get_link_up_info)(struct igc_hw *, uint16_t *, uint16_t *);
	void (*update_mc_addr_list)(struct igc_hw *, uint8_t *, uint32_t);
	int  (*reset_hw)(struct igc_hw *);
	int  (*init_hw)(struct igc_hw *);
	int  (*setup_link)(struct igc_hw *);
	int  (*setup_physical_interface)(struct igc_hw *);
	void (*write_vfta)(struct igc_hw *, uint32_t, uint32_t);
	void (*config_collision_dist)(struct igc_hw *);
	int  (*rar_set)(struct igc_hw *, uint8_t *, uint32_t);
	int  (*read_mac_addr)(struct igc_hw *);
	int  (*validate_mdi_setting)(struct igc_hw *);
	int  (*acquire_swfw_sync)(struct igc_hw *, uint16_t);
	void (*release_swfw_sync)(struct igc_hw *, uint16_t);
};

struct igc_phy_operations {
	int  (*init_params)(struct igc_hw *);
	int  (*acquire)(struct igc_hw *);
	int  (*check_reset_block)(struct igc_hw *);
	int  (*force_speed_duplex)(struct igc_hw *);
	int  (*get_info)(struct igc_hw *);
	int  (*set_page)(struct igc_hw *, uint16_t);
	int  (*read_reg)(struct igc_hw *, uint32_t, uint16_t *);
	int  (*read_reg_locked)(struct igc_hw *, uint32_t, uint16_t *);
	int  (*read_reg_page)(struct igc_hw *, uint32_t, uint16_t *);
	void (*release)(struct igc_hw *);
	int  (*reset)(struct igc_hw *);
	int  (*set_d0_lplu_state)(struct igc_hw *, bool);
	int  (*set_d3_lplu_state)(struct igc_hw *, bool);
	int  (*write_reg)(struct igc_hw *, uint32_t, uint16_t);
	int  (*write_reg_locked)(struct igc_hw *, uint32_t, uint16_t);
	int  (*write_reg_page)(struct igc_hw *, uint32_t, uint16_t);
	void (*power_up)(struct igc_hw *);
	void (*power_down)(struct igc_hw *);
};

struct igc_nvm_operations {
	int  (*init_params)(struct igc_hw *);
	int  (*acquire)(struct igc_hw *);
	int  (*read)(struct igc_hw *, uint16_t, uint16_t, uint16_t *);
	void (*release)(struct igc_hw *);
	void (*reload)(struct igc_hw *);
	int  (*update)(struct igc_hw *);
	int  (*validate)(struct igc_hw *);
	int  (*write)(struct igc_hw *, uint16_t, uint16_t, uint16_t *);
};

struct igc_mac_info {
	struct igc_mac_operations ops;
	uint8_t  addr[ETHER_ADDR_LEN];
	uint8_t  perm_addr[ETHER_ADDR_LEN];

	enum igc_mac_type type;

	uint32_t mc_filter_type;

	uint16_t current_ifs_val;
	uint16_t ifs_max_val;
	uint16_t ifs_min_val;
	uint16_t ifs_ratio;
	uint16_t ifs_step_size;
	uint16_t mta_reg_count;
	uint16_t uta_reg_count;

#define MAX_MTA_REG 128
	uint32_t mta_shadow[MAX_MTA_REG];
	uint16_t rar_entry_count;

	uint8_t  forced_speed_duplex;

	bool     asf_firmware_present;
	bool     autoneg;
	bool     get_link_status;
	uint32_t max_frame_size;
};

struct igc_phy_info {
	struct igc_phy_operations ops;
	enum igc_phy_type         type;
	enum igc_smart_speed      smart_speed;

	uint32_t addr;
	uint32_t id;
	uint32_t reset_delay_us;
	uint32_t revision;

	enum igc_media_type media_type;

	uint16_t autoneg_advertised;
	uint16_t autoneg_mask;

	uint8_t  mdix;

	bool     polarity_correction;
	bool     speed_downgraded;
	bool     autoneg_wait_to_complete;
};

struct igc_nvm_info {
	struct igc_nvm_operations ops;
	enum igc_nvm_type         type;

	uint16_t word_size;
	uint16_t delay_usec;
	uint16_t address_bits;
	uint16_t opcode_bits;
	uint16_t page_size;
};

struct igc_bus_info {
	enum igc_bus_type  type;
	enum igc_bus_speed speed;
	enum igc_bus_width width;

	uint16_t func;
	uint16_t pci_cmd_word;
};

struct igc_fc_info {
	uint32_t high_water;
	uint32_t low_water;
	uint16_t pause_time;
	uint16_t refresh_time;
	bool     send_xon;
	bool     strict_ieee;
	enum igc_fc_mode current_mode;
	enum igc_fc_mode requested_mode;
};

struct igc_dev_spec_i225 {
	bool     eee_disable;
	bool     clear_semaphore_once;
	uint32_t mtu;
};

struct igc_hw {
	void    *back;
	uint8_t *hw_addr;

	struct igc_mac_info mac;
	struct igc_fc_info  fc;
	struct igc_phy_info phy;
	struct igc_nvm_info nvm;
	struct igc_bus_info bus;

	union {
		struct igc_dev_spec_i225 _i225;
	} dev_spec;

	uint16_t device_id;
	uint16_t vendor_id;
	uint16_t subsystem_vendor_id;
	uint16_t subsystem_device_id;
	uint8_t  revision_id;
};

#endif /* _IGC_HW_H_ */
