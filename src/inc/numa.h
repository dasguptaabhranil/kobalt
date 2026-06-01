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

#define NUMA_MAX_NODES  8
#define NUMA_NODE_NONE  0xFF

typedef struct {
    uint32_t prox_domain;
    uint64_t mem_base;
    uint64_t mem_len;
} numa_node_t;

extern numa_node_t g_numa_nodes[NUMA_MAX_NODES];
extern uint32_t    g_numa_node_count;
extern uint8_t     g_apic_to_node[256];

void     numa_init(void);
uint32_t numa_node_count(void);
uint32_t numa_node_of_apic(uint8_t apic_id);
uint32_t numa_current_node(void);
