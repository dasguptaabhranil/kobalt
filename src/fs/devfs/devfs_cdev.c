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

#include "devfs.h"
#include "devfs_tykid.h"
#include "devfs_ioctl.h"
#include "../../inc/kernel.h"
#include "../../inc/string.h"
#include "../../inc/random.h"
#include "../../drivers/vga/vga.h"

extern int kbd_getc(void);

__attribute__((weak)) int  uart_rx_avail(void)   { return 0; }
__attribute__((weak)) int  uart_read_byte(void)  { return -1; }
__attribute__((weak)) void uart_putc(char c)     { (void)c; }

static ssize_t null_read(void *p, void *buf, size_t n, uint64_t *pos)
{
    (void)p; (void)buf; (void)n; (void)pos;
    return 0;
}

static ssize_t null_write(void *p, const void *buf, size_t n, uint64_t *pos)
{
    (void)p; (void)buf; (void)pos;
    return (ssize_t)n;
}

devfs_ops_t devfs_null_ops = {
    .open  = NULL, .close = NULL,
    .read  = null_read, .write = null_write,
    .ioctl = NULL, .poll  = NULL,
};

static ssize_t zero_read(void *p, void *buf, size_t n, uint64_t *pos)
{
    (void)p;
    memset(buf, 0, n);
    *pos += n;
    return (ssize_t)n;
}

devfs_ops_t devfs_zero_ops = {
    .open  = NULL, .close = NULL,
    .read  = zero_read, .write = null_write,
    .ioctl = NULL, .poll  = NULL,
};

static ssize_t dev_random_read(void *p, void *buf, size_t n, uint64_t *pos)
{
    (void)p; (void)pos;
    random_read(buf, n);
    return (ssize_t)n;
}

static ssize_t dev_urandom_read(void *p, void *buf, size_t n, uint64_t *pos)
{
    (void)p; (void)pos;
    urandom_read(buf, n);
    return (ssize_t)n;
}

static ssize_t dev_random_write(void *p, const void *buf, size_t n, uint64_t *pos)
{
    (void)p; (void)pos;
    random_add_entropy(buf, n, 0u);
    return (ssize_t)n;
}

devfs_ops_t devfs_random_ops = {
    .open  = NULL, .close = NULL,
    .read  = dev_random_read, .write = dev_random_write,
    .ioctl = NULL, .poll  = NULL,
};

devfs_ops_t devfs_urandom_ops = {
    .open  = NULL, .close = NULL,
    .read  = dev_urandom_read, .write = dev_random_write,
    .ioctl = NULL, .poll  = NULL,
};

static int tty_open(void *p, int flags)
{
    (void)p; (void)flags;
    return 0;
}

static ssize_t tty_read(void *p, void *buf, size_t n, uint64_t *pos)
{
    (void)p; (void)pos;
    char *out = buf;
    size_t got = 0;
    while (got < n) {
        out[got] = kbd_getc();
        if (out[got++] == '\n') break;
    }
    return (ssize_t)got;
}

static ssize_t tty_write(void *p, const void *buf, size_t n, uint64_t *pos)
{
    (void)p; (void)pos;
    const char *s = buf;
    for (size_t i = 0; i < n; i++) vga_putc(s[i]);
    return (ssize_t)n;
}

static int tty_ioctl(void *p, unsigned long cmd, void *arg)
{
    (void)p;
    if (cmd == TIOCGWINSZ) {
        devfs_winsize_t *ws = arg;
        ws->ws_row = 25; ws->ws_col = 80;
        ws->ws_xpixel = 640; ws->ws_ypixel = 400;
        return 0;
    }
    return -1;
}

devfs_ops_t devfs_tty_ops = {
    .open  = tty_open, .close = NULL,
    .read  = tty_read, .write = tty_write,
    .ioctl = tty_ioctl, .poll  = NULL,
};

static ssize_t uart_cdev_read(void *p, void *buf, size_t n, uint64_t *pos)
{
    (void)p; (void)pos;
    char *out = buf;
    size_t got = 0;
    while (got < n && uart_rx_avail())
        out[got++] = (char)uart_read_byte();
    return (ssize_t)got;
}

static ssize_t uart_cdev_write(void *p, const void *buf, size_t n, uint64_t *pos)
{
    (void)p; (void)pos;
    const char *s = buf;
    for (size_t i = 0; i < n; i++) uart_putc(s[i]);
    return (ssize_t)n;
}

devfs_ops_t devfs_uart_ops = {
    .open  = NULL, .close = NULL,
    .read  = uart_cdev_read, .write = uart_cdev_write,
    .ioctl = NULL, .poll  = NULL,
};

devfs_file_t *devfs_cdev_open(devfs_node_t *node, int flags)
{
    if (!node) return NULL;
    if (devfs_tykid_check(node, flags) < 0) return NULL;

    if (node->ops && node->ops->open) {
        if (node->ops->open(node->priv, flags) < 0) return NULL;
    }

    devfs_file_t *f = devfs_alloc_file();
    if (!f) return NULL;
    f->node  = node;
    f->flags = flags;
    f->pos   = 0;
    return f;
}

void devfs_cdev_close(devfs_file_t *f)
{
    if (!f) return;
    if (f->node && f->node->ops && f->node->ops->close)
        f->node->ops->close(f->node->priv);
    devfs_free_file(f);
}

ssize_t devfs_cdev_read(devfs_file_t *f, void *buf, size_t n)
{
    if (!f || !f->node || !f->node->ops || !f->node->ops->read)
        return -1;
    return f->node->ops->read(f->node->priv, buf, n, &f->pos);
}

ssize_t devfs_cdev_write(devfs_file_t *f, const void *buf, size_t n)
{
    if (!f || !f->node || !f->node->ops || !f->node->ops->write)
        return -1;
    return f->node->ops->write(f->node->priv, buf, n, &f->pos);
}

int devfs_cdev_ioctl(devfs_file_t *f, unsigned long cmd, void *arg)
{
    if (!f || !f->node) return -1;
    if (cmd == DEVIOC_GETINFO) {
        devfs_info_t *di = arg;
        strncpy(di->name, f->node->name, sizeof(di->name) - 1);
        di->major       = MAJOR(f->node->dev);
        di->minor       = MINOR(f->node->dev);
        di->tykid_class = f->node->tykid_class;
        di->tykid_tag   = f->node->tykid_tag;
        return 0;
    }
    if (cmd == DEVIOC_GETTAG) {
        *(uint32_t *)arg = f->node->tykid_tag;
        return 0;
    }
    if (!f->node->ops || !f->node->ops->ioctl) return -1;
    return f->node->ops->ioctl(f->node->priv, cmd, arg);
}
