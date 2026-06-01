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

#include "../inc/pci.h"
#include "../inc/acpi.h"
#include "../inc/kfmt.h"

static pci_mcfg_t      *g_mcfg;
static uint64_t         g_ecam_base;
static int              g_ecam_ok;
static kobalt_pci_dev_t g_dev_cache[PCI_MAX_DEVICES];
static uint32_t         g_dev_count;

static volatile void *pci_ecam_addr(uint8_t bus, uint8_t dev,
                                    uint8_t func, uint16_t offset)
{
    uintptr_t addr = (uintptr_t)g_ecam_base
                   + ((uint32_t)bus  << 20)
                   + ((uint32_t)dev  << 15)
                   + ((uint32_t)func << 12)
                   + offset;
    return (volatile void *)addr;
}

static inline void outl_p(uint32_t val, uint16_t port)
{
    __asm__ volatile("outl %0, %w1" :: "a"(val), "Nd"(port) : "memory");
}

static inline uint32_t inl_p(uint16_t port)
{
    uint32_t v;
    __asm__ volatile("inl %w1, %0" : "=a"(v) : "Nd"(port) : "memory");
    return v;
}

static uint32_t cf8_addr(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t off)
{
    return 0x80000000u
         | ((uint32_t)bus << 16)
         | ((uint32_t)dev << 11)
         | ((uint32_t)fn  <<  8)
         | (off & 0xFCu);
}

static uint32_t legacy_read32(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t off)
{
    outl_p(cf8_addr(bus, dev, fn, off), 0xCF8u);
    uint32_t v = inl_p(0xCFCu);
    uint8_t shift = (off & 3u) * 8u;
    return v >> shift;
}

static void legacy_write32(uint8_t bus, uint8_t dev, uint8_t fn,
                           uint16_t off, uint32_t val)
{
    outl_p(cf8_addr(bus, dev, fn, off), 0xCF8u);
    outl_p(val, 0xCFCu);
}

uint8_t pci_read_config8(pci_device_t *dev, uint16_t offset)
{
    if (g_ecam_ok) {
        volatile uint8_t *p =
            (volatile uint8_t *)pci_ecam_addr(dev->bus, dev->device,
                                              dev->function, offset);
        return *p;
    }
    return (uint8_t)(legacy_read32(dev->bus, dev->device,
                                   dev->function, offset) & 0xFFu);
}

uint16_t pci_read_config16(pci_device_t *dev, uint16_t offset)
{
    if (g_ecam_ok) {
        volatile uint16_t *p =
            (volatile uint16_t *)pci_ecam_addr(dev->bus, dev->device,
                                               dev->function, offset);
        return *p;
    }
    return (uint16_t)(legacy_read32(dev->bus, dev->device,
                                    dev->function, offset) & 0xFFFFu);
}

uint32_t pci_read_config32(pci_device_t *dev, uint16_t offset)
{
    if (g_ecam_ok) {
        volatile uint32_t *p =
            (volatile uint32_t *)pci_ecam_addr(dev->bus, dev->device,
                                               dev->function, offset);
        return *p;
    }
    return legacy_read32(dev->bus, dev->device, dev->function, offset);
}

void pci_write_config8(pci_device_t *dev, uint16_t offset, uint8_t val)
{
    if (g_ecam_ok) {
        volatile uint8_t *p =
            (volatile uint8_t *)pci_ecam_addr(dev->bus, dev->device,
                                              dev->function, offset);
        *p = val;
        return;
    }
    outl_p(cf8_addr(dev->bus, dev->device, dev->function, offset), 0xCF8u);
    uint32_t old = inl_p(0xCFCu);
    uint8_t  sh  = (offset & 3u) * 8u;
    old = (old & ~((uint32_t)0xFFu << sh)) | ((uint32_t)val << sh);
    outl_p(old, 0xCFCu);
}

void pci_write_config16(pci_device_t *dev, uint16_t offset, uint16_t val)
{
    if (g_ecam_ok) {
        volatile uint16_t *p =
            (volatile uint16_t *)pci_ecam_addr(dev->bus, dev->device,
                                               dev->function, offset);
        *p = val;
        return;
    }
    outl_p(cf8_addr(dev->bus, dev->device, dev->function, offset), 0xCF8u);
    uint32_t old = inl_p(0xCFCu);
    uint8_t  sh  = (offset & 2u) * 8u;
    old = (old & ~((uint32_t)0xFFFFu << sh)) | ((uint32_t)val << sh);
    outl_p(old, 0xCFCu);
}

void pci_write_config32(pci_device_t *dev, uint16_t offset, uint32_t val)
{
    if (g_ecam_ok) {
        volatile uint32_t *p =
            (volatile uint32_t *)pci_ecam_addr(dev->bus, dev->device,
                                               dev->function, offset);
        *p = val;
        return;
    }
    legacy_write32(dev->bus, dev->device, dev->function, offset, val);
}

static void pci_cache_bars(kobalt_pci_dev_t *dev)
{
    for (uint8_t i = 0; i < PCI_MAX_BARS; ) {
        uint16_t cfg_off = (uint16_t)(PCI_CFG_BAR0 + i * 4u);
        uint32_t bar_lo  = pci_read_config32(dev, cfg_off);

        if (bar_lo & PCI_BAR_SPACE_IO) {
            dev->bar[i] = 0;
            i++;
            continue;
        }

        uint8_t bar_type = (uint8_t)(bar_lo & PCI_BAR_MEM_TYPE_MASK);

        if (bar_type == PCI_BAR_MEM_TYPE_64) {
            if (i + 1u >= PCI_MAX_BARS) { dev->bar[i] = 0; break; }
            uint32_t  bar_hi = pci_read_config32(dev, (uint16_t)(cfg_off + 4u));
            uintptr_t base   = ((uintptr_t)bar_hi << 32)
                             |  (bar_lo & (uint32_t)PCI_BAR_MEM_ADDR_MASK);
            dev->bar[i]     = base;
            dev->bar[i + 1] = 0;
            i += 2;
        } else {
            dev->bar[i] = (uintptr_t)(bar_lo & (uint32_t)PCI_BAR_MEM_ADDR_MASK);
            i++;
        }
    }
}

static uint8_t pci_probe_function(uint8_t bus, uint8_t devno, uint8_t func)
{
    if (g_dev_count >= PCI_MAX_DEVICES)
        return 0;

    uint32_t id_word;
    if (g_ecam_ok) {
        volatile uint32_t *cfg =
            (volatile uint32_t *)pci_ecam_addr(bus, devno, func, 0);
        id_word = *cfg;
    } else {
        id_word = legacy_read32(bus, devno, func, 0);
    }

    if ((id_word & 0xFFFFu) == 0xFFFFu)
        return 0;

    kobalt_pci_dev_t *d = &g_dev_cache[g_dev_count];

    d->bus       = bus;
    d->device    = devno;
    d->function  = func;
    d->vendor_id = (uint16_t)(id_word & 0xFFFFu);
    d->device_id = (uint16_t)(id_word >> 16);

    d->revision    = pci_read_config8(d, PCI_CFG_REVISION);
    d->prog_if     = pci_read_config8(d, PCI_CFG_PROG_IF);
    d->subclass    = pci_read_config8(d, PCI_CFG_SUBCLASS);
    d->class_code  = pci_read_config8(d, PCI_CFG_CLASS);
    d->header_type = pci_read_config8(d, PCI_CFG_HEADER_TYPE) & PCI_HDR_TYPE_MASK;
    d->irq_line    = pci_read_config8(d, PCI_CFG_IRQ_LINE);
    d->irq_pin     = pci_read_config8(d, PCI_CFG_IRQ_PIN);

    if (d->header_type == PCI_HDR_TYPE_ENDPOINT) {
        d->subsystem_vendor = pci_read_config16(d, PCI_CFG_SUBSYS_VENDOR);
        d->subsystem_id     = pci_read_config16(d, PCI_CFG_SUBSYS_ID);
        pci_cache_bars(d);
    } else {
        d->subsystem_vendor = 0;
        d->subsystem_id     = 0;
        for (uint8_t i = 0; i < PCI_MAX_BARS; i++)
            d->bar[i] = 0;
    }

    g_dev_count++;

    if (d->class_code == PCI_CLASS_BRIDGE &&
        d->subclass   == PCI_SUBCLASS_PCI_BRIDGE)
        return pci_read_config8(d, PCI_CFG_SECONDARY_BUS);
    return 0;
}

static void pci_scan_bus(uint8_t bus)
{
    for (uint8_t devno = 0; devno < 32u; devno++) {
        uint8_t secondary = pci_probe_function(bus, devno, 0);
        if (secondary)
            pci_scan_bus(secondary);

        uint8_t hdr;
        if (g_ecam_ok) {
            volatile uint8_t *hdr_ptr =
                (volatile uint8_t *)pci_ecam_addr(bus, devno, 0,
                                                   PCI_CFG_HEADER_TYPE);
            hdr = *hdr_ptr;
        } else {
            hdr = (uint8_t)((legacy_read32(bus, devno, 0, PCI_CFG_HEADER_TYPE & 0xFCu)
                             >> 8) & 0xFFu);
        }

        if (!(hdr & PCI_HDR_MULTI_FUNC))
            continue;

        for (uint8_t func = 1; func < 8u; func++) {
            secondary = pci_probe_function(bus, devno, func);
            if (secondary)
                pci_scan_bus(secondary);
        }
    }
}

void pci_init(void)
{
    g_mcfg = (pci_mcfg_t *)acpi_find_table("MCFG");
    if (!g_mcfg) {
        klog_warn("pci", "no MCFG -- falling back to legacy CF8/CFC");
        g_ecam_ok = 0;
    } else {
        uint32_t entries_len = g_mcfg->length - (uint32_t)sizeof(pci_mcfg_t);
        uint32_t n_entries   = entries_len / (uint32_t)sizeof(pci_mcfg_entry_t);

        if (n_entries == 0) {
            klog_warn("pci", "MCFG has no entries -- legacy fallback");
            g_ecam_ok = 0;
        } else {
            g_ecam_base = g_mcfg->entries[0].base_addr;
            g_ecam_ok   = 1;

            char msg[64];
            ksnprintf(msg, sizeof(msg),
                      "ECAM base 0x%x (seg 0, bus %u-%u)",
                      (uint32_t)g_ecam_base,
                      g_mcfg->entries[0].start_bus,
                      g_mcfg->entries[0].end_bus);
            klog_ok("pci", msg);
        }
    }

    if (!g_ecam_ok)
        klog_ok("pci", "using legacy IO port config access");

    uint8_t start_bus = g_ecam_ok ? g_mcfg->entries[0].start_bus : 0u;
    uint8_t end_bus   = g_ecam_ok ? g_mcfg->entries[0].end_bus   : 255u;

    for (uint8_t bus = start_bus; ; bus++) {
        pci_scan_bus(bus);
        if (bus == end_bus) break;
    }

    char count_msg[32];
    ksnprintf(count_msg, sizeof(count_msg), "%u device(s) discovered", g_dev_count);
    klog_ok("pci", count_msg);
}

uint32_t pci_count(void) { return g_dev_count; }

pci_device_t *pci_get_device(uint32_t index)
{
    if (index >= g_dev_count)
        return NULL;
    return &g_dev_cache[index];
}

pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id)
{
    for (uint32_t i = 0; i < g_dev_count; i++) {
        if (g_dev_cache[i].vendor_id == vendor_id &&
            g_dev_cache[i].device_id == device_id)
            return &g_dev_cache[i];
    }
    return NULL;
}

pci_device_t *pci_find_class(uint8_t class_code, uint8_t subclass)
{
    for (uint32_t i = 0; i < g_dev_count; i++) {
        if (g_dev_cache[i].class_code != class_code)
            continue;
        if (subclass != 0xFFu && g_dev_cache[i].subclass != subclass)
            continue;
        return &g_dev_cache[i];
    }
    return NULL;
}

void pci_list_devices(void)
{
    kputs("  B:D.F   Vendor:Dev   Cl:Sub  Rev  IRQ  Description\n");
    kputs("  ------  -----------  ------  ---  ---  -----------\n");

    for (uint32_t i = 0; i < g_dev_count; i++) {
        const kobalt_pci_dev_t *d = &g_dev_cache[i];
        char line[128];
        ksnprintf(line, sizeof(line),
                  "  %02x:%02x.%x  %04x:%04x   %02x:%02x   %02x   %3d",
                  d->bus, d->device, d->function,
                  d->vendor_id, d->device_id,
                  d->class_code, d->subclass,
                  d->revision,
                  (int)d->irq_line);
        kputs(line);
        kputs("\n");
    }
}

uintptr_t pci_bar_base(pci_device_t *dev, uint8_t bar_idx)
{
    if (bar_idx >= PCI_MAX_BARS)
        return 0;
    uint16_t  off    = (uint16_t)(PCI_CFG_BAR0 + bar_idx * 4u);
    uint32_t  bar_lo = pci_read_config32(dev, off);

    if (!bar_lo || (bar_lo & PCI_BAR_SPACE_IO))
        return 0;

    uintptr_t base = (uintptr_t)(bar_lo & (uint32_t)PCI_BAR_MEM_ADDR_MASK);

    if ((bar_lo & PCI_BAR_MEM_TYPE_MASK) == PCI_BAR_MEM_TYPE_64) {
        if (bar_idx + 1u < PCI_MAX_BARS) {
            uint64_t hi = pci_read_config32(dev, (uint16_t)(off + 4u));
            base |= (hi << 32);
        }
    }
    return base;
}

uint32_t pci_bar_size(pci_device_t *dev, uint8_t bar_idx)
{
    if (bar_idx >= PCI_MAX_BARS)
        return 0;

    uint16_t cfg_off = (uint16_t)(PCI_CFG_BAR0 + bar_idx * 4u);
    uint32_t bar_lo  = pci_read_config32(dev, cfg_off);

    if (bar_lo & PCI_BAR_SPACE_IO)
        return 0;

    uint16_t cmd = pci_read_config16(dev, PCI_CFG_COMMAND);
    pci_write_config16(dev, PCI_CFG_COMMAND,
                       (uint16_t)(cmd & ~(uint16_t)PCI_CMD_MEM_SPACE));

    pci_write_config32(dev, cfg_off, 0xFFFFFFFFu);
    uint32_t mask = pci_read_config32(dev, cfg_off);

    pci_write_config32(dev, cfg_off, bar_lo);
    pci_write_config16(dev, PCI_CFG_COMMAND, cmd);

    if (mask == 0 || mask == 0xFFFFFFFFu)
        return 0;

    return ~(mask & (uint32_t)PCI_BAR_MEM_ADDR_MASK) + 1u;
}

void pci_enable_device(pci_device_t *dev)
{
    uint16_t cmd = pci_read_config16(dev, PCI_CFG_COMMAND);
    pci_write_config16(dev, PCI_CFG_COMMAND,
                       (uint16_t)(cmd | PCI_CMD_BUS_MASTER | PCI_CMD_MEM_SPACE));
}
