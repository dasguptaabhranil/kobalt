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

#ifndef PCI_H
#define PCI_H

#include <kernel.h>

typedef struct __attribute__((packed)) {
    uint64_t    base_addr;
    uint16_t    segment_group;
    uint8_t     start_bus;
    uint8_t     end_bus;
    uint32_t    _reserved;
} pci_mcfg_entry_t;

_Static_assert(sizeof(pci_mcfg_entry_t) == 16,
               "pci_mcfg_entry_t must be 16 bytes");

typedef struct __attribute__((packed)) {

    char        signature[4];
    uint32_t    length;
    uint8_t     revision;
    uint8_t     checksum;
    char        oem_id[6];
    char        oem_table_id[8];
    uint32_t    oem_revision;
    uint32_t    creator_id;
    uint32_t    creator_revision;

    uint64_t    _reserved;
    pci_mcfg_entry_t entries[];
} pci_mcfg_t;

#define PCI_CFG_VENDOR_ID       0x00u
#define PCI_CFG_DEVICE_ID       0x02u
#define PCI_CFG_COMMAND         0x04u
#define PCI_CFG_STATUS          0x06u
#define PCI_CFG_REVISION        0x08u
#define PCI_CFG_PROG_IF         0x09u
#define PCI_CFG_SUBCLASS        0x0Au
#define PCI_CFG_CLASS           0x0Bu
#define PCI_CFG_CACHE_LINE      0x0Cu
#define PCI_CFG_LATENCY         0x0Du
#define PCI_CFG_HEADER_TYPE     0x0Eu
#define PCI_CFG_BIST            0x0Fu

#define PCI_CFG_BAR0            0x10u
#define PCI_CFG_BAR1            0x14u
#define PCI_CFG_BAR2            0x18u
#define PCI_CFG_BAR3            0x1Cu
#define PCI_CFG_BAR4            0x20u
#define PCI_CFG_BAR5            0x24u
#define PCI_CFG_CARDBUS_CIS     0x28u
#define PCI_CFG_SUBSYS_VENDOR   0x2Cu
#define PCI_CFG_SUBSYS_ID       0x2Eu
#define PCI_CFG_EXPROM_BASE     0x30u
#define PCI_CFG_CAP_PTR         0x34u
#define PCI_CFG_IRQ_LINE        0x3Cu
#define PCI_CFG_IRQ_PIN         0x3Du
#define PCI_CFG_MIN_GNT         0x3Eu
#define PCI_CFG_MAX_LAT         0x3Fu

#define PCI_CFG_PRIMARY_BUS     0x18u
#define PCI_CFG_SECONDARY_BUS   0x19u
#define PCI_CFG_SUBORDINATE_BUS 0x1Au

#define PCI_CMD_IO_SPACE        (1u << 0)
#define PCI_CMD_MEM_SPACE       (1u << 1)
#define PCI_CMD_BUS_MASTER      (1u << 2)
#define PCI_CMD_SPECIAL_CYCLES  (1u << 3)
#define PCI_CMD_MWI             (1u << 4)
#define PCI_CMD_VGA_PALETTE_SNP (1u << 5)
#define PCI_CMD_PARITY_ERR_RESP (1u << 6)
#define PCI_CMD_SERR_EN         (1u << 8)
#define PCI_CMD_FAST_B2B_EN     (1u << 9)
#define PCI_CMD_INTX_DISABLE    (1u << 10)

#define PCI_STS_IMM_READINESS   (1u << 0)
#define PCI_STS_INTX_STATUS     (1u << 3)
#define PCI_STS_CAP_LIST        (1u << 4)
#define PCI_STS_66MHZ           (1u << 5)
#define PCI_STS_FAST_B2B        (1u << 7)
#define PCI_STS_MASTER_DATA_PAR (1u << 8)
#define PCI_STS_DEVSEL_MASK     (3u << 9)
#define PCI_STS_TARGET_ABORT_TX (1u << 11)
#define PCI_STS_TARGET_ABORT_RX (1u << 12)
#define PCI_STS_MASTER_ABORT    (1u << 13)
#define PCI_STS_SERR            (1u << 14)
#define PCI_STS_PARITY_ERR      (1u << 15)

#define PCI_HDR_TYPE_MASK       0x7Fu
#define PCI_HDR_MULTI_FUNC      0x80u
#define PCI_HDR_TYPE_ENDPOINT   0x00u
#define PCI_HDR_TYPE_PCI_BRIDGE 0x01u
#define PCI_HDR_TYPE_CARDBUS    0x02u

#define PCI_BAR_SPACE_IO        (1u << 0)
#define PCI_BAR_MEM_TYPE_MASK   0x6u
#define PCI_BAR_MEM_TYPE_32     0x0u
#define PCI_BAR_MEM_TYPE_64     0x4u
#define PCI_BAR_MEM_PREFETCH    (1u << 3)
#define PCI_BAR_MEM_ADDR_MASK   (~0xFu)
#define PCI_BAR_IO_ADDR_MASK    (~0x3u)

#define PCI_CAP_ID_PM           0x01u
#define PCI_CAP_ID_AGP          0x02u
#define PCI_CAP_ID_VPD          0x03u
#define PCI_CAP_ID_MSI          0x05u
#define PCI_CAP_ID_PCIX         0x07u
#define PCI_CAP_ID_HT           0x08u
#define PCI_CAP_ID_VENDOR       0x09u
#define PCI_CAP_ID_PCIE         0x10u
#define PCI_CAP_ID_MSIX         0x11u
#define PCI_CAP_ID_SATA         0x12u
#define PCI_CAP_ID_AF           0x13u

#define PCI_CLASS_UNCLASSIFIED  0x00u
#define PCI_CLASS_STORAGE       0x01u
#define PCI_CLASS_NETWORK       0x02u
#define PCI_CLASS_DISPLAY       0x03u
#define PCI_CLASS_MULTIMEDIA    0x04u
#define PCI_CLASS_MEMORY        0x05u
#define PCI_CLASS_BRIDGE        0x06u
#define PCI_CLASS_COMM          0x07u
#define PCI_CLASS_PERIPHERAL    0x08u
#define PCI_CLASS_INPUT         0x09u
#define PCI_CLASS_DOCKING       0x0Au
#define PCI_CLASS_PROCESSOR     0x0Bu
#define PCI_CLASS_SERIAL_BUS    0x0Cu
#define PCI_CLASS_WIRELESS      0x0Du
#define PCI_CLASS_SATELLITE     0x0Fu
#define PCI_CLASS_CRYPTO        0x10u
#define PCI_CLASS_SIGNAL        0x11u
#define PCI_CLASS_ACCELERATOR   0x12u
#define PCI_CLASS_UNDEFINED     0xFFu

#define PCI_SUBCLASS_HOST_BRIDGE    0x00u
#define PCI_SUBCLASS_ISA_BRIDGE     0x01u
#define PCI_SUBCLASS_PCI_BRIDGE     0x04u

#define PCI_MAX_DEVICES     512u
#define PCI_MAX_BARS        6u

typedef struct {

    uint8_t     bus;
    uint8_t     device;
    uint8_t     function;

    uint16_t    vendor_id;
    uint16_t    device_id;
    uint16_t    subsystem_vendor;
    uint16_t    subsystem_id;
    uint8_t     revision;

    uint8_t     class_code;
    uint8_t     subclass;
    uint8_t     prog_if;

    uint8_t     header_type;
    uint8_t     irq_line;
    uint8_t     irq_pin;

    uintptr_t   bar[PCI_MAX_BARS];
} kobalt_pci_dev_t;

typedef kobalt_pci_dev_t pci_device_t;

void pci_init(void);

uint32_t pci_count(void);

pci_device_t *pci_get_device(uint32_t index);

pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id);

pci_device_t *pci_find_class(uint8_t class_code, uint8_t subclass);

void pci_list_devices(void);

uint8_t  pci_read_config8 (pci_device_t *dev, uint16_t offset);
uint16_t pci_read_config16(pci_device_t *dev, uint16_t offset);
uint32_t pci_read_config32(pci_device_t *dev, uint16_t offset);

void pci_write_config8 (pci_device_t *dev, uint16_t offset, uint8_t  val);
void pci_write_config16(pci_device_t *dev, uint16_t offset, uint16_t val);
void pci_write_config32(pci_device_t *dev, uint16_t offset, uint32_t val);

uintptr_t pci_bar_base(pci_device_t *dev, uint8_t bar_idx);

uint32_t pci_bar_size(pci_device_t *dev, uint8_t bar_idx);

void pci_enable_device(pci_device_t *dev);

#endif
