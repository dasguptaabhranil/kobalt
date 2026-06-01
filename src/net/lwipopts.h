/* Copyright (C) 2026  Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef KOBALT_LWIPOPTS_H
#define KOBALT_LWIPOPTS_H

/* -------------------------------------------------------------------------
 * Threading model: bare-metal, no OS, no locks.
 * ------------------------------------------------------------------------- */
#define NO_SYS                      0   /* netconn/socket API requires OS shim */
#define SYS_LIGHTWEIGHT_PROT        0
#define SYS_MBOX_SIZE               32  /* ring buffer depth in sys_mbox_t     */

/* API surface: raw PCB + netconn + BSD socket layer.
 * LWIP_SOCKET requires LWIP_NETCONN and NO_SYS=0.
 * ------------------------------------------------------------------------- */
#define LWIP_RAW                    1
#define LWIP_NETCONN                1
#define LWIP_SOCKET                 1
#define LWIP_COMPAT_SOCKETS         0   /* use lwip_* prefixed names only    */
#define LWIP_POSIX_SOCKETS_IO_NAMES 0   /* don't shadow read/write/close     */

/* -------------------------------------------------------------------------
 * Protocol stack features.
 * ------------------------------------------------------------------------- */
#define LWIP_ETHERNET               1
#define LWIP_ARP                    1
#define LWIP_ICMP                   1
#define LWIP_DHCP                   1
#define LWIP_UDP                    1
#define LWIP_TCP                    1
#define ETH_PAD_SIZE                0
#define LWIP_ALTCP_TLS              0
#define LWIP_ALTCP_TLS_MBEDTLS      0

/* -------------------------------------------------------------------------
 * Netif client data slots.
 *
 * LWIP_NUM_NETIF_CLIENT_DATA is the correct lwIP option.
 * 1 slot is required for DHCP state tracking (dhcp.c uses
 * netif_get_client_data / netif_set_client_data internally).
 * ------------------------------------------------------------------------- */
#define LWIP_NUM_NETIF_CLIENT_DATA  1

/* -------------------------------------------------------------------------
 * Memory configuration.
 *
 * CRITICAL: MEMP_MEM_MALLOC must NOT be set to 1 on Kobalt.
 *
 * When MEMP_MEM_MALLOC=1, all lwIP pool allocations (pbufs, ARP entries,
 * PCBs) are served from the lwIP heap -- a 2 MB static array in .bss.
 * mm_seal() marks .bss RO/NX after boot. Any allocation that happens after
 * the seal (e.g. pbuf_alloc() inside cmd_ping, or writing an ARP cache
 * entry when the gateway ARP reply arrives) writes into a now-read-only
 * page. The write faults silently: the allocation returns NULL, the packet
 * is dropped, the ARP table stays empty, ping always times out.
 *
 * Fix: use lwIP's static memory pools (MEMP_MEM_MALLOC=0, the default).
 * Pool storage is allocated at link time as individual static arrays, each
 * sized for exactly their pool count. These arrays live in .bss but are
 * managed by the pool allocator which keeps its own free-list in the same
 * memory -- so they must remain writable. mm_seal() must NOT seal the pool
 * region, OR (simpler) we keep MEMP_MEM_MALLOC=0 and size the pools here.
 * ------------------------------------------------------------------------- */

/* Heap for non-pool allocations (TCP buffers, etc.) */
#define MEM_ALIGNMENT               8
#define MEM_SIZE                    (512 * 1024)    /* 512 KB */

/* pbuf pool: 24 slots × 1536 bytes covers burst RX + ping + ARP in flight */
#define PBUF_POOL_SIZE              24
#define PBUF_POOL_BUFSIZE           1536

/* ARP table: 8 entries is enough for a single-NIC kernel */
#define ARP_TABLE_SIZE              8
#define ARP_MAXAGE                  300             /* seconds */

/* DHCP: 1 concurrent client */
#define DHCP_TABLE_SIZE             1

/* TCP/UDP PCB counts */
#define MEMP_NUM_TCP_PCB            4
#define MEMP_NUM_UDP_PCB            4
#define MEMP_NUM_RAW_PCB            4
#define MEMP_NUM_NETBUF             8
#define MEMP_NUM_NETCONN            8

#endif /* KOBALT_LWIPOPTS_H */