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

#include "flatfs_internal.h"
#include "flatfs_inode_cache.h"
#include "flatfs_inode.h"

typedef struct {
    uint64_t        ino;
    uint64_t        stamp;
    int             valid;
    int             dirty;
    flatfs_inode_t  in;
} icache_entry_t;

static icache_entry_t  cache[FLATFS_ICACHE_SIZE];
static uint64_t        clock_val;
static uint64_t        stat_hits, stat_misses;

void flatfs_icache_init(void)
{
    FMEMSET(cache, 0, sizeof(cache));
    clock_val = 0;
    stat_hits = stat_misses = 0;
}

static icache_entry_t *find_slot(uint64_t ino)
{

    uint32_t h = (uint32_t)(ino % FLATFS_ICACHE_SIZE);
    for (uint32_t i = 0; i < FLATFS_ICACHE_SIZE; i++) {
        uint32_t idx = (h + i) % FLATFS_ICACHE_SIZE;
        if (cache[idx].valid && cache[idx].ino == ino)
            return &cache[idx];
    }
    return NULL;
}

static icache_entry_t *evict_lru(void)
{
    uint64_t min_stamp = UINT64_MAX;
    icache_entry_t *victim = &cache[0];
    for (uint32_t i = 0; i < FLATFS_ICACHE_SIZE; i++) {
        if (!cache[i].valid) return &cache[i];
        if (cache[i].stamp < min_stamp) {
            min_stamp = cache[i].stamp;
            victim = &cache[i];
        }
    }
    if (victim->dirty)
        flatfs_inode_write(&victim->in);
    return victim;
}

flatfs_inode_t *flatfs_icache_get(uint64_t ino)
{
    icache_entry_t *e = find_slot(ino);
    if (e) {
        e->stamp = ++clock_val;
        stat_hits++;
        return &e->in;
    }
    stat_misses++;
    return NULL;
}

flatfs_inode_t *flatfs_icache_insert(uint64_t ino, const flatfs_inode_t *in)
{

    icache_entry_t *e = find_slot(ino);
    if (!e) {
        e = evict_lru();
        e->valid = 0;
    }
    e->ino   = ino;
    e->stamp = ++clock_val;
    e->dirty = 0;
    e->valid = 1;
    FMEMCPY(&e->in, in, sizeof(flatfs_inode_t));
    return &e->in;
}

void flatfs_icache_invalidate(uint64_t ino)
{
    icache_entry_t *e = find_slot(ino);
    if (e) {
        if (e->dirty) flatfs_inode_write(&e->in);
        e->valid = 0;
    }
}

void flatfs_icache_flush(void)
{
    for (uint32_t i = 0; i < FLATFS_ICACHE_SIZE; i++) {
        if (cache[i].valid && cache[i].dirty) {
            flatfs_inode_write(&cache[i].in);
            cache[i].dirty = 0;
        }
    }
}

void flatfs_icache_stats(uint64_t *hits, uint64_t *misses)
{
    *hits   = stat_hits;
    *misses = stat_misses;
}
