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

#include <bearssl.h>
#include <br_kobalt.h>
#include <stdint.h>
#include <stddef.h>

static br_ssl_client_context   g_tls_client;
static br_x509_minimal_context g_x509;
static br_sslio_context        g_ioctx;

static uint8_t g_ibuf[BR_SSL_BUFSIZE_INPUT];
static uint8_t g_obuf[BR_SSL_BUFSIZE_OUTPUT];

static const br_x509_trust_anchor g_trust_anchors[] = {

};
#define N_TRUST_ANCHORS  (sizeof(g_trust_anchors) / sizeof(g_trust_anchors[0]))

static int tls_read_cb(void *ctx, uint8_t *buf, size_t len)
{

    (void)ctx; (void)buf; (void)len;
    return -1;
}

static int tls_write_cb(void *ctx, const uint8_t *buf, size_t len)
{

    (void)ctx; (void)buf; (void)len;
    return -1;
}

int ktls_connect(const char *hostname)
{

    br_x509_minimal_init(&g_x509, &br_sha256_vtable,
                         g_trust_anchors, N_TRUST_ANCHORS);

    br_ssl_client_init_full(&g_tls_client, &g_x509,
                            g_trust_anchors, N_TRUST_ANCHORS);

    br_ssl_engine_set_buffers_bidi(&g_tls_client.eng,
                                   g_ibuf, sizeof(g_ibuf),
                                   g_obuf, sizeof(g_obuf));

    br_ssl_client_reset(&g_tls_client, hostname, 0);

    br_sslio_init(&g_ioctx, &g_tls_client.eng,
                  tls_read_cb,  NULL,
                  tls_write_cb, NULL);

    if (br_sslio_flush(&g_ioctx) < 0) {
        int err = br_ssl_engine_last_error(&g_tls_client.eng);
        return (err != BR_ERR_OK) ? -err : -1;
    }

    return 0;
}

int ktls_write(const void *buf, size_t len)
{
    int r = br_sslio_write_all(&g_ioctx, buf, len);
    if (r < 0) return -br_ssl_engine_last_error(&g_tls_client.eng);
    return br_sslio_flush(&g_ioctx) < 0
           ? -br_ssl_engine_last_error(&g_tls_client.eng)
           : (int)len;
}

int ktls_read(void *buf, size_t len)
{
    int r = br_sslio_read(&g_ioctx, buf, len);
    if (r < 0) return -br_ssl_engine_last_error(&g_tls_client.eng);
    return r;
}

void ktls_close(void)
{
    br_sslio_close(&g_ioctx);
}
