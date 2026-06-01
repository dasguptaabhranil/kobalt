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

#ifndef VIRTIO_H
#define VIRTIO_H

#include <kernel.h>
#include <pci.h>

#define VIRTIO_PCI_CAP_VENDOR_ID    0x09u

#define VIRTIO_PCI_CAP_COMMON_CFG   1u
#define VIRTIO_PCI_CAP_NOTIFY_CFG   2u
#define VIRTIO_PCI_CAP_ISR_CFG      3u
#define VIRTIO_PCI_CAP_DEVICE_CFG   4u
#define VIRTIO_PCI_CAP_PCI_CFG      5u

typedef struct __attribute__((packed)) {
    uint8_t  cap_vndr;
    uint8_t  cap_next;
    uint8_t  cap_len;
    uint8_t  cfg_type;
    uint8_t  bar;
    uint8_t  _padding[3];
    uint32_t offset;
    uint32_t length;
} virtio_pci_cap_t;

_Static_assert(sizeof(virtio_pci_cap_t) == 16,
               "virtio_pci_cap_t must be 16 bytes");

typedef struct __attribute__((packed)) {
    virtio_pci_cap_t  cap;
    uint32_t          notify_off_multiplier;
} virtio_pci_notify_cap_t;

typedef struct __attribute__((packed)) {

    uint32_t    device_feature_select;
    uint32_t    device_feature;

    uint32_t    driver_feature_select;
    uint32_t    driver_feature;

    uint16_t    msix_config;
    uint16_t    num_queues;

    uint8_t     device_status;
    uint8_t     config_generation;

    uint16_t    queue_select;
    uint16_t    queue_size;
    uint16_t    queue_msix_vector;
    uint16_t    queue_enable;
    uint16_t    queue_notify_off;
    uint64_t    queue_desc;
    uint64_t    queue_driver;
    uint64_t    queue_device;
} virtio_common_cfg_t;

_Static_assert(sizeof(virtio_common_cfg_t) == 56,
               "virtio_common_cfg_t must be 56 bytes");

#define VIRTIO_STATUS_ACKNOWLEDGE   (1u << 0)
#define VIRTIO_STATUS_DRIVER        (1u << 1)
#define VIRTIO_STATUS_DRIVER_OK     (1u << 2)
#define VIRTIO_STATUS_FEATURES_OK   (1u << 3)
#define VIRTIO_STATUS_NEEDS_RESET   (1u << 6)
#define VIRTIO_STATUS_FAILED        (1u << 7)

#define VIRTIO_F_NOTIFY_ON_EMPTY    (1u << 24)
#define VIRTIO_F_ANY_LAYOUT         (1u << 27)
#define VIRTIO_F_RING_INDIRECT_DESC (1u << 28)
#define VIRTIO_F_RING_EVENT_IDX     (1u << 29)
#define VIRTIO_F_VERSION_1          (1ULL << 32)

#define VIRTQ_DESC_F_NEXT       (1u << 0)
#define VIRTQ_DESC_F_WRITE      (1u << 1)
#define VIRTQ_DESC_F_INDIRECT   (1u << 2)

typedef struct __attribute__((packed)) {
    uint64_t    addr;
    uint32_t    len;
    uint16_t    flags;
    uint16_t    next;
} virtq_desc_t;

_Static_assert(sizeof(virtq_desc_t) == 16,
               "virtq_desc_t must be 16 bytes");

#define VIRTQ_AVAIL_F_NO_INTERRUPT  (1u << 0)

typedef struct __attribute__((packed)) {
    uint16_t    flags;
    uint16_t    idx;
    uint16_t    ring[];

} virtq_avail_t;

#define VIRTQ_USED_F_NO_NOTIFY  (1u << 0)

typedef struct __attribute__((packed)) {
    uint32_t    id;
    uint32_t    len;
} virtq_used_elem_t;

typedef struct __attribute__((packed)) {
    uint16_t        flags;
    uint16_t        idx;
    virtq_used_elem_t ring[];

} virtq_used_t;

#define VIRTQ_SIZE              256u
#define VIRTQ_DESC_TABLE_BYTES  (16u * VIRTQ_SIZE)
#define VIRTQ_AVAIL_BYTES       (6u  + 2u * VIRTQ_SIZE)
#define VIRTQ_USED_BYTES        (6u  + 8u * VIRTQ_SIZE)

typedef struct {

    volatile virtq_desc_t   *desc;
    volatile virtq_avail_t  *avail;
    volatile virtq_used_t   *used;

    volatile uint16_t       *notify;

    uint16_t    queue_size;
    uint16_t    last_used_idx;
    uint16_t    free_head;
    uint16_t    num_free;
} virtqueue_t;

int virtio_net_init(void);

int virtio_net_send(const void *buf, uint16_t len);

void virtio_net_poll(void (*rx_cb)(const void *buf, uint16_t len));

void virtio_net_mac(uint8_t out[6]);
void net_poll(void);

#endif
