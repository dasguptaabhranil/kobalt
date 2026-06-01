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

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifndef compiler_barrier
#define compiler_barrier() __asm__ __volatile__("" ::: "memory")
#endif

#define AHCI_PCI_CLASS      0x01u
#define AHCI_PCI_SUBCLASS   0x06u
#define AHCI_PCI_PROGIF     0x01u
#define AHCI_BAR_IDX        5u

#define AHCI_HBA_CAP        0x00u
#define AHCI_HBA_GHC        0x04u
#define AHCI_HBA_IS         0x08u
#define AHCI_HBA_PI         0x0Cu
#define AHCI_HBA_VS         0x10u
#define AHCI_HBA_CCC_CTL    0x14u
#define AHCI_HBA_CCC_PORTS  0x18u
#define AHCI_HBA_EM_LOC     0x1Cu
#define AHCI_HBA_EM_CTL     0x20u
#define AHCI_HBA_CAP2       0x24u
#define AHCI_HBA_BOHC       0x28u

#define AHCI_GHC_AE         (1u << 31)
#define AHCI_GHC_MRSM       (1u << 2)
#define AHCI_GHC_IE         (1u << 1)
#define AHCI_GHC_HR         (1u << 0)

#define AHCI_CAP_NP_MASK    0x1Fu
#define AHCI_CAP_NCS_SHIFT  8u
#define AHCI_CAP_NCS_MASK   (0x1Fu << 8u)
#define AHCI_CAP_PSC        (1u << 13)
#define AHCI_CAP_SSC        (1u << 14)
#define AHCI_CAP_PMD        (1u << 15)
#define AHCI_CAP_FBSS       (1u << 16)
#define AHCI_CAP_SPM        (1u << 17)
#define AHCI_CAP_SAM        (1u << 18)
#define AHCI_CAP_ISS_MASK   (0xFu << 20)
#define AHCI_CAP_SCLO       (1u << 24)
#define AHCI_CAP_SAL        (1u << 25)
#define AHCI_CAP_SALP       (1u << 26)
#define AHCI_CAP_SSS        (1u << 27)
#define AHCI_CAP_SMPS       (1u << 28)
#define AHCI_CAP_SSNTF      (1u << 29)
#define AHCI_CAP_SNCQ       (1u << 30)
#define AHCI_CAP_S64A       (1u << 31)

#define AHCI_CAP2_BOH       (1u << 0)
#define AHCI_CAP2_NVMHCI    (1u << 1)
#define AHCI_CAP2_APST      (1u << 2)
#define AHCI_CAP2_SDS       (1u << 3)
#define AHCI_CAP2_SADM      (1u << 4)
#define AHCI_CAP2_DESO      (1u << 5)

#define AHCI_BOHC_OOH       (1u << 0)
#define AHCI_BOHC_SOO       (1u << 1)
#define AHCI_BOHC_OOCS      (1u << 2)
#define AHCI_BOHC_BOS       (1u << 3)
#define AHCI_BOHC_BB        (1u << 4)

#define AHCI_PORT_CLB       0x00u
#define AHCI_PORT_CLBU      0x04u
#define AHCI_PORT_FB        0x08u
#define AHCI_PORT_FBU       0x0Cu
#define AHCI_PORT_IS        0x10u
#define AHCI_PORT_IE        0x14u
#define AHCI_PORT_CMD       0x18u

#define AHCI_PORT_TFD       0x20u
#define AHCI_PORT_SIG       0x24u
#define AHCI_PORT_SSTS      0x28u
#define AHCI_PORT_SCTL      0x2Cu
#define AHCI_PORT_SERR      0x30u
#define AHCI_PORT_SACT      0x34u
#define AHCI_PORT_CI        0x38u
#define AHCI_PORT_SNTF      0x3Cu
#define AHCI_PORT_FBS       0x40u
#define AHCI_PORT_DEVSLP    0x44u

#define AHCI_CMD_ST         (1u << 0)
#define AHCI_CMD_SUD        (1u << 1)
#define AHCI_CMD_POD        (1u << 2)
#define AHCI_CMD_CLO        (1u << 3)
#define AHCI_CMD_FRE        (1u << 4)
#define AHCI_CMD_CCS_MASK   (0x1Fu << 8)
#define AHCI_CMD_MPSS       (1u << 13)
#define AHCI_CMD_FR         (1u << 14)
#define AHCI_CMD_CR         (1u << 15)
#define AHCI_CMD_CPS        (1u << 16)
#define AHCI_CMD_PMA        (1u << 17)
#define AHCI_CMD_HPCP       (1u << 18)
#define AHCI_CMD_MPSP       (1u << 19)
#define AHCI_CMD_CPD        (1u << 20)
#define AHCI_CMD_ESP        (1u << 21)
#define AHCI_CMD_FBSCP      (1u << 22)
#define AHCI_CMD_APSTE      (1u << 23)
#define AHCI_CMD_ATAPI      (1u << 24)
#define AHCI_CMD_DLAE       (1u << 25)
#define AHCI_CMD_ALPE       (1u << 26)
#define AHCI_CMD_ASP        (1u << 27)
#define AHCI_CMD_ICC_MASK   (0xFu << 28)
#define AHCI_CMD_ICC_ACTIVE (1u << 28)
#define AHCI_CMD_ICC_SLEEP  (6u << 28)

#define AHCI_TFD_ERR        (1u << 0)
#define AHCI_TFD_DRQ        (1u << 3)
#define AHCI_TFD_BSY        (1u << 7)

#define AHCI_SSTS_DET_MASK  0x00Fu
#define AHCI_SSTS_DET_NONE  0x0u
#define AHCI_SSTS_DET_CONN  0x3u
#define AHCI_SSTS_DET_OFFLN 0x4u
#define AHCI_SSTS_SPD_MASK  0x0F0u
#define AHCI_SSTS_IPM_MASK  0xF00u
#define AHCI_SSTS_IPM_ACT   0x100u
#define AHCI_SSTS_IPM_PART  0x200u
#define AHCI_SSTS_IPM_SLUM  0x600u

#define AHCI_IS_DHRS        (1u << 0)
#define AHCI_IS_PSS         (1u << 1)
#define AHCI_IS_DSS         (1u << 2)
#define AHCI_IS_SDBS        (1u << 3)
#define AHCI_IS_UFS         (1u << 4)
#define AHCI_IS_DPS         (1u << 5)
#define AHCI_IS_PCS         (1u << 6)
#define AHCI_IS_DMPS        (1u << 7)
#define AHCI_IS_PRCS        (1u << 22)
#define AHCI_IS_IPMS        (1u << 23)
#define AHCI_IS_OFS         (1u << 24)
#define AHCI_IS_INFS        (1u << 26)
#define AHCI_IS_IFS         (1u << 27)
#define AHCI_IS_HBDS        (1u << 28)
#define AHCI_IS_HBFS        (1u << 29)
#define AHCI_IS_TFES        (1u << 30)
#define AHCI_IS_CPDS        (1u << 31)

#define AHCI_IS_FATAL_MASK  (AHCI_IS_IFS  | AHCI_IS_HBDS | \
                             AHCI_IS_HBFS | AHCI_IS_TFES)

#define AHCI_SIG_SATA       0x00000101u
#define AHCI_SIG_SATAPI     0xEB140101u
#define AHCI_SIG_SEMB       0xC33C0101u
#define AHCI_SIG_PM         0x96690101u

#define ATA_CMD_IDENTIFY            0xECu
#define ATA_CMD_IDENTIFY_PACKET     0xA1u
#define ATA_CMD_READ_DMA_EXT        0x25u
#define ATA_CMD_WRITE_DMA_EXT       0x35u
#define ATA_CMD_FLUSH_EXT           0xEAu
#define ATA_CMD_READ_FPDMA_QUEUED   0x60u
#define ATA_CMD_WRITE_FPDMA_QUEUED  0x61u

#define ATA_DEV_LBA         (1u << 6)
#define ATA_DEV_OBS         (1u << 7) | (1u << 5)

#define FIS_TYPE_H2D        0x27u
#define FIS_TYPE_D2H        0x34u
#define FIS_TYPE_DMA_ACT    0x39u
#define FIS_TYPE_DMA_SETUP  0x41u
#define FIS_TYPE_DATA       0x46u
#define FIS_TYPE_BIST       0x58u
#define FIS_TYPE_PIO_SETUP  0x5Fu
#define FIS_TYPE_D2H_SDB    0xA1u

#define FIS_H2D_C           (1u << 7)

#define AHCI_MAX_PORTS      8u

#define AHCI_MAX_CMD_SLOTS  32u

#define AHCI_PRDT_MAX       8u

#define AHCI_CH_CFL(n)      ((n) & 0x1Fu)
#define AHCI_CH_ATAPI       (1u << 5)
#define AHCI_CH_WRITE       (1u << 6)
#define AHCI_CH_PREFETCH    (1u << 7)
#define AHCI_CH_RESET       (1u << 8)
#define AHCI_CH_CLR_BUSY    (1u << 10)
#define AHCI_CH_PMP(p)      (((p) & 0xFu) << 12)

typedef struct __attribute__((packed)) {
    uint16_t    flags;
    uint16_t    prdtl;
    uint32_t    prdbc;
    uint32_t    ctba;
    uint32_t    ctbau;
    uint32_t    _rsvd[4];
} ahci_cmd_hdr_t;

_Static_assert(sizeof(ahci_cmd_hdr_t) == 32,
               "ahci_cmd_hdr_t must be 32 bytes");

#define AHCI_PRDT_I         (1u << 31)

#define AHCI_PRDT_DBC(bytes)  (((bytes) - 1u) & 0x3FFFFFu)

typedef struct __attribute__((packed)) {
    uint32_t    dba;
    uint32_t    dbau;
    uint32_t    _rsvd;
    uint32_t    dbc;
} ahci_prdt_entry_t;

_Static_assert(sizeof(ahci_prdt_entry_t) == 16,
               "ahci_prdt_entry_t must be 16 bytes");

typedef struct __attribute__((packed)) {
    uint8_t           cfis[64];
    uint8_t           acmd[16];
    uint8_t           _rsvd[48];
    ahci_prdt_entry_t prdt[AHCI_PRDT_MAX];
} ahci_cmd_tbl_t;

_Static_assert(sizeof(ahci_cmd_tbl_t) >= 128u,
               "ahci_cmd_tbl_t sanity check failed");

typedef struct __attribute__((packed)) {
    uint8_t dsfis[28];
    uint8_t _pad0[4];
    uint8_t psfis[20];
    uint8_t _pad1[12];
    uint8_t rfis[20];
    uint8_t _pad2[4];
    uint8_t sdbfis[8];
    uint8_t ufis[64];
    uint8_t _rsvd[96];
} ahci_recv_fis_t;

_Static_assert(sizeof(ahci_recv_fis_t) == 256,
               "ahci_recv_fis_t must be 256 bytes");

#define ATA_IDENT_GENERAL       0u
#define ATA_IDENT_SERIAL        20u
#define ATA_IDENT_FIRMWARE      46u
#define ATA_IDENT_MODEL         54u
#define ATA_IDENT_CAPS          98u
#define ATA_IDENT_FIELD_VALID   106u
#define ATA_IDENT_MULTIWORD_DMA 128u
#define ATA_IDENT_PIO_MODES     130u
#define ATA_IDENT_COMMAND_SETS  164u
#define ATA_IDENT_UDMA_MODES    176u
#define ATA_IDENT_LBA28_LO      120u
#define ATA_IDENT_LBA28_HI      122u
#define ATA_IDENT_LBA48_0       200u
#define ATA_IDENT_LBA48_1       202u
#define ATA_IDENT_LBA48_2       204u
#define ATA_IDENT_LBA48_3       206u
#define ATA_IDENT_SECTOR_SIZE   234u
#define ATA_IDENT_PHY_SECT      212u

#define ATA_IDENT_GEN_ATAPI     (1u << 15)

#define ATA_CMDSET_LBA48        (1u << 10)

typedef struct {
    volatile void       *port_regs;
    ahci_cmd_hdr_t      *cmd_list;
    ahci_recv_fis_t     *recv_fis;
    ahci_cmd_tbl_t      *cmd_tbl;
    uint32_t             num_slots;
    uint64_t             num_sectors;
    uint32_t             sector_size;
    int                  valid;
    int                  atapi;
    char                 model[41];
    char                 serial[21];
} ahci_port_t;

int ahci_init(void);

int ahci_read(int port_idx, uint64_t lba, uint32_t count, void *buf);

int ahci_write(int port_idx, uint64_t lba, uint32_t count, const void *buf);
