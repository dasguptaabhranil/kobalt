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

#ifndef __KERNEL__
#define __KERNEL__
#endif

#include <kernel.h>
#include "hda.h"

#define HDA_MEMSET(d,c,n)   __builtin_memset((d),(c),(n))

extern uint8_t      hda_codec_count(void);
extern hda_codec_t *hda_codec_ptr(uint8_t i);
extern void         hda_codec_count_inc(void);
extern hda_status_t hda_verb_exec(uint8_t codec, uint8_t nid,
                                   uint32_t verb_id, uint32_t payload,
                                   uint32_t *resp_out);
extern uintptr_t    hda_mmio(void);
extern int          kprintf(const char *fmt, ...);

static inline uint16_t statests_read(void)
{
    return *(volatile uint16_t *)(hda_mmio() + HDA_REG_STATESTS);
}

static hda_status_t hda_get_param(uint8_t codec, uint8_t nid,
                                   uint8_t param, uint32_t *out)
{

    return hda_verb_exec(codec, nid, 0xF00u, param, out);
}

static hda_status_t hda_verb(uint8_t codec, uint8_t nid,
                              uint32_t verb_id, uint32_t payload,
                              uint32_t *out)
{
    return hda_verb_exec(codec, nid, verb_id, payload, out);
}

static void hda_read_conn_list(hda_codec_t *c, hda_widget_t *w)
{
    uint32_t cll;
    if (hda_get_param(c->addr, w->nid, HDA_PARAM_CONN_LIST_LEN, &cll) != HDA_OK)
        return;

    const uint8_t  total   = (uint8_t)(cll & 0x7Fu);
    const uint8_t  long_form = (uint8_t)((cll >> 7) & 1u);
    uint8_t        fetched = 0;

    while (fetched < total && fetched < HDA_MAX_CONNS) {
        uint32_t resp;
        if (hda_verb(c->addr, w->nid, HDA_VERB_GET_CONN_LIST, fetched, &resp) != HDA_OK)
            break;

        if (long_form) {

            for (int j = 0; j < 2 && fetched < total; j++, fetched++) {
                const uint16_t entry = (uint16_t)((resp >> (j * 16)) & 0xFFFFu);
                if (fetched < HDA_MAX_CONNS)
                    w->conn_list[fetched] = (uint8_t)(entry & 0xFFu);
            }
        } else {

            for (int j = 0; j < 4 && fetched < total; j++, fetched++) {
                const uint8_t entry = (uint8_t)((resp >> (j * 8)) & 0xFFu);
                if (fetched < HDA_MAX_CONNS)
                    w->conn_list[fetched] = entry;
            }
        }
    }
    w->conn_count = (fetched < HDA_MAX_CONNS) ? fetched : (uint8_t)(HDA_MAX_CONNS - 1u);
}

static void hda_enum_widget(hda_codec_t *c, uint8_t nid)
{
    hda_widget_t *w = &c->widgets[nid];
    HDA_MEMSET(w, 0, sizeof(*w));
    w->nid   = nid;
    w->valid = 1;

    if (hda_get_param(c->addr, nid, HDA_PARAM_AUDIO_WCAP, &w->wcap) != HDA_OK)
        return;

    w->type = (uint8_t)((w->wcap & HDA_WCAP_TYPE_MASK) >> HDA_WCAP_TYPE_SHIFT);

    if (w->type == HDA_WID_TYPE_AUD_OUT || w->type == HDA_WID_TYPE_AUD_IN ||
        (w->wcap & HDA_WCAP_FORMAT_OVR)) {
        hda_get_param(c->addr, nid, HDA_PARAM_PCM_SIZES,    &w->supported_rates);
        hda_get_param(c->addr, nid, HDA_PARAM_STREAM_FMTS,  &w->supported_fmts);
    }

    if (w->wcap & HDA_WCAP_IN_AMP)
        hda_get_param(c->addr, nid, HDA_PARAM_AMP_CAP_IN, &w->amp_cap_in);
    if (w->wcap & HDA_WCAP_OUT_AMP)
        hda_get_param(c->addr, nid, HDA_PARAM_AMP_CAP_OUT, &w->amp_cap_out);

    if (w->type == HDA_WID_TYPE_PIN) {
        hda_get_param(c->addr, nid, HDA_PARAM_PIN_CAP, &w->pincap);
        hda_verb(c->addr, nid, HDA_VERB_GET_CONFIG_DEFAULT, 0, &w->cfg_default);
    }

    if (w->wcap & HDA_WCAP_CONN_LIST)
        hda_read_conn_list(c, w);

    if (w->wcap & HDA_WCAP_POWER_CTL) {
        uint32_t ps;
        hda_verb(c->addr, nid, HDA_VERB_GET_POWER_STATE, 0, &ps);
        w->power_state = (uint8_t)(ps & 0xFu);
    }

    if (w->conn_count > 1u) {
        uint32_t sel;
        hda_verb(c->addr, nid, HDA_VERB_GET_CONN_SEL, 0, &sel);
        w->sel_conn = (uint8_t)(sel & 0xFFu);
    }
}

static hda_status_t hda_find_afg(hda_codec_t *c)
{

    uint32_t nc;
    if (hda_get_param(c->addr, 0, HDA_PARAM_NODE_COUNT, &nc) != HDA_OK)
        return HDA_ERR_VERB_TIMEOUT;

    const uint8_t fg_start = (uint8_t)((nc >> 16) & 0xFFu);
    const uint8_t fg_count = (uint8_t)(nc & 0xFFu);

    for (uint8_t i = 0; i < fg_count; i++) {
        const uint8_t fg_nid = fg_start + i;
        uint32_t fgt;
        if (hda_get_param(c->addr, fg_nid, HDA_PARAM_FUNC_GROUP_TYPE, &fgt) != HDA_OK)
            continue;

        if ((fgt & 0xFFu) == 0x01u) {
            c->afg_nid = fg_nid;
            return HDA_OK;
        }
    }
    return HDA_ERR_NO_CODECS;
}

static void hda_powerup_afg(hda_codec_t *c)
{

    hda_verb(c->addr, c->afg_nid, HDA_VERB_SET_POWER_STATE,
             HDA_POWER_D0, NULL);

    for (volatile int i = 0; i < 100000; i++) __asm__("nop");

    for (uint8_t n = c->node_start; n < c->node_start + c->node_count; n++) {
        hda_widget_t *w = &c->widgets[n];
        if (w->valid && (w->wcap & HDA_WCAP_POWER_CTL)) {
            hda_verb(c->addr, n, HDA_VERB_SET_POWER_STATE, HDA_POWER_D0, NULL);
            w->power_state = HDA_POWER_D0;
        }
    }
}

static void hda_enable_eapd(hda_codec_t *c)
{
    for (uint8_t n = c->node_start; n < c->node_start + c->node_count; n++) {
        hda_widget_t *w = &c->widgets[n];
        if (!w->valid || w->type != HDA_WID_TYPE_PIN) continue;
        if (!(w->pincap & HDA_PINCAP_EAPD)) continue;

        uint32_t eapd;
        hda_verb(c->addr, n, HDA_VERB_GET_EAPD_BTL, 0, &eapd);
        eapd |= HDA_EAPD_BTL_EAPD;
        hda_verb(c->addr, n, HDA_VERB_SET_EAPD_BTL, eapd & 0xFFu, NULL);
    }
}

static int is_dac(const hda_codec_t *c, uint8_t nid)
{
    if (nid < c->node_start || nid >= c->node_start + c->node_count)
        return 0;
    return (c->widgets[nid].valid &&
            c->widgets[nid].type == HDA_WID_TYPE_AUD_OUT);
}

static int hda_dfs_to_dac(const hda_codec_t *c, uint8_t nid,
                           uint8_t *path, uint8_t *plen, uint8_t depth)
{
    if (depth > 10u) return 0;

    if (is_dac(c, nid)) {
        if (*plen < 16u) path[(*plen)++] = nid;
        return 1;
    }

    const hda_widget_t *w = &c->widgets[nid];
    if (!w->valid) return 0;

    if (w->type != HDA_WID_TYPE_AUD_MIX  &&
        w->type != HDA_WID_TYPE_AUD_SEL  &&
        w->type != HDA_WID_TYPE_PIN)
        return 0;

    for (uint8_t i = 0; i < w->conn_count; i++) {
        uint8_t saved_len = *plen;
        if (hda_dfs_to_dac(c, w->conn_list[i], path, plen, depth + 1u)) {
            if (*plen < 16u) path[(*plen)++] = nid;
            return 1;
        }
        *plen = saved_len;
    }
    return 0;
}

static void reverse_path(uint8_t *path, uint8_t len)
{
    for (uint8_t i = 0; i < len / 2u; i++) {
        uint8_t tmp     = path[i];
        path[i]         = path[len - 1u - i];
        path[len-1u-i]  = tmp;
    }
}

static void hda_route_codec(hda_codec_t *c)
{
    c->path_count = 0;

    for (uint8_t n = c->node_start; n < c->node_start + c->node_count; n++) {
        const hda_widget_t *w = &c->widgets[n];
        if (!w->valid || w->type != HDA_WID_TYPE_PIN) continue;
        if (!(w->pincap & HDA_PINCAP_OUT_CAP)) continue;

        const uint32_t port_conn =
            (w->cfg_default >> HDA_CFG_PORT_CONN_SHIFT) & 0x3u;
        if (port_conn == HDA_CFG_PORT_CONN_NONE) continue;

        uint8_t path[16];
        uint8_t plen = 0;
        path[plen++] = n;

        int found = 0;
        for (uint8_t ci = 0; ci < w->conn_count; ci++) {
            uint8_t sublen = 0;
            uint8_t subpath[16];
            if (hda_dfs_to_dac(c, w->conn_list[ci], subpath, &sublen, 0)) {

                reverse_path(subpath, sublen);
                uint8_t total = sublen;
                if (total < 16u) { subpath[total++] = n; }

                hda_audio_path_t *ap = &c->paths[c->path_count];
                ap->dac_nid  = subpath[0];
                ap->pin_nid  = n;
                ap->path_len = total;
                for (uint8_t k = 0; k < total && k < 16u; k++)
                    ap->path[k] = subpath[k];

                const uint32_t dev =
                    (w->cfg_default >> HDA_CFG_DEF_DEVICE_SHIFT) & 0xFu;
                ap->is_headphone = (dev == HDA_CFG_DEVICE_HP_OUT) ? 1u : 0u;
                ap->is_speaker   = (dev == HDA_CFG_DEVICE_SPEAKER) ? 1u : 0u;
                ap->active       = 0u;

                c->path_count++;
                found = 1;
                break;
            }
        }
        (void)found;
        if (c->path_count >= HDA_MAX_AUDIO_PATHS)
            break;
    }
}

static void hda_activate_path(hda_codec_t *c, hda_audio_path_t *ap,
                               uint8_t stream_tag, uint16_t fmt)
{
    if (!ap || ap->path_len == 0u) return;

    const uint8_t dac = ap->dac_nid;
    uint16_t chan_stream = (uint16_t)(((stream_tag & 0xFu) << 4) | 0x0u);
    hda_verb_exec(c->addr, dac, HDA_VERB_SET_CONV_STREAM, chan_stream, NULL);
    hda_verb_exec(c->addr, dac,
                  HDA_VERB4(c->addr, dac, HDA_VERB4_SET_CONV_FMT, fmt),
                  0, NULL);

    for (uint8_t i = 0; i < ap->path_len; i++) {
        const uint8_t nid = ap->path[i];
        const hda_widget_t *w = &c->widgets[nid];
        if (!w->valid) continue;

        if (w->wcap & HDA_WCAP_OUT_AMP) {
            const uint32_t num_steps = (w->amp_cap_out >> 8) & 0x7Fu;
            const uint8_t  gain_val  = (uint8_t)(num_steps > 0u ? num_steps : 0u);
            const uint16_t amp_payload =
                (uint16_t)(HDA_AMP_SET_OUTPUT | HDA_AMP_SET_LEFT |
                           HDA_AMP_SET_RIGHT | (gain_val & HDA_AMP_SET_GAIN_MASK));
            hda_verb_exec(c->addr, nid,
                          HDA_VERB4(c->addr, nid, HDA_VERB4_SET_AMP_GAIN, amp_payload),
                          0, NULL);
        }
    }

    const hda_widget_t *pw = &c->widgets[ap->pin_nid];
    uint8_t pinctl = HDA_PINCTL_OUT_EN;
    if (pw->pincap & HDA_PINCAP_HP_DRV)
        pinctl |= HDA_PINCTL_HP_EN;
    hda_verb(c->addr, ap->pin_nid, HDA_VERB_SET_PIN_CTRL, pinctl, NULL);

    ap->active = 1u;
}

hda_status_t hda_codec_enumerate(void)
{

    const uint16_t statests = statests_read();
    if (!statests)
        return HDA_ERR_NO_CODECS;

    for (uint8_t addr = 0; addr < 15u && hda_codec_count() < HDA_MAX_CODECS; addr++) {
        if (!(statests & (1u << addr)))
            continue;

        hda_codec_t *c = hda_codec_ptr(hda_codec_count());
        if (!c) break;

        HDA_MEMSET(c, 0, sizeof(*c));
        c->addr  = addr;
        c->valid = 1;

        uint32_t vid;
        if (hda_get_param(addr, 0, HDA_PARAM_VENDOR_ID, &vid) != HDA_OK) {
            c->valid = 0;
            continue;
        }
        c->vendor_id = (uint16_t)(vid >> 16);
        c->device_id = (uint16_t)(vid & 0xFFFFu);

        hda_get_param(addr, 0, HDA_PARAM_REVISION_ID, &c->revision);

        hda_verb(addr, 0, HDA_VERB_GET_SUBSYS_ID, 0, &c->subsys_id);

        kprintf("[HDA] Codec %u: vendor=%04x device=%04x rev=%08x\n",
                addr, c->vendor_id, c->device_id, c->revision);

        if (hda_find_afg(c) != HDA_OK) {
            kprintf("[HDA] Codec %u: no AFG found\n", addr);
            c->valid = 0;
            continue;
        }

        uint32_t nc;
        if (hda_get_param(addr, c->afg_nid, HDA_PARAM_NODE_COUNT, &nc) != HDA_OK) {
            c->valid = 0;
            continue;
        }
        c->node_start = (uint8_t)((nc >> 16) & 0xFFu);
        c->node_count = (uint8_t)(nc & 0xFFu);

        kprintf("[HDA] Codec %u: AFG=%u, widgets %u..%u\n",
                addr, c->afg_nid,
                c->node_start, c->node_start + c->node_count - 1u);

        hda_powerup_afg(c);

        const uint8_t last = c->node_start + c->node_count;
        for (uint8_t n = c->node_start; n < last; n++)
            hda_enum_widget(c, n);

        hda_enable_eapd(c);

        hda_codec_count_inc();
    }

    return (hda_codec_count() > 0u) ? HDA_OK : HDA_ERR_NO_CODECS;
}

void hda_codec_route_default_paths(void)
{
    for (uint8_t ci = 0; ci < hda_codec_count(); ci++) {
        hda_codec_t *c = hda_codec_ptr(ci);
        if (!c || !c->valid) continue;

        hda_route_codec(c);

        kprintf("[HDA] Codec %u: %u audio path(s) found\n",
                ci, c->path_count);

        for (uint8_t pi = 0; pi < c->path_count; pi++) {
            hda_activate_path(c, &c->paths[pi], 1u,

                              (uint16_t)(HDA_FMT_BASE_48 | HDA_FMT_MULT_1X |
                                         HDA_FMT_DIV(1) | HDA_FMT_BITS_16 |
                                         HDA_FMT_CHAN(2)));
        }
    }
}
