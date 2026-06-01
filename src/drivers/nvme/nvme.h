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

#ifndef memory_barrier
#define memory_barrier()   __asm__ __volatile__("mfence" ::: "memory")
#endif

#define NVME_PCI_CLASS      0x01u
#define NVME_PCI_SUBCLASS   0x08u
#define NVME_PCI_PROGIF     0x02u
#define NVME_BAR_IDX        0u

#define NVME_REG_CAP        0x0000u
#define NVME_REG_VS         0x0008u
#define NVME_REG_INTMS      0x000Cu
#define NVME_REG_INTMC      0x0010u
#define NVME_REG_CC         0x0014u

#define NVME_REG_CSTS       0x001Cu
#define NVME_REG_NSSR       0x0020u
#define NVME_REG_AQA        0x0024u
#define NVME_REG_ASQ        0x0028u
#define NVME_REG_ACQ        0x0030u
#define NVME_REG_CMBLOC     0x0038u
#define NVME_REG_CMBSZ      0x003Cu
#define NVME_REG_BPINFO     0x0040u
#define NVME_REG_BPRSEL     0x0044u
#define NVME_REG_BPMBL      0x0048u
#define NVME_REG_PMRCAP     0x0E00u
#define NVME_REG_PMRCTL     0x0E04u

#define NVME_DOORBELL_BASE  0x1000u

#define NVME_CAP_MQES(cap)      (((cap) & 0xFFFFu) + 1u)

#define NVME_CAP_CQR            (1ULL << 16)

#define NVME_CAP_AMS(cap)       (((cap) >> 17u) & 0x7Fu)

#define NVME_CAP_TO(cap)        (((cap) >> 24u) & 0xFFu)

#define NVME_CAP_DSTRD(cap)     (((cap) >> 32u) & 0xFu)

#define NVME_CAP_NSSRS          (1ULL << 36)

#define NVME_CAP_CSS(cap)       (((cap) >> 37u) & 0x7Fu)
#define NVME_CAP_CSS_NVM        (1u)

#define NVME_CAP_BPS            (1ULL << 44)

#define NVME_CAP_MPSMIN(cap)    (((cap) >> 48u) & 0xFu)

#define NVME_CAP_MPSMAX(cap)    (((cap) >> 52u) & 0xFu)

#define NVME_CC_EN          (1u << 0)
#define NVME_CC_CSS_NVM     (0u << 4)
#define NVME_CC_MPS(n)      ((uint32_t)(n) << 7u)
#define NVME_CC_AMS_RR      (0u << 11)
#define NVME_CC_SHN_NONE    (0u << 14)
#define NVME_CC_SHN_NORMAL  (1u << 14)
#define NVME_CC_IOSQES(n)   ((uint32_t)(n) << 16u)
#define NVME_CC_IOCQES(n)   ((uint32_t)(n) << 20u)

#define NVME_SQ_ENTRY_SHIFT  6u
#define NVME_CQ_ENTRY_SHIFT  4u

#define NVME_CSTS_RDY       (1u << 0)
#define NVME_CSTS_CFS       (1u << 1)
#define NVME_CSTS_SHST_MASK (3u << 2)
#define NVME_CSTS_SHST_NONE (0u << 2)
#define NVME_CSTS_SHST_PROC (1u << 2)
#define NVME_CSTS_SHST_COMP (2u << 2)
#define NVME_CSTS_NSSRO     (1u << 4)
#define NVME_CSTS_PP        (1u << 5)

#define NVME_AQA(sqsz, cqsz) \
    ((uint32_t)(((sqsz) - 1u) & 0xFFFu) | \
     (uint32_t)((((cqsz) - 1u) & 0xFFFu) << 16u))

#define NVME_CDW0_OPC(op)   ((uint32_t)(op) & 0xFFu)
#define NVME_CDW0_FUSE_NONE (0u << 8)
#define NVME_CDW0_PSDT_PRP  (0u << 14)
#define NVME_CDW0_CID(cid)  ((uint32_t)(cid) << 16u)

typedef struct __attribute__((packed)) {
    uint32_t cdw0;
    uint32_t nsid;
    uint64_t _rsvd;
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} nvme_sqe_t;

_Static_assert(sizeof(nvme_sqe_t) == 64,
               "nvme_sqe_t must be 64 bytes");

typedef struct __attribute__((packed)) {
    uint32_t cdw0;
    uint32_t _rsvd;
    uint16_t sqhd;
    uint16_t sqid;
    uint16_t cid;
    uint16_t sf;
} nvme_cqe_t;

_Static_assert(sizeof(nvme_cqe_t) == 16,
               "nvme_cqe_t must be 16 bytes");

#define NVME_CQE_SF_P       (1u << 0)
#define NVME_CQE_SF_SC(sf)  (((sf) >> 1u) & 0xFFu)
#define NVME_CQE_SF_SCT(sf) (((sf) >> 9u) & 0x7u)
#define NVME_CQE_SF_DNR(sf) (((sf) >> 14u) & 0x1u)
#define NVME_CQE_SF_MORE(sf)(((sf) >> 13u) & 0x1u)

#define NVME_SC_SUCCESS     0x00u

#define NVME_ADMIN_DELETE_SQ    0x00u
#define NVME_ADMIN_CREATE_SQ    0x01u
#define NVME_ADMIN_GET_LOG_PAGE 0x02u
#define NVME_ADMIN_DELETE_CQ    0x04u
#define NVME_ADMIN_CREATE_CQ    0x05u
#define NVME_ADMIN_IDENTIFY     0x06u
#define NVME_ADMIN_ABORT        0x08u
#define NVME_ADMIN_SET_FEATURES 0x09u
#define NVME_ADMIN_GET_FEATURES 0x0Au
#define NVME_ADMIN_NS_MGMT      0x0Du
#define NVME_ADMIN_FW_COMMIT    0x10u
#define NVME_ADMIN_FW_DOWNLOAD  0x11u
#define NVME_ADMIN_NS_ATTACH    0x15u
#define NVME_ADMIN_KEEP_ALIVE   0x18u
#define NVME_ADMIN_FORMAT       0x80u
#define NVME_ADMIN_SECURITY_SEND 0x81u
#define NVME_ADMIN_SECURITY_RECV 0x82u

#define NVME_IDENTIFY_CNS_NS    0x00u
#define NVME_IDENTIFY_CNS_CTRL  0x01u
#define NVME_IDENTIFY_CNS_NSLIST 0x02u

#define NVME_IO_FLUSH       0x00u
#define NVME_IO_WRITE       0x01u
#define NVME_IO_READ        0x02u
#define NVME_IO_WRITE_UNCORR 0x04u
#define NVME_IO_COMPARE     0x05u
#define NVME_IO_WRITE_ZEROS 0x08u
#define NVME_IO_DATASET_MGMT 0x09u
#define NVME_IO_VERIFY      0x0Cu

#define NVME_CQ_CDW11_PC    (1u << 0)
#define NVME_CQ_CDW11_IEN   (1u << 1)
#define NVME_CQ_CDW11_IV(v) ((uint32_t)(v) << 16u)

#define NVME_SQ_CDW11_PC    (1u << 0)
#define NVME_SQ_CDW11_QPRIO_URGENT  (0u << 1)
#define NVME_SQ_CDW11_QPRIO_HIGH    (1u << 1)
#define NVME_SQ_CDW11_QPRIO_MEDIUM  (2u << 1)
#define NVME_SQ_CDW11_QPRIO_LOW     (3u << 1)
#define NVME_SQ_CDW11_CQID(id) ((uint32_t)(id) << 16u)

#define NVME_Q_CDW10(qid, sz)  ((uint32_t)((qid) & 0xFFFFu) | \
                                (uint32_t)(((sz) - 1u) << 16u))

#define NVME_ID_CTRL_VID        0u
#define NVME_ID_CTRL_SSVID      2u
#define NVME_ID_CTRL_SN         4u
#define NVME_ID_CTRL_MN         24u
#define NVME_ID_CTRL_FR         64u
#define NVME_ID_CTRL_MDTS       77u
#define NVME_ID_CTRL_CNTLID     78u
#define NVME_ID_CTRL_VER        80u
#define NVME_ID_CTRL_OACS       256u
#define NVME_ID_CTRL_NN         516u
#define NVME_ID_CTRL_ONCS       520u

#define NVME_ID_NS_NSZE         0u
#define NVME_ID_NS_NCAP         8u
#define NVME_ID_NS_NUSE         16u
#define NVME_ID_NS_NLBAF        25u
#define NVME_ID_NS_FLBAS        26u
#define NVME_ID_NS_LBAF_BASE    128u

#define NVME_LBAF_LBADS(lbaf)   (((lbaf) >> 16u) & 0xFFu)

#define NVME_ADMIN_QUEUE_DEPTH  64u

#define NVME_IOQ_DEPTH          256u

#define NVME_IOQ_ID             1u

#define NVME_MAX_DRIVES         4u

int nvme_init(void);

int nvme_read(uint32_t nsid, uint64_t slba, uint32_t count, void *buf);

int nvme_write(uint32_t nsid, uint64_t slba, uint32_t count, const void *buf);
