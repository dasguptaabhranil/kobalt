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

#include "../inc/numa.h"
#include "../inc/acpi.h"
#include <stddef.h>
#include <string.h>

numa_node_t g_numa_nodes[NUMA_MAX_NODES];
uint32_t    g_numa_node_count = 0;
uint8_t     g_apic_to_node[256];

#define SRAT_TYPE_CPU    0
#define SRAT_TYPE_MEM    1
#define SRAT_TYPE_X2APIC 2
#define SRAT_FL_ENABLED  (1u << 0)

typedef struct __attribute__((packed)) {
    uint8_t  type; uint8_t len;
    uint8_t  prox_lo;
    uint8_t  apic_id;
    uint32_t flags;
    uint8_t  sapic_eid;
    uint8_t  prox_hi[3];
    uint32_t clock_domain;
} srat_cpu_t;

typedef struct __attribute__((packed)) {
    uint8_t  type; uint8_t len;
    uint32_t prox_domain;
    uint16_t _r1;
    uint32_t base_lo; uint32_t base_hi;
    uint32_t len_lo;  uint32_t len_hi;
    uint32_t _r2;
    uint32_t flags;
    uint64_t _r3;
} srat_mem_t;

typedef struct __attribute__((packed)) {
    uint8_t  type; uint8_t len;
    uint16_t _r1;
    uint32_t prox_domain;
    uint32_t x2apic_id;
    uint32_t flags;
    uint32_t clock_domain;
    uint32_t _r2;
} srat_x2apic_t;

static uint32_t prox_to_node(uint32_t prox)
{
    for (uint32_t i = 0; i < g_numa_node_count; i++)
        if (g_numa_nodes[i].prox_domain == prox)
            return i;
    if (g_numa_node_count >= NUMA_MAX_NODES)
        return 0;
    uint32_t idx = g_numa_node_count++;
    g_numa_nodes[idx].prox_domain = prox;
    g_numa_nodes[idx].mem_base    = 0;
    g_numa_nodes[idx].mem_len     = 0;
    return idx;
}

static void fallback_single_node(void)
{
    g_numa_node_count         = 1;
    g_numa_nodes[0].prox_domain = 0;
    g_numa_nodes[0].mem_base    = 0;
    g_numa_nodes[0].mem_len     = ~(uint64_t)0;
    memset(g_apic_to_node, 0, sizeof(g_apic_to_node));
}

void numa_init(void)
{
    memset(g_apic_to_node, NUMA_NODE_NONE, sizeof(g_apic_to_node));
    memset(g_numa_nodes,   0,              sizeof(g_numa_nodes));

    acpi_sdt_hdr_t *hdr = acpi_find_table("SRAT");
    if (!hdr) {
        fallback_single_node();
        return;
    }

    const uint8_t *p   = (const uint8_t *)hdr + sizeof(acpi_sdt_hdr_t) + 12;
    const uint8_t *end = (const uint8_t *)hdr + hdr->length;

    for (const uint8_t *q = p; q < end; ) {
        if (q[1] < 2 || q + q[1] > end) break;
        if (q[0] == SRAT_TYPE_MEM) {
            const srat_mem_t *m = (const srat_mem_t *)q;
            if (m->flags & SRAT_FL_ENABLED) {
                uint32_t nid  = prox_to_node(m->prox_domain);
                uint64_t base = ((uint64_t)m->base_hi << 32) | m->base_lo;
                uint64_t mlen = ((uint64_t)m->len_hi  << 32) | m->len_lo;
                if (mlen > g_numa_nodes[nid].mem_len) {
                    g_numa_nodes[nid].mem_base = base;
                    g_numa_nodes[nid].mem_len  = mlen;
                }
            }
        }
        q += q[1];
    }

    for (const uint8_t *q = p; q < end; ) {
        if (q[1] < 2 || q + q[1] > end) break;
        if (q[0] == SRAT_TYPE_CPU) {
            const srat_cpu_t *c = (const srat_cpu_t *)q;
            if (c->flags & SRAT_FL_ENABLED) {
                uint32_t prox = ((uint32_t)c->prox_hi[2] << 24) |
                                ((uint32_t)c->prox_hi[1] << 16) |
                                ((uint32_t)c->prox_hi[0] <<  8) |
                                c->prox_lo;
                uint32_t nid = prox_to_node(prox);
                g_apic_to_node[c->apic_id] = (uint8_t)nid;
            }
        } else if (q[0] == SRAT_TYPE_X2APIC) {
            const srat_x2apic_t *c = (const srat_x2apic_t *)q;
            if ((c->flags & SRAT_FL_ENABLED) && c->x2apic_id < 256) {
                uint32_t nid = prox_to_node(c->prox_domain);
                g_apic_to_node[c->x2apic_id] = (uint8_t)nid;
            }
        }
        q += q[1];
    }

    if (!g_numa_node_count)
        fallback_single_node();
}

uint32_t numa_node_count(void)
{
    return g_numa_node_count ? g_numa_node_count : 1;
}

uint32_t numa_node_of_apic(uint8_t apic_id)
{
    uint8_t n = g_apic_to_node[apic_id];
    return (n == NUMA_NODE_NONE) ? 0 : n;
}

uint32_t numa_current_node(void)
{

    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0xC0000101u));
    if (!lo && !hi)
        return 0;
    uint32_t apic_id;
    __asm__ volatile("movl %%gs:%c1, %0" : "=r"(apic_id) : "i"(12));
    return numa_node_of_apic((uint8_t)apic_id);
}
