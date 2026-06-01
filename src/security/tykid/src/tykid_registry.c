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

#include "tykid_internal.h"

static TYKID_ALWAYS_INL s32 ty_hexdigit(u8 c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static u64 ty_parse_hex(const char *s) {
    u64 v = 0;
    while (*s) {
        s32 d = ty_hexdigit((u8)*s);
        if (d < 0) break;
        v = (v << 4) | (u64)d;
        s++;
    } return v;
}

static u32 ty_parse_dec(const char *s) {
    u32 v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s++ - '0'); }
    return v;
}

static const char *ty_skip_ws(const char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r') s++;
    return s;
}

static const char *ty_skip_token(const char *s) {
    while (*s && *s != ' ' && *s != '\t' && *s != '\r' && *s != '\n') s++;
    return ty_skip_ws(s);
}

static bool8 ty_key_eq(const char *line, const char *key) {
    while (*key) { if (*line++ != *key++) return TYKID_FALSE; }
    return (*line == ' ' || *line == '\t') ? TYKID_TRUE : TYKID_FALSE;
}

static void ty_strip_nl(char *s) {
    usz n = ty_strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' '))
        s[--n] = '\0';
}

static bool8 ty_parse_hmac(const char *hex, u8 out[TYKID_HMAC_DIGEST_BYTES]) {
    for (u32 i = 0; i < TYKID_HMAC_DIGEST_BYTES; i++) {
        s32 hi = ty_hexdigit((u8)hex[i*2]);
        s32 lo = ty_hexdigit((u8)hex[i*2+1]);
        if (hi < 0 || lo < 0) return TYKID_FALSE;
        out[i] = (u8)((hi << 4) | lo);
    } return TYKID_TRUE;
}

TYKID_INTERNAL tykid_status_t
ty_registry_parse_manifest(tykid_gate_ctx_t *ctx,
                            const char *manifest_path,
                            tykid_driver_desc_t *out)
{

    (void)ctx;
    (void)manifest_path;
    (void)out;

}

static tykid_status_t
ty_parse_manifest_buf(tykid_gate_ctx_t *ctx,
                      const char *buf,
                      tykid_driver_desc_t *drv)
{
    ty_memzero_secure(drv, sizeof(*drv));
    drv->state    = TYKID_DRV_STATE_ABSENT;
 drv->priority = 128;

    char dep_names[TYKID_MAX_DEPS][TYKID_MAX_NAME];
    u8   dep_name_count = 0;

    const char *p  = buf;
    char line[TYKID_MAX_PATH + 64];
    u32  lineno = 0;

    while (*p) {

        usz li = 0;
        while (*p && *p != '\n' && li < sizeof(line) - 1)
            line[li++] = *p++;
        if (*p == '\n') p++;
        line[li] = '\0';
        ty_strip_nl(line);
        lineno++;

        const char *L = ty_skip_ws(line);
 if (!*L || *L == '#') continue;

        if (ty_key_eq(L, "NAME")) {
            const char *v = ty_skip_token(L);
            ty_strncpy(drv->name, v, TYKID_MAX_NAME);

        } else if (ty_key_eq(L, "PATH")) {
            const char *v = ty_skip_token(L);
            ty_strncpy(drv->path, v, TYKID_MAX_PATH);

        } else if (ty_key_eq(L, "CLASS")) {
            if (drv->hw_class_count < 8) {
                const char *v = ty_skip_token(L);
                drv->hw_classes[drv->hw_class_count++] = (tykid_hwclass_t)ty_parse_hex(v);
            }

        } else if (ty_key_eq(L, "VENDOR")) {
            const char *v = ty_skip_token(L);
            drv->vendor_mask = (u32)ty_parse_hex(v);

        } else if (ty_key_eq(L, "DEVICE")) {
            const char *v = ty_skip_token(L);
            drv->device_mask = (u32)ty_parse_hex(v);

        } else if (ty_key_eq(L, "VERSION")) {
            const char *v = ty_skip_token(L);
            drv->version_major = (u16)ty_parse_dec(v);
            while (*v && *v != '.') v++;
            if (*v == '.') v++;
            drv->version_minor = (u16)ty_parse_dec(v);

        } else if (ty_key_eq(L, "PRIORITY")) {
            const char *v = ty_skip_token(L);
            drv->priority = (u8)ty_parse_dec(v);

        } else if (ty_key_eq(L, "FLAGS")) {
            const char *v = ty_skip_token(L);
            drv->flags = (u32)ty_parse_hex(v);

        } else if (ty_key_eq(L, "DEPENDS")) {
            if (dep_name_count < TYKID_MAX_DEPS) {
                const char *v = ty_skip_token(L);
                ty_strncpy(dep_names[dep_name_count++], v, TYKID_MAX_NAME);
            }

        } else if (ty_key_eq(L, "HMAC")) {
            const char *v = ty_skip_token(L);
            if (!ty_parse_hmac(v, drv->hmac)) {
                TY_LOG(ctx, TY_LOG_WARN, "Manifest line %u: bad HMAC hex", lineno);
            }
        }
    }

    if (!drv->name[0] || !drv->path[0]) {
        TY_LOG(ctx, TY_LOG_ERROR, "Manifest missing NAME or PATH");
        return TYKID_ERR_PATH;
    }

    drv->dep_count = dep_name_count;

    for (u8 i = 0; i < dep_name_count; i++) {
        drv->deps[i] = 0xFF;

 (void)dep_names[i];
    }

    return TYKID_OK;
}

static tykid_status_t
ty_registry_resolve_deps(tykid_gate_ctx_t *ctx,
                          ty_driver_registry_t *reg,
                          const char dep_name_table[][TYKID_MAX_NAME],
                          u8 dep_name_counts[])
{
    for (u32 i = 0; i < reg->count; i++) {
        tykid_driver_desc_t *drv = &reg->entries[i];
        for (u8 j = 0; j < drv->dep_count; j++) {

            const char *dep_name = dep_name_table[i * TYKID_MAX_DEPS + j];
            bool8 found = TYKID_FALSE;
            for (u32 k = 0; k < reg->count; k++) {
                if (ty_strncmp(reg->entries[k].name, dep_name, TYKID_MAX_NAME) == 0) {
                    drv->deps[j] = (u8)k;
                    found = TYKID_TRUE;
                    break;
                }
            }
            if (!found) {
                TY_LOG(ctx, TY_LOG_WARN,
                    "Driver '%s' depends on '%s' which was not found in registry",
                    drv->name, dep_name);
 drv->deps[j] = 0xFF;
            }
        }
    }
    (void)dep_name_counts;
    return TYKID_OK;
}

TYKID_INTERNAL tykid_status_t
ty_dep_graph_add(ty_dep_graph_t *g, u8 u, u8 v)
{

    if (u >= g->node_count || v >= g->node_count)
        return TYKID_ERR_GENERIC;
    u32 word = v >> 6, bit = v & 63;
    if (!(g->adj[u][word] & (1ULL << bit))) {
        g->adj[u][word] |= (1ULL << bit);
        g->in_degree[u]++;
    }
    return TYKID_OK;
}

TYKID_INTERNAL tykid_status_t
ty_dep_graph_toposort(ty_dep_graph_t *g)
{

    u8 queue[TYKID_MAX_DRIVERS];
    u8 qhead = 0, qtail = 0;
    u8 in_deg[TYKID_MAX_DRIVERS];

    ty_memcpy(in_deg, g->in_degree, g->node_count);

    for (u8 i = 0; i < g->node_count; i++) {
        if (in_deg[i] == 0) queue[qtail++] = i;
    }

    g->topo_count = 0;

    while (qhead < qtail) {
        u8 u = queue[qhead++];
        g->topo[g->topo_count++] = u;

        for (u8 v = 0; v < g->node_count; v++) {
            u32 word = u >> 6, bit = u & 63;

            if (g->adj[v][word] & (1ULL << bit)) {
                if (--in_deg[v] == 0)
                    queue[qtail++] = v;
            }
        }
    }

    if (g->topo_count != g->node_count)
        return TYKID_ERR_CYCLE;

    for (u8 i = 0; i < g->topo_count / 2; i++) {
        u8 tmp = g->topo[i];
        g->topo[i] = g->topo[g->topo_count - 1 - i];
        g->topo[g->topo_count - 1 - i] = tmp;
    }
    return TYKID_OK;
}

typedef struct {
    tykid_gate_ctx_t    *ctx;
    ty_driver_registry_t *reg;

    char                (*dep_names)[TYKID_MAX_DEPS][TYKID_MAX_NAME];
    u8                    dep_counts[TYKID_MAX_DRIVERS];
    tykid_status_t        err;
} ty_scan_ctx_t;

extern tykid_status_t kobalt_vfs_read_text(const char *path,
                                             char *buf, usz buf_sz,
                                             usz *out_len);

static bool8
ty_scan_callback(const char *entry_name, void *priv)
{
    ty_scan_ctx_t       *sc  = (ty_scan_ctx_t *)priv;
    tykid_gate_ctx_t    *ctx = sc->ctx;
    ty_driver_registry_t *reg = sc->reg;

    usz nlen = ty_strlen(entry_name);
    if (nlen < 16) return TYKID_TRUE;
    static const char SUFFIX[] = ".tykid-manifest";
    usz slen = sizeof(SUFFIX) - 1;
    if (ty_strncmp(entry_name + nlen - slen, SUFFIX, slen) != 0)
        return TYKID_TRUE;

    if (reg->count >= TYKID_MAX_DRIVERS) {
        TY_LOG(ctx, TY_LOG_WARN, "Driver table full, skipping '%s'", entry_name);
        return TYKID_FALSE;
    }

    char full_path[TYKID_MAX_PATH];
    const char *dir = ctx->cfg.drivers_dir ? ctx->cfg.drivers_dir : "../drivers";
    usz dlen = ty_strlen(dir);
    ty_strncpy(full_path, dir, TYKID_MAX_PATH);
    if (dlen < TYKID_MAX_PATH - 1 && dir[dlen-1] != '/') {
        full_path[dlen++] = '/';
        full_path[dlen]   = '\0';
    }
    ty_strncpy(full_path + dlen, entry_name, TYKID_MAX_PATH - dlen);

    char manifest_buf[4096];
    usz  manifest_len = 0;
    tykid_status_t st = kobalt_vfs_read_text(full_path, manifest_buf,
                                               sizeof(manifest_buf) - 1,
                                               &manifest_len);
    if (st != TYKID_OK) {
        TY_LOG(ctx, TY_LOG_WARN, "Could not read manifest '%s': %d", full_path, st);
        return TYKID_TRUE;
    }
    manifest_buf[manifest_len] = '\0';

    tykid_driver_desc_t tmp;
    st = ty_parse_manifest_buf(ctx, manifest_buf, &tmp);
    if (st != TYKID_OK) {
        TY_LOG(ctx, TY_LOG_WARN, "Failed to parse manifest '%s': %d", full_path, st);
        return TYKID_TRUE;
    }

    {
        const char *p = manifest_buf;
        char line[TYKID_MAX_PATH + 64];
        u8 dc = 0;
        while (*p && dc < TYKID_MAX_DEPS) {
            usz li = 0;
            while (*p && *p != '\n' && li < sizeof(line)-1) line[li++] = *p++;
            if (*p == '\n') p++;
            line[li] = '\0';
            const char *L = ty_skip_ws(line);
            if (ty_key_eq(L, "DEPENDS")) {
                const char *v = ty_skip_token(L);
                ty_strncpy(sc->dep_names[reg->count][dc++], v, TYKID_MAX_NAME);
            }
        }
        sc->dep_counts[reg->count] = dc;
    }

    reg->entries[reg->count] = tmp;
    TY_LOG(ctx, TY_LOG_DEBUG, "Registry: loaded manifest for driver '%s'", tmp.name);
    reg->count++;

 return TYKID_TRUE;
}

TYKID_INTERNAL tykid_status_t
ty_registry_scan(tykid_gate_ctx_t *ctx)
{
    ty_driver_registry_t *reg = &ctx->reg;
    ty_memzero_secure(reg, sizeof(*reg));

    if (__ty_unlikely(!ctx->cfg.path_iter_fn)) {
        TY_LOG(ctx, TY_LOG_ERROR, "path_iter_fn is NULL — cannot scan drivers");
        return TYKID_ERR_GENERIC;
    }

    typedef char dep_entry_t[TYKID_MAX_DEPS][TYKID_MAX_NAME];
    dep_entry_t *dep_names = (dep_entry_t *)TY_ALLOC(ctx,
                                sizeof(dep_entry_t) * TYKID_MAX_DRIVERS);
    if (!dep_names) {
        TY_LOG(ctx, TY_LOG_ERROR, "Failed to allocate dep_names table");
        return TYKID_ERR_ALLOC;
    }
    ty_memzero_secure(dep_names, sizeof(dep_entry_t) * TYKID_MAX_DRIVERS);

    ty_scan_ctx_t sc;
    ty_memzero_secure(&sc, sizeof(sc));
    sc.ctx       = ctx;
    sc.reg       = reg;
    sc.dep_names = dep_names;

    const char *dir = ctx->cfg.drivers_dir ? ctx->cfg.drivers_dir : "../drivers";
    tykid_status_t st = ctx->cfg.path_iter_fn(dir, ty_scan_callback, &sc);
    if (st != TYKID_OK) {
        TY_LOG(ctx, TY_LOG_ERROR, "path_iter_fn failed for '%s': %d", dir, st);
        TY_FREE(ctx, dep_names, sizeof(dep_entry_t) * TYKID_MAX_DRIVERS);
        return TYKID_ERR_PATH;
    }

    TY_LOG(ctx, TY_LOG_INFO, "Registry scan complete: %u drivers found in '%s'",
           reg->count, dir);

    reg->dir_hash = ty_siphash24(ctx->session_key,
                                  reg->entries,
                                  reg->count * sizeof(tykid_driver_desc_t));
    reg->scan_timestamp = ty_entropy_u64(ctx);

    if (reg->count == 0) {
        TY_FREE(ctx, dep_names, sizeof(dep_entry_t) * TYKID_MAX_DRIVERS);
 return TYKID_OK;
    }

    ty_dep_graph_t *g = &reg->deps;
    g->node_count = (u8)reg->count;

    ty_registry_resolve_deps(ctx, reg,
                              (const char (*)[TYKID_MAX_NAME])sc.dep_names,
                              sc.dep_counts);

    for (u32 i = 0; i < reg->count; i++) {
        tykid_driver_desc_t *drv = &reg->entries[i];
        for (u8 j = 0; j < drv->dep_count; j++) {
            u8 dep_idx = drv->deps[j];
            if (dep_idx == 0xFF || dep_idx >= reg->count) continue;
            st = ty_dep_graph_add(g, (u8)i, dep_idx);
            if (st != TYKID_OK) {
                TY_FREE(ctx, dep_names, sizeof(dep_entry_t) * TYKID_MAX_DRIVERS);
                return st;
            }
        }
    }

    st = ty_dep_graph_toposort(g);
    TY_FREE(ctx, dep_names, sizeof(dep_entry_t) * TYKID_MAX_DRIVERS);
    if (st == TYKID_ERR_CYCLE) {
        TY_LOG(ctx, TY_LOG_ERROR, "Dependency cycle detected in driver registry");
        return TYKID_ERR_CYCLE;
    }

    return TYKID_OK;
}
