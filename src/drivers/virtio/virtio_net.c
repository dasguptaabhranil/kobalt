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

#include <virtio.h>
#include <pci.h>
#include <kfmt.h>

#define VIRTIO_VENDOR_ID        0x1AF4u
#define VIRTIO_NET_DEVICE_MODERN    0x1041u
#define VIRTIO_NET_DEVICE_LEGACY    0x1000u

#define VIRTIO_NET_F_CSUM           (1u << 0)
#define VIRTIO_NET_F_GUEST_CSUM     (1u << 1)
#define VIRTIO_NET_F_CTRL_GUEST_OFFLOADS (1u << 2)
#define VIRTIO_NET_F_MTU            (1u << 3)
#define VIRTIO_NET_F_MAC            (1u << 5)
#define VIRTIO_NET_F_GUEST_TSO4     (1u << 7)
#define VIRTIO_NET_F_GUEST_TSO6     (1u << 8)
#define VIRTIO_NET_F_GUEST_ECN      (1u << 9)
#define VIRTIO_NET_F_GUEST_UFO      (1u << 10)
#define VIRTIO_NET_F_HOST_TSO4      (1u << 11)
#define VIRTIO_NET_F_HOST_TSO6      (1u << 12)
#define VIRTIO_NET_F_HOST_ECN       (1u << 13)
#define VIRTIO_NET_F_HOST_UFO       (1u << 14)
#define VIRTIO_NET_F_MRG_RXBUF      (1u << 15)
#define VIRTIO_NET_F_STATUS         (1u << 16)
#define VIRTIO_NET_F_CTRL_VQ        (1u << 17)
#define VIRTIO_NET_F_CTRL_RX        (1u << 18)
#define VIRTIO_NET_F_CTRL_VLAN      (1u << 19)
#define VIRTIO_NET_F_CTRL_RX_EXTRA  (1u << 20)
#define VIRTIO_NET_F_GUEST_ANNOUNCE (1u << 21)
#define VIRTIO_NET_F_MQ             (1u << 22)
#define VIRTIO_NET_F_CTRL_MAC_ADDR  (1u << 23)

#define VIRTIO_F_VERSION_1           (1ULL << 32)
#define VIRTIO_F_ACCESS_PLATFORM    (1ULL << 33)

#define DRIVER_FEATURES_LO  (VIRTIO_NET_F_MAC | VIRTIO_NET_F_STATUS)
#define DRIVER_FEATURES_HI  ((uint32_t)((VIRTIO_F_VERSION_1 | VIRTIO_F_ACCESS_PLATFORM) >> 32))

typedef struct __attribute__((packed)) {
    uint8_t     mac[6];
    uint16_t    status;
    uint16_t    max_virtqueue_pairs;
    uint16_t    mtu;
} virtio_net_cfg_t;

#define VIRTIO_NET_S_LINK_UP    (1u << 0)
#define VIRTIO_NET_S_ANNOUNCE   (1u << 1)

typedef struct virtio_net_hdr {
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;
} __attribute__((packed)) virtio_net_hdr_t;

#define VIRTIO_NET_HDR_F_NEEDS_CSUM     1u
#define VIRTIO_NET_GSO_NONE             0u
#define VIRTIO_NET_GSO_TCPV4            1u
#define VIRTIO_NET_GSO_UDP              3u
#define VIRTIO_NET_GSO_TCPV6            4u
#define VIRTIO_NET_GSO_ECN              0x80u

#define VQ_RX   0u
#define VQ_TX   1u

#define DECLARE_QUEUE_MEM(name)                                             \
    static virtq_desc_t  name##_desc[VIRTQ_SIZE]                           \
        __attribute__((aligned(4096)));                                     \
    static uint8_t       name##_avail_raw[VIRTQ_AVAIL_BYTES + 2]           \
        __attribute__((aligned(4096)));                                     \
    static uint8_t       name##_used_raw[VIRTQ_USED_BYTES + 2]             \
        __attribute__((aligned(4096)));

DECLARE_QUEUE_MEM(rxq)
DECLARE_QUEUE_MEM(txq)

#define VIRTIO_NET_HDR_SIZE sizeof(virtio_net_hdr_t)
#define ETH_FRAME_MAX       1514
#define RX_BUF_SIZE         (VIRTIO_NET_HDR_SIZE + ETH_FRAME_MAX)
static uint8_t g_rx_bufs[VIRTQ_SIZE][RX_BUF_SIZE] __attribute__((aligned(64)));

static virtio_net_hdr_t g_tx_hdrs[VIRTQ_SIZE] __attribute__((aligned(16)));

#define TX_BUF_SIZE     1536u
static uint8_t g_tx_payload_bufs[VIRTQ_SIZE][TX_BUF_SIZE]
    __attribute__((aligned(64)));

static volatile virtio_common_cfg_t *g_common  = NULL;
static volatile virtio_net_cfg_t    *g_net_cfg = NULL;
static volatile uint8_t             *g_isr      = NULL;
static volatile uint8_t             *g_notify_base = NULL;
static uint32_t                      g_notify_mult = 0;

static virtqueue_t  g_vq[2];
static uint8_t      g_mac[6];
static int          g_initialised = 0;

static inline uint8_t mmio_read8(const volatile void *addr)
{
    return *(const volatile uint8_t *)addr;
}
static inline void mmio_write8(volatile void *addr, uint8_t v)
{
    *(volatile uint8_t *)addr = v;
}
static inline uint16_t mmio_read16(const volatile void *addr)
{
    return *(const volatile uint16_t *)addr;
}
static inline void mmio_write16(volatile void *addr, uint16_t v)
{
    *(volatile uint16_t *)addr = v;
}
static inline uint32_t mmio_read32(const volatile void *addr)
{
    return *(const volatile uint32_t *)addr;
}
static inline void mmio_write32(volatile void *addr, uint32_t v)
{
    *(volatile uint32_t *)addr = v;
}
static inline void mmio_write64(volatile void *addr, uint64_t v)
{

    mmio_write32(addr, (uint32_t)(v & 0xFFFFFFFFu));
    mmio_write32((volatile uint8_t *)addr + 4, (uint32_t)(v >> 32));
}

static inline void compiler_barrier(void)
{
    __asm__ __volatile__("" ::: "memory");
}

static inline volatile void *bar_to_virt(uintptr_t bar_pa, uint32_t offset)
{
    return (volatile void *)(bar_pa + offset);
}

static int virtio_map_caps(pci_device_t *dev)
{
    uint8_t cap_ptr = pci_read_config8(dev, PCI_CFG_CAP_PTR) & 0xFCu;

    while (cap_ptr != 0) {
        uint8_t cap_id   = pci_read_config8(dev, cap_ptr);
        uint8_t cap_next = pci_read_config8(dev, cap_ptr + 1u);

        if (cap_id != VIRTIO_PCI_CAP_VENDOR_ID) {
            cap_ptr = cap_next & 0xFCu;
            continue;
        }

        uint8_t  cfg_type = pci_read_config8(dev,  cap_ptr + 3u);
        uint8_t  bar_idx  = pci_read_config8(dev,  cap_ptr + 4u);
        uint32_t offset   = pci_read_config32(dev, cap_ptr + 8u);
        uint32_t length   = pci_read_config32(dev, cap_ptr + 12u);

        if (cfg_type == VIRTIO_PCI_CAP_PCI_CFG) {
            cap_ptr = cap_next & 0xFCu;
            continue;
        }

        uintptr_t bar_base = pci_bar_base(dev, bar_idx);
        if (bar_base == 0) {
            klog_warn("virtio", "capability references unmapped BAR");
            cap_ptr = cap_next & 0xFCu;
            continue;
        }

        volatile void *region = bar_to_virt(bar_base, offset);
        (void)length;

        switch (cfg_type) {
        case VIRTIO_PCI_CAP_COMMON_CFG:
            g_common = (volatile virtio_common_cfg_t *)region;
            break;
        case VIRTIO_PCI_CAP_NOTIFY_CFG:
            g_notify_base = (volatile uint8_t *)region;
            g_notify_mult = pci_read_config32(dev, cap_ptr + 16u);
            break;
        case VIRTIO_PCI_CAP_ISR_CFG:
            g_isr = (volatile uint8_t *)region;
            break;
        case VIRTIO_PCI_CAP_DEVICE_CFG:
            g_net_cfg = (volatile virtio_net_cfg_t *)region;
            break;
        default:
            break;
        }

        cap_ptr = cap_next & 0xFCu;
    }

    if (!g_common) {
        klog_fail("virtio", "VIRTIO_PCI_CAP_COMMON_CFG not found");
        return -1;
    }
    if (!g_notify_base) {
        klog_fail("virtio", "VIRTIO_PCI_CAP_NOTIFY_CFG not found");
        return -1;
    }
    if (!g_net_cfg) {
        klog_fail("virtio", "VIRTIO_PCI_CAP_DEVICE_CFG not found");
        return -1;
    }
    return 0;
}

static int virtio_negotiate_features(void)
{

    mmio_write32(&g_common->device_feature_select, 0);
    compiler_barrier();
    uint32_t dev_feat_lo = mmio_read32(&g_common->device_feature);

    mmio_write32(&g_common->device_feature_select, 1);
    compiler_barrier();
    uint32_t dev_feat_hi = mmio_read32(&g_common->device_feature);

    uint32_t neg_lo = dev_feat_lo & DRIVER_FEATURES_LO;
    uint32_t neg_hi = dev_feat_hi & DRIVER_FEATURES_HI;

    if (!(dev_feat_hi & (uint32_t)(VIRTIO_F_VERSION_1 >> 32))) {
        klog_fail("virtio", "device does not offer VIRTIO_F_VERSION_1");
        return -1;
    }

    mmio_write32(&g_common->driver_feature_select, 0);
    compiler_barrier();
    mmio_write32(&g_common->driver_feature, neg_lo);

    mmio_write32(&g_common->driver_feature_select, 1);
    compiler_barrier();
    mmio_write32(&g_common->driver_feature, neg_hi);

    uint8_t status = mmio_read8(&g_common->device_status);
    mmio_write8(&g_common->device_status,
                (uint8_t)(status | VIRTIO_STATUS_FEATURES_OK));
    compiler_barrier();

    status = mmio_read8(&g_common->device_status);
    if (!(status & VIRTIO_STATUS_FEATURES_OK)) {
        klog_fail("virtio", "device rejected negotiated feature set");
        return -1;
    }

    return 0;
}

static int virtq_init(virtqueue_t *vq, uint16_t queue_idx,
                      void *desc_mem, void *avail_mem, void *used_mem)
{

    mmio_write16(&g_common->queue_select, queue_idx);
    compiler_barrier();

    uint16_t max_size = mmio_read16(&g_common->queue_size);
    if (max_size == 0) {
        klog_fail("virtio", "queue_size is 0 -- queue not available");
        return -1;
    }

    uint16_t chosen_size = (VIRTQ_SIZE < max_size) ? VIRTQ_SIZE : max_size;
    mmio_write16(&g_common->queue_size, chosen_size);
    compiler_barrier();

    for (size_t i = 0; i < VIRTQ_DESC_TABLE_BYTES; i++)
        ((uint8_t *)desc_mem)[i] = 0;
    for (size_t i = 0; i < VIRTQ_AVAIL_BYTES + 2; i++)
        ((uint8_t *)avail_mem)[i] = 0;
    for (size_t i = 0; i < VIRTQ_USED_BYTES + 2; i++)
        ((uint8_t *)used_mem)[i] = 0;

    vq->desc        = (volatile virtq_desc_t *)desc_mem;
    vq->avail       = (volatile virtq_avail_t *)avail_mem;
    vq->used        = (volatile virtq_used_t  *)used_mem;
    vq->queue_size  = chosen_size;
    vq->last_used_idx = 0;
    vq->free_head   = 0;
    vq->num_free    = chosen_size;

    for (uint16_t i = 0; i < chosen_size - 1u; i++) {
        vq->desc[i].flags = VIRTQ_DESC_F_NEXT;
        vq->desc[i].next  = (uint16_t)(i + 1u);
    }
    vq->desc[chosen_size - 1u].flags = 0;
    vq->desc[chosen_size - 1u].next  = 0;

    uint16_t notify_off = mmio_read16(&g_common->queue_notify_off);
    vq->notify = (volatile uint16_t *)
                 (g_notify_base + (uintptr_t)notify_off * g_notify_mult);

    mmio_write64(&g_common->queue_desc,   (uint64_t)(uintptr_t)desc_mem);
    mmio_write64(&g_common->queue_driver, (uint64_t)(uintptr_t)avail_mem);
    mmio_write64(&g_common->queue_device, (uint64_t)(uintptr_t)used_mem);
    compiler_barrier();

    mmio_write16(&g_common->queue_enable, 1);
    compiler_barrier();

    return 0;
}

static void rxq_populate(void)
{
    virtqueue_t *vq = &g_vq[VQ_RX];

    for (uint16_t i = 0; i < vq->queue_size; i++) {
        vq->desc[i].addr  = (uint64_t)(uintptr_t)g_rx_bufs[i];
        vq->desc[i].len   = RX_BUF_SIZE;
        vq->desc[i].flags = VIRTQ_DESC_F_WRITE;
        vq->desc[i].next  = 0;

        vq->avail->ring[vq->avail->idx % vq->queue_size] = i;
        compiler_barrier();
        vq->avail->idx++;
    }
    vq->num_free = 0;

    compiler_barrier();
    mmio_write16(vq->notify, VQ_RX);
}

int virtio_net_init(void)
{

    pci_device_t *dev = pci_find_device(VIRTIO_VENDOR_ID,
                                        VIRTIO_NET_DEVICE_MODERN);
    if (!dev) {
        dev = pci_find_device(VIRTIO_VENDOR_ID, VIRTIO_NET_DEVICE_LEGACY);
        if (!dev) {
            klog_info("virtio", "no VirtIO-Net device found");
            return -1;
        }
    }

pci_enable_device(dev);

    klog_info("virtio", "VirtIO-Net PCI device located");

    uint16_t cmd = pci_read_config16(dev, PCI_CFG_COMMAND);
    pci_write_config16(dev, PCI_CFG_COMMAND,
                       (uint16_t)(cmd | PCI_CMD_BUS_MASTER | PCI_CMD_MEM_SPACE));

    if (virtio_map_caps(dev) != 0)
        return -1;

    mmio_write8(&g_common->device_status, 0);
    compiler_barrier();
    for (int retries = 1000; retries > 0; retries--) {
        if (mmio_read8(&g_common->device_status) == 0)
            break;
    }
    if (mmio_read8(&g_common->device_status) != 0) {
        klog_fail("virtio", "device did not complete reset");
        return -1;
    }

    mmio_write8(&g_common->device_status, VIRTIO_STATUS_ACKNOWLEDGE);
    compiler_barrier();

    mmio_write8(&g_common->device_status,
                VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);
    compiler_barrier();

    if (virtio_negotiate_features() != 0) {
        mmio_write8(&g_common->device_status, VIRTIO_STATUS_FAILED);
        return -1;
    }

    klog_ok("virtio", "feature negotiation complete (MAC + STATUS + v1.0)");

    for (int i = 0; i < 6; i++)
        g_mac[i] = mmio_read8(&g_net_cfg->mac[i]);

    char mac_str[32];
    ksnprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
              g_mac[0], g_mac[1], g_mac[2],
              g_mac[3], g_mac[4], g_mac[5]);
    klog_ok("virtio", mac_str);

    if (virtq_init(&g_vq[VQ_RX], VQ_RX,
                   rxq_desc, rxq_avail_raw, rxq_used_raw) != 0) {
        klog_fail("virtio", "RX queue init failed");
        mmio_write8(&g_common->device_status, VIRTIO_STATUS_FAILED);
        return -1;
    }
    klog_ok("virtio", "RX virtqueue initialised");

    if (virtq_init(&g_vq[VQ_TX], VQ_TX,
                   txq_desc, txq_avail_raw, txq_used_raw) != 0) {
        klog_fail("virtio", "TX queue init failed");
        mmio_write8(&g_common->device_status, VIRTIO_STATUS_FAILED);
        return -1;
    }
    klog_ok("virtio", "TX virtqueue initialised");

    rxq_populate();

    uint8_t final_status =
        VIRTIO_STATUS_ACKNOWLEDGE |
        VIRTIO_STATUS_DRIVER      |
        VIRTIO_STATUS_FEATURES_OK |
        VIRTIO_STATUS_DRIVER_OK;
    mmio_write8(&g_common->device_status, final_status);
    compiler_barrier();

    uint8_t check = mmio_read8(&g_common->device_status);
    if (check & (VIRTIO_STATUS_NEEDS_RESET | VIRTIO_STATUS_FAILED)) {
        klog_fail("virtio", "device entered error state after DRIVER_OK");
        return -1;
    }

    uint16_t link = mmio_read16(&g_net_cfg->status);
    if (link & VIRTIO_NET_S_LINK_UP)
        klog_ok("virtio", "network link is UP");
    else
        klog_warn("virtio", "network link is DOWN");

    g_initialised = 1;
    klog_ok("virtio", "VirtIO-Net initialised");
    return 0;
}

int virtio_net_send(const void *buf, uint16_t len)
{
    if (!g_initialised)
        return -1;

    virtqueue_t *vq = &g_vq[VQ_TX];

    if (vq->num_free < 2)
        return -1;

    uint16_t hdr_idx  = vq->free_head;
    vq->free_head     = vq->desc[hdr_idx].next;
    vq->num_free--;

    uint16_t data_idx = vq->free_head;
    vq->free_head     = vq->desc[data_idx].next;
    vq->num_free--;

    virtio_net_hdr_t *hdr = &g_tx_hdrs[hdr_idx];
    for (size_t i = 0; i < sizeof(*hdr); i++)
        ((uint8_t *)hdr)[i] = 0;
    hdr->flags    = 0;
    hdr->gso_type = VIRTIO_NET_GSO_NONE;

    vq->desc[hdr_idx].addr  = (uint64_t)(uintptr_t)hdr;
    vq->desc[hdr_idx].len   = sizeof(virtio_net_hdr_t);
    vq->desc[hdr_idx].flags = VIRTQ_DESC_F_NEXT;
    vq->desc[hdr_idx].next  = data_idx;

    if (len > TX_BUF_SIZE)
        len = (uint16_t)TX_BUF_SIZE;
    for (uint16_t _i = 0; _i < len; _i++)
        g_tx_payload_bufs[data_idx][_i] = ((const uint8_t *)buf)[_i];

    vq->desc[data_idx].addr  = (uint64_t)(uintptr_t)g_tx_payload_bufs[data_idx];
    vq->desc[data_idx].len   = len;
    vq->desc[data_idx].flags = 0;
    vq->desc[data_idx].next  = 0;

    uint16_t avail_slot = vq->avail->idx % vq->queue_size;
    vq->avail->ring[avail_slot] = hdr_idx;
    compiler_barrier();
    vq->avail->idx++;
    compiler_barrier();

    mmio_write16(vq->notify, VQ_TX);

    return 0;
}

void virtio_net_poll(void (*rx_cb)(const void *buf, uint16_t len))
{
    if (!g_initialised)
        return;

    {
        virtqueue_t *vq = &g_vq[VQ_RX];
        while (vq->last_used_idx != vq->used->idx) {
            uint16_t         slot = vq->last_used_idx % vq->queue_size;
            virtq_used_elem_t el  = vq->used->ring[slot];
            vq->last_used_idx++;

            uint16_t desc_idx = (uint16_t)el.id;
            uint32_t rx_len   = el.len;

            if (rx_len > sizeof(virtio_net_hdr_t) && rx_cb) {

                const uint8_t *frame =
                    (const uint8_t *)g_rx_bufs[desc_idx]
                    + sizeof(virtio_net_hdr_t);
                uint16_t frame_len =
                    (uint16_t)(rx_len - sizeof(virtio_net_hdr_t));
                rx_cb(frame, frame_len);
            }

            vq->desc[desc_idx].addr  = (uint64_t)(uintptr_t)g_rx_bufs[desc_idx];
            vq->desc[desc_idx].len   = RX_BUF_SIZE;
            vq->desc[desc_idx].flags = VIRTQ_DESC_F_WRITE;
            vq->desc[desc_idx].next  = 0;

            uint16_t avail_slot = vq->avail->idx % vq->queue_size;
            vq->avail->ring[avail_slot] = desc_idx;
            compiler_barrier();
            vq->avail->idx++;
        }
        compiler_barrier();
        mmio_write16(vq->notify, VQ_RX);
    }

    {
        virtqueue_t *vq = &g_vq[VQ_TX];
        while (vq->last_used_idx != vq->used->idx) {
            uint16_t          slot     = vq->last_used_idx % vq->queue_size;
            virtq_used_elem_t el       = vq->used->ring[slot];
            vq->last_used_idx++;

            uint16_t idx = (uint16_t)el.id;
            while (1) {
                uint16_t flags = vq->desc[idx].flags;
                uint16_t next  = vq->desc[idx].next;
                vq->desc[idx].next  = vq->free_head;
                vq->desc[idx].flags = VIRTQ_DESC_F_NEXT;
                vq->free_head = idx;
                vq->num_free++;
                if (!(flags & VIRTQ_DESC_F_NEXT))
                    break;
                idx = next;
            }
        }
    }
}

void virtio_net_mac(uint8_t out[6])
{
    for (int i = 0; i < 6; i++)
        out[i] = g_mac[i];
}
