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

#include <kernel.h>
#include <virtio.h>
#include <lwip/netif.h>
#include <lwip/pbuf.h>
#include <lwip/etharp.h>
#include <lwip/timeouts.h>
#include <netif/ethernet.h>
#include <string.h>

static struct netif *g_virtio_netif = NULL;

static err_t virtio_netif_output(struct netif *netif, struct pbuf *p)
{
    (void)netif;

    static uint8_t tx_linear[1536];
    uint16_t total = pbuf_copy_partial(p, tx_linear, (u16_t)p->tot_len, 0);
    if (virtio_net_send(tx_linear, total) != 0)
        return ERR_IF;
    return ERR_OK;
}

static void virtio_rx_callback(const void *buf, uint16_t len)
{
    if (!g_virtio_netif) return;

    struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (p) {
        memcpy(p->payload, buf, len);
        if (g_virtio_netif->input(p, g_virtio_netif) != ERR_OK)
            pbuf_free(p);
    }
}

err_t virtio_netif_init(struct netif *netif)
{
    netif->name[0] = 'e';
    netif->name[1] = 'n';

    netif->output     = etharp_output;
    netif->linkoutput = virtio_netif_output;

    virtio_net_mac(netif->hwaddr);
    netif->hwaddr_len = 6;
    netif->mtu        = 1500;

    netif->flags = NETIF_FLAG_BROADCAST
                 | NETIF_FLAG_ETHARP
                 | NETIF_FLAG_ETHERNET
                 | NETIF_FLAG_LINK_UP;

    g_virtio_netif = netif;
    return ERR_OK;
}

void net_poll(void)
{
    virtio_net_poll(virtio_rx_callback);
    sys_check_timeouts();
}
