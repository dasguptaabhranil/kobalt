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

#include "sysfs_kobalt.h"
#include <blkdev.h>
#include <kmalloc.h>
#include <string.h>
#include <kfmt.h>

static sysfs_obj_t *g_blk_dir;
static sysfs_obj_t *g_pci_dir;
static sysfs_obj_t *g_net_dir;

typedef struct {
    uint64_t nsectors;
    uint32_t sector_sz;
    int      removable;
    char     driver[32];
} blk_priv_t;

typedef struct {
    uint8_t  bus, dev, fn;
    uint16_t vendor, device;
    uint8_t  cls, sub, irq;
} pci_priv_t;

typedef struct {
    char mac[20];
    char driver[32];
} net_priv_t;

static int blk_show_size(sysfs_obj_t *obj, char *buf, size_t sz)
{
    blk_priv_t *p = obj->priv;
    return ksnprintf(buf, sz, "%llu\n", (unsigned long long)p->nsectors);
}

static int blk_show_removable(sysfs_obj_t *obj, char *buf, size_t sz)
{
    return ksnprintf(buf, sz, "%d\n", ((blk_priv_t *)obj->priv)->removable);
}

static int blk_show_sector_size(sysfs_obj_t *obj, char *buf, size_t sz)
{
    return ksnprintf(buf, sz, "%u\n", ((blk_priv_t *)obj->priv)->sector_sz);
}

static int blk_show_driver(sysfs_obj_t *obj, char *buf, size_t sz)
{
    return ksnprintf(buf, sz, "%s\n", ((blk_priv_t *)obj->priv)->driver);
}

static sysfs_attr_t blk_attr_size       = { "size",        0444, blk_show_size,        NULL };
static sysfs_attr_t blk_attr_removable  = { "removable",   0444, blk_show_removable,   NULL };
static sysfs_attr_t blk_attr_sectorsize = { "sector_size", 0444, blk_show_sector_size, NULL };
static sysfs_attr_t blk_attr_driver     = { "driver",      0444, blk_show_driver,      NULL };

static int pci_show_vendor(sysfs_obj_t *obj, char *buf, size_t sz)
{
    return ksnprintf(buf, sz, "0x%04x\n", ((pci_priv_t *)obj->priv)->vendor);
}

static int pci_show_device(sysfs_obj_t *obj, char *buf, size_t sz)
{
    return ksnprintf(buf, sz, "0x%04x\n", ((pci_priv_t *)obj->priv)->device);
}

static int pci_show_class(sysfs_obj_t *obj, char *buf, size_t sz)
{
    pci_priv_t *p = obj->priv;
    return ksnprintf(buf, sz, "0x%02x%02x\n", p->cls, p->sub);
}

static int pci_show_irq(sysfs_obj_t *obj, char *buf, size_t sz)
{
    return ksnprintf(buf, sz, "%u\n", (unsigned)((pci_priv_t *)obj->priv)->irq);
}

static sysfs_attr_t pci_attr_vendor = { "vendor", 0444, pci_show_vendor, NULL };
static sysfs_attr_t pci_attr_device = { "device", 0444, pci_show_device, NULL };
static sysfs_attr_t pci_attr_class  = { "class",  0444, pci_show_class,  NULL };
static sysfs_attr_t pci_attr_irq    = { "irq",    0444, pci_show_irq,    NULL };

static int net_show_mac(sysfs_obj_t *obj, char *buf, size_t sz)
{
    return ksnprintf(buf, sz, "%s\n", ((net_priv_t *)obj->priv)->mac);
}

static int net_show_driver(sysfs_obj_t *obj, char *buf, size_t sz)
{
    return ksnprintf(buf, sz, "%s\n", ((net_priv_t *)obj->priv)->driver);
}

static sysfs_attr_t net_attr_mac    = { "address", 0444, net_show_mac,    NULL };
static sysfs_attr_t net_attr_driver = { "driver",  0444, net_show_driver, NULL };

sysfs_obj_t *sysfs_blkdev_add(const char *devname, int removable,
                               uint64_t nsectors, const char *driver)
{
    if (!g_blk_dir) return NULL;

    blk_priv_t *p = kmalloc(sizeof(*p));
    if (!p) return NULL;
    p->nsectors  = nsectors;
    p->sector_sz = 512;
    p->removable = removable;
    size_t dl = strlen(driver);
    if (dl >= sizeof(p->driver)) dl = sizeof(p->driver) - 1;
    memcpy(p->driver, driver, dl);
    p->driver[dl] = '\0';

    sysfs_obj_t *obj = sysfs_obj_create(devname, g_blk_dir);
    if (!obj) { kfree(p); return NULL; }
    obj->priv = p;

    sysfs_add_attr(obj, &blk_attr_size);
    sysfs_add_attr(obj, &blk_attr_removable);
    sysfs_add_attr(obj, &blk_attr_sectorsize);
    sysfs_add_attr(obj, &blk_attr_driver);
    return obj;
}

void sysfs_blkdev_remove(const char *devname)
{
    if (!g_blk_dir) return;
    sysfs_obj_t *obj = sysfs_lookup(g_blk_dir, devname);
    if (!obj) return;
    kfree(obj->priv);
    obj->priv = NULL;
    sysfs_obj_unref(obj);
    sysfs_obj_unref(obj);
}

sysfs_obj_t *sysfs_pci_add_device(uint8_t bus, uint8_t dev, uint8_t fn,
                                   uint16_t vendor, uint16_t device,
                                   uint8_t cls, uint8_t sub, uint8_t irq)
{
    if (!g_pci_dir) return NULL;

    pci_priv_t *p = kmalloc(sizeof(*p));
    if (!p) return NULL;
    p->bus    = bus;
    p->dev    = dev;
    p->fn     = fn;
    p->vendor = vendor;
    p->device = device;
    p->cls    = cls;
    p->sub    = sub;
    p->irq    = irq;

    char name[20];
    ksnprintf(name, sizeof(name), "%04x:%02x.%x",
              (unsigned)bus, (unsigned)dev, (unsigned)fn);

    sysfs_obj_t *obj = sysfs_obj_create(name, g_pci_dir);
    if (!obj) { kfree(p); return NULL; }
    obj->priv = p;

    sysfs_add_attr(obj, &pci_attr_vendor);
    sysfs_add_attr(obj, &pci_attr_device);
    sysfs_add_attr(obj, &pci_attr_class);
    sysfs_add_attr(obj, &pci_attr_irq);
    return obj;
}

sysfs_obj_t *sysfs_net_add(const char *ifname, const char *mac,
                            const char *driver)
{
    if (!g_net_dir) return NULL;

    net_priv_t *p = kmalloc(sizeof(*p));
    if (!p) return NULL;

    size_t ml = strlen(mac);
    if (ml >= sizeof(p->mac)) ml = sizeof(p->mac) - 1;
    memcpy(p->mac, mac, ml);
    p->mac[ml] = '\0';

    size_t dl = strlen(driver);
    if (dl >= sizeof(p->driver)) dl = sizeof(p->driver) - 1;
    memcpy(p->driver, driver, dl);
    p->driver[dl] = '\0';

    sysfs_obj_t *obj = sysfs_obj_create(ifname, g_net_dir);
    if (!obj) { kfree(p); return NULL; }
    obj->priv = p;

    sysfs_add_attr(obj, &net_attr_mac);
    sysfs_add_attr(obj, &net_attr_driver);
    return obj;
}

void sysfs_net_remove(const char *ifname)
{
    if (!g_net_dir) return;
    sysfs_obj_t *obj = sysfs_lookup(g_net_dir, ifname);
    if (!obj) return;
    kfree(obj->priv);
    obj->priv = NULL;
    sysfs_obj_unref(obj);
    sysfs_obj_unref(obj);
}

void sysfs_kobalt_init(void)
{
    g_blk_dir = sysfs_mkdir_p("block",          NULL);
    g_pci_dir = sysfs_mkdir_p("bus/pci/devices", NULL);
    g_net_dir = sysfs_mkdir_p("class/net",       NULL);

    unsigned int n = blkdev_count();
    for (unsigned int i = 0; i < n; i++) {
        blkdev_t *d = blkdev_get(i);
        if (!d) continue;
        const char *drv = "unknown";
        if      (d->name[0] == 'a') drv = "ahci";
        else if (d->name[0] == 'n') drv = "nvme";
        else if (d->name[0] == 'v') drv = "virtio-blk";
        sysfs_blkdev_add(d->name, 0, d->num_sectors, drv);
    }
}
