/* Copyright (C) 2026 Abhranil Dasgupta
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see https://www.gnu.org/licenses/.
 */

#include <string.h>
#include <kernel.h>
#include <idt.h>
#include <gdt.h>
#include <cpuid.h>
#include <msr.h>
#include <smap.h>
#include <pat.h>
#include <speculation.h>
#include <debug.h>
#include <exception.h>
#include <fpu.h>
#include <hw_breakpoint.h>
#include <signal_frame.h>
#include <syscall.h>
#include <uaccess.h>
#include <vdso.h>
#include <xsave.h>
#include <acpi.h>
#include <madt.h>
#include <pci.h>
#include <pci_msi.h>
#include "../drivers/vga/vga.h"
#include "../drivers/vga/fb.h"
#include "../drivers/vga/fb_font.h"
#include "../drivers/vga/fb_console.h"
#include "../drivers/ps2/kbd.h"
#include <iommu.h>
#include <virtio.h>
#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/timeouts.h>
#include <lwip/icmp.h>
#include <lwip/ip_addr.h>
#include <lwip/raw.h>
#include <netif/ethernet.h>
#include <lwip/ip4_addr.h>
#include <lwip/inet_chksum.h>
#include <lwip/dhcp.h>
#include <lwip/prot/dhcp.h>
#include <lwip/etharp.h>
#include <kmalloc.h>
#include <sched.h>
#include <irq.h>
#include <apic_timer.h>
#include <kssh.h>
#include <kobalt_ident.h>
#include <tykid.h>
#include <kposixz.h>
#include <ktrace.h>
#include <sound.h>
#include <blkdev.h>
#include "../drivers/ahci/ahci.h"
#include "../drivers/nvme/nvme.h"
#include "../drivers/virtio/storage/virtio_blk.h"
#include <usb_init.h>
#include "../drivers/net/e1000/e1000.h"
#include "../drivers/net/igc/kobalt_igc.h"
#include "../drivers/net/ixgbe/kobalt_ixgbe.h"
#include "../drivers/usb/ehci/kobalt_ehci.h"
#include <percpu.h>
#include <spinlock.h>
#include <ipi.h>
#include <smp.h>
#include <random.h>
#include <flatfs.h>
#include <flatfs_vfs.h>
#include <flatfs_kobalt.h>
#include <tmpfs_vfs.h>
#include <fatfs_kobalt.h>
#include <ff.h>
#include <devfs_kobalt.h>
#include <devfs_vfs.h>
#include <procfs_vfs.h>
#include <sysfs_vfs.h>
#include <vfs.h>
#include <rtc.h>
#include <numa.h>
#include <cpufreq.h>
#include <cpuidle.h>
#include <hugepage.h>
#include <mreclaim.h>
#include <swap.h>
#include <cpuhp.h>
#include <waitqueue.h>
#include <uaccess.h>
#include <amx_init.h>
#include <ksh.h>

#define KOBALT_OS       "Kobalt"
#define KOBALT_VERSION  "1.0.4-b"
#define KOBALT_BUILD    "#1 SMP"
#define KOBALT_ARCH     "x86_64"
#define KOBALT_HOSTNAME "kobalt"
#define KOBALT_FULL     KOBALT_OS " " KOBALT_VERSION " " KOBALT_BUILD

#define DMESG_SIZE  65536u

#define DHCP_TIMEOUT_MS 5000u

#define NET_STATIC_ADDRS(ip, mask, gw) \
    IP4_ADDR(ip,   10,  0,  2, 15);   \
    IP4_ADDR(mask, 255,255,255,  0);  \
    IP4_ADDR(gw,   10,  0,  2,  2)

#define NET_STATIC_LOG  "static: 10.0.2.15 / 255.255.255.0  gw 10.0.2.2"

#ifndef netif_dhcp_data
#define netif_dhcp_data(netif) ((struct dhcp*)netif_get_client_data(netif, LWIP_NETIF_CLIENT_DATA_INDEX_DHCP))
#endif

char          g_dmesg[DMESG_SIZE];
size_t        g_dmesg_pos = 0;
static int        g_nxhci = 0;
static rtc_time_t g_boot_time;

static kobalt_igc_t   g_igc;
static kobalt_ixgbe_t g_ixgbe;
static kobalt_ehci_t  g_ehci;
static const char    *g_active_netdrv = NULL;

void dmesg_write(const char *s)
{
    while (*s && g_dmesg_pos < DMESG_SIZE - 1u)
        g_dmesg[g_dmesg_pos++] = *s++;
}

extern err_t virtio_netif_init(struct netif *netif);
extern void  virtio_net_mac(uint8_t mac[6]);
extern void     net_poll(void);
extern void     tsc_calibrate(void);
extern uint32_t sys_now(void);
extern void tykid_kobalt_config_init(tykid_config_t *cfg, uint64_t boot_token);
extern void tykid_kobalt_register_builtins(tykid_gate_ctx_t *ctx);
extern void tykid_kobalt_gate_builtins(tykid_gate_ctx_t *ctx,
                                        const tykid_hw_enumset_t *hw);
extern int  tykid_kobalt_builtin_approved(tykid_gate_ctx_t *ctx,
                                           const char *name);
extern void tykid_kobalt_set_ctx(tykid_gate_ctx_t *ctx);
extern tykid_gate_ctx_t *tykid_kobalt_get_ctx(void);
extern void xsave_init(void);
extern void hw_breakpoint_init(void);
extern void signal_frame_init(void);
extern void uaccess_init(void);
extern void amx_run_tests(void);

struct netif kobalt_netif;

static void kputc(char c)
{
    vga_putc(c);
    uart_putc(c);
}

static void print_banner(void)
{
    vga_clear();
    kputs(
        "\n"
        "  --------------------------------------------------------\n"
        "\n"
    );
    kprintf("  %s\n", KOBALT_FULL);
    kprintf("  System is Loading... Please Wait!\n");
    {
        char dtbuf[20];
        rtc_fmt(&g_boot_time, dtbuf, sizeof(dtbuf));
        kprintf("  %s UTC\n", dtbuf);
    }
    kputs(
        "\n"
        "  --------------------------------------------------------\n"
        "\n"
    );
}

void my_security_auditor(const uint8_t *digest, uint64_t iter)
{
    static uint8_t prev[CRYPTO_SHA256_DIGEST_SIZE];
    static int     init = 0;

    if (!init) {
        __builtin_memcpy(prev, digest, CRYPTO_SHA256_DIGEST_SIZE);
        init = 1;
        return;
    }

    if (__builtin_memcmp(digest, prev, CRYPTO_SHA256_DIGEST_SIZE) == 0) {
        char buf[64];
        ksnprintf(buf, sizeof(buf), "digest stall at iter %llu",
                  (unsigned long long)iter);
        klog_warn("kssh", buf);
    }

    __builtin_memcpy(prev, digest, CRYPTO_SHA256_DIGEST_SIZE);

    if ((iter & 0xFFULL) == 0) {
        char buf[80];
        ksnprintf(buf, sizeof(buf), "iter=%llu d0=%02x%02x%02x%02x",
                  (unsigned long long)iter,
                  digest[0], digest[1], digest[2], digest[3]);
        klog_info("kssh", buf);
    }
}

__attribute__((noreturn))
static void kpanic(const char *reason)
{
    kprintf("\n\n*** [!!] KERNEL PANIC: %s\n", reason);
    kstack_trace();
    kputs("*** System halted.\n");
    for (;;) __asm__ volatile("cli; hlt" ::: "memory");
}

static void run_init_cfg(void)
{
    int fd = vfs_open("/init.cfg", VFS_O_RDONLY, 0);
    if (fd < 0) {
        klog_info("fs", "init.cfg not found -- skipping");
        return;
    }

    vfs_stat_t st;
    if (vfs_fstat(fd, &st) < 0 || st.size == 0) {
        vfs_close(fd);
        return;
    }

    klog_ok("fs", "init.cfg found -- executing");

    char   lbuf[256];
    size_t li = 0;

    for (uint64_t i = 0; i <= st.size; i++) {
        char c = '\n';
        if (i < st.size) {
            if (vfs_read(fd, &c, 1) != 1) break;
        }

        if (c == '\n' || c == '\r') {
            lbuf[li] = '\0';
            li = 0;
            const char *p = lbuf;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\0' || *p == '#') continue;
            ksh_dispatch(p);
        } else {
            if (li < sizeof(lbuf) - 1u)
                lbuf[li++] = c;
        }
    }

    vfs_close(fd);
}


void kmain(uint32_t boot_magic, void *boot_info)
{
    uint64_t bsp_stack_top;
    __asm__ volatile ("lea 8(%%rsp), %0" : "=r"(bsp_stack_top));
    uart_init();
    vga_init();
    if (boot_magic == 0x36D76289u)
        fb_init_from_mbi(boot_info);
    else
        fb_init_pvh(boot_info);

    cpuid_init();
    debug_init();
    xsave_init();
    hw_breakpoint_init();

    rtc_read(&g_boot_time);

    gdt_init();
    idt_init();
    exception_init();
    signal_frame_init();
    vfs_init();

    extern void pgfault_entry(void);
    idt_set_gate(14, (uintptr_t)pgfault_entry, GDT_KCODE64,
                 IDT_GATE_INTERRUPT, 0);

    pat_init();
    smap_init();
    speculation_init();
    fpu_init();

    klog_ok("cpu", "GDT/IDT loaded");

    klog_info("kposix", "kposix init...");
    if (kposixz_init() != 0) {
        klog_fail("kposix", "syscall layer failed");
    } else {
        klog_ok("kposix", "syscall layer ready");
    }

    kssh_integrity_hook = my_security_auditor;

    irq_init();

    {
        uint32_t n = pci_count();
        int msi_ok = 0, msi_fail = 0;
        for (uint32_t i = 0; i < n; i++) {
            pci_device_t *d = pci_get_device(i);
            if (!d) continue;
            if (d->class_code == PCI_CLASS_BRIDGE) continue;
            if (d->class_code == PCI_CLASS_NETWORK) continue;
            if (!pci_find_cap(d, PCI_CAP_ID_MSI) &&
                !pci_find_cap(d, PCI_CAP_ID_MSIX)) continue;
            int vec = pci_enable_msi(d, 1);
            if (vec >= 0) msi_ok++;
            else          msi_fail++;
        }
        if (msi_ok > 0) {
            char msi_msg[56];
            ksnprintf(msi_msg, sizeof(msi_msg),
                      "%d device(s) using MSI, %d fell back to INTx",
                      msi_ok, msi_fail);
            klog_ok("msi", msi_msg);
        } else {
            klog_info("msi", "no MSI-capable devices found");
        }
    }

    numa_init();
    kmalloc_init();

    if (!acpi_init()) {
        klog_fail("acpi", "init failed");
        kpanic("ACPI required for IOMMU/APIC");
    }
    klog_ok("acpi", "ACPI tables located");

    pci_init();
    madt_parse();
    klog_ok("pci", "PCI bus enumerated");
    iommu_init();
    klog_ok("iommu", "DMA remapping active");

    hugepage_init();
    apic_timer_init();
    sched_init(bsp_stack_top);
	smp_bsp_early_init();
    sti();
    klog_ok("cpu", "preemption enabled");
    klog_ok("sched", "EEVDF scheduler online");

    ipi_init();
    klog_ok("ipi", "IPI vectors registered");

    mreclaim_init();

    smp_init();
    {
        char smp_msg[48];
        ksnprintf(smp_msg, sizeof(smp_msg),
                  "%u CPU(s) online", smp_cpu_count());
        klog_ok("smp", smp_msg);
    }
    cpufreq_init();
    cpuidle_init();
    cpuhp_init();
    syscall_init();
    uaccess_init();
    klog_ok("cpu", "SYSCALL/SYSRET is working");

    random_init();
    klog_ok("random", "entropy pool seeded");

    tsc_calibrate();
    vdso_init();
    lwip_init();
    klog_ok("lwip", "TCP/IP stack up");

    tykid_gate_ctx_t   *ty_ctx = NULL;
    tykid_hw_enumset_t  ty_hw;

    {
        uint64_t boot_token = (uint64_t)sys_now()
                            ^ KOBALT_KERNEL_IDENT
                            ^ ((uint64_t)boot_magic << 32);

        tykid_config_t ty_cfg;
        tykid_kobalt_config_init(&ty_cfg, boot_token);

        tykid_status_t ty_st = tykid_init(&ty_cfg, &ty_ctx);
        if (ty_st != TYKID_OK) {
            klog_fail("tykid", tykid_strerror(ty_st));
            kpanic("TYKID gate init failed");
        }
        klog_ok("tykid", "driver gate active");

        tykid_kobalt_set_ctx(ty_ctx);

        tykid_kobalt_register_builtins(ty_ctx);

        ty_st = tykid_enumerate_hardware(ty_ctx, &ty_hw);
        if (ty_st == TYKID_OK) {
            char hw_msg[72];
            ksnprintf(hw_msg, sizeof(hw_msg),
                      "%u device(s) fingerprinted, topology %08x",
                      ty_hw.count,
                      (unsigned)(ty_hw.bus_topology_hash & 0xFFFFFFFFUL));
            klog_ok("tykid", hw_msg);
        }

        ty_hw.iommu_active = (bool8)(acpi_find_table("DMAR") != NULL);
        tykid_kobalt_gate_builtins(ty_ctx, &ty_hw);
    }

    if (tykid_kobalt_builtin_approved(ty_ctx, "virtio-net") &&
        virtio_net_init() == 0) {
        klog_ok("virtio", "VirtIO-Net ready");
        g_active_netdrv = "virtio-net";
    } else {
        if (!tykid_kobalt_builtin_approved(ty_ctx, "virtio-net"))
            klog_info("net", "virtio-net skipped by TYKID (no hardware), trying igc");
        else
            klog_info("net", "VirtIO-Net not found, trying igc");

        int igc_found = 0;
        if (tykid_kobalt_builtin_approved(ty_ctx, "igc")) {
            static const uint16_t igc_devids[] = {
                0x15F2, 0x15F3, 0x3100, 0x3101, 0x0D9F, 0
            };
            uint32_t npci = pci_count();
            for (uint32_t i = 0; i < npci && !igc_found; i++) {
                pci_device_t *d = pci_get_device(i);
                if (!d || d->vendor_id != 0x8086) continue;
                for (int j = 0; igc_devids[j]; j++) {
                    if (d->device_id == igc_devids[j]) {
                        if (kobalt_igc_init(d, &g_igc) == 0) {
                            igc_found = 1;
                            g_active_netdrv = "igc";
                            klog_ok("igc", "I225 ready");
                        } else {
                            klog_fail("igc", "kobalt_igc_init failed");
                        }
                        break;
                    }
                }
            }
        } else {
            klog_info("net", "igc skipped by TYKID (no hardware)");
        }

        if (!igc_found) {
            int ixgbe_found = 0;
            if (tykid_kobalt_builtin_approved(ty_ctx, "ixgbe")) {
                klog_info("net", "igc not found, trying ixgbe");
                static const uint16_t ixgbe_devids[] = {
                    0x10B6, 0x10C6, 0x10C7, 0x10C8, 0x10E1, 0x10F1, 0x10F4,
                    0x10DB, 0x10DC, 0x10FB, 0x10F7, 0x10F8, 0x10F9, 0x1517,
                    0x10E7, 0x1514, 0x1515, 0x1507, 0x1508, 0x1528, 0x1560,
                    0x1530, 0x1531, 0x1563, 0x15D4, 0x15E5, 0x15CE, 0x1565,
                    0x15D1, 0x15D2, 0x15AD, 0x15AE, 0x15C8, 0x15C6, 0x15C7,
                    0x57AE, 0x57AF, 0x57B0, 0x57B1, 0x57B2, 0
                };
                uint32_t npci = pci_count();
                for (uint32_t i = 0; i < npci && !ixgbe_found; i++) {
                    pci_device_t *d = pci_get_device(i);
                    if (!d || d->vendor_id != 0x8086) continue;
                    for (int j = 0; ixgbe_devids[j]; j++) {
                        if (d->device_id == ixgbe_devids[j]) {
                            if (kobalt_ixgbe_init(d, &g_ixgbe) == 0) {
                                ixgbe_found = 1;
                                g_active_netdrv = "ixgbe";
                                klog_ok("ixgbe", "10GbE ready");
                            } else {
                                klog_fail("ixgbe", "kobalt_ixgbe_init failed");
                            }
                            break;
                        }
                    }
                }
            } else {
                klog_info("net", "ixgbe skipped by TYKID (no hardware)");
            }

            if (!ixgbe_found) {
                if (tykid_kobalt_builtin_approved(ty_ctx, "e1000")) {
                    klog_info("net", "trying e1000");
                    e1000_init();
                    if (!e1000_present())
                        klog_warn("net", "no usable network interface");
                    else
                        g_active_netdrv = "e1000";
                } else {
                    klog_info("net", "e1000 skipped by TYKID (no hardware)");
                }
            }
            goto net_done;
        }
    }

    {
        ip4_addr_t ip, mask, gw;
        ip4_addr_set_zero(&ip);
        ip4_addr_set_zero(&mask);
        ip4_addr_set_zero(&gw);

        if (!netif_add(&kobalt_netif, &ip, &mask, &gw,
                       NULL, virtio_netif_init, ethernet_input)) {
            klog_fail("net", "netif_add failed");
            goto net_done;
        }
    }

    netif_set_default(&kobalt_netif);
    netif_set_up(&kobalt_netif);
    netif_set_link_up(&kobalt_netif);

    {
        int bound = 0;
        err_t dhcp_err = dhcp_start(&kobalt_netif);
        if (dhcp_err != ERR_OK) {
            klog_warn("dhcp", "dhcp_start() failed -- static fallback");
            goto net_static_fallback;
        }

        klog_info("dhcp", "DHCP DISCOVER sent");

        uint32_t dhcp_t0 = sys_now();

        while ((sys_now() - dhcp_t0) < DHCP_TIMEOUT_MS) {
            net_poll();
            sys_check_timeouts();
            if (dhcp_supplied_address(&kobalt_netif)) { bound = 1; break; }
            struct dhcp *d = netif_dhcp_data(&kobalt_netif);
            if (d && d->state == DHCP_STATE_OFF) break;
        }

        if (bound) {
            char addr_buf[16], mask_buf[16], gw_buf[16];
            ksnprintf(addr_buf, sizeof(addr_buf), "%s",
                      ip4addr_ntoa(netif_ip4_addr(&kobalt_netif)));
            ksnprintf(mask_buf, sizeof(mask_buf), "%s",
                      ip4addr_ntoa(netif_ip4_netmask(&kobalt_netif)));
            ksnprintf(gw_buf,   sizeof(gw_buf),   "%s",
                      ip4addr_ntoa(netif_ip4_gw(&kobalt_netif)));
            char lease_msg[80];
            ksnprintf(lease_msg, sizeof(lease_msg),
                      "%s / %s  gw %s  (DHCP)",
                      addr_buf, mask_buf, gw_buf);
            klog_ok("dhcp", lease_msg);
            goto net_print_mac;
        }

        {
            uint32_t elapsed = sys_now() - dhcp_t0;
            char to_msg[48];
            ksnprintf(to_msg, sizeof(to_msg),
                      "no lease after %u ms -- static fallback", elapsed);
            klog_info("dhcp", to_msg);
        }
        dhcp_stop(&kobalt_netif);

net_static_fallback:;
        ip4_addr_t sip, smask, sgw;
        NET_STATIC_ADDRS(&sip, &smask, &sgw);
        netif_set_addr(&kobalt_netif, &sip, &smask, &sgw);

        etharp_gratuitous(&kobalt_netif);
        etharp_request(&kobalt_netif, &sgw);
        {
            uint32_t arp_t0 = sys_now();
            while ((sys_now() - arp_t0) < 500u)
                net_poll();
        }

        klog_info("net", NET_STATIC_LOG);
    }

net_print_mac:;
    {
        uint8_t mac[6];
        virtio_net_mac(mac);
        char mac_msg[48];
        ksnprintf(mac_msg, sizeof(mac_msg),
                  "MAC %02x:%02x:%02x:%02x:%02x:%02x  MTU 1500",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        klog_ok("net", mac_msg);
        klog_ok("net", "IPv4 stack up");
    }

net_done:;
    if (g_active_netdrv)
        klog_ok("net", g_active_netdrv);
    else
        klog_warn("net", "no active Ethernet driver");

    sched_thread_t *crypto_th = sched_thread_create("k_crypto", crypto_daemon, NULL, 0);
    if (crypto_th) {
        klog_ok("kssh", "integrity daemon started (k_crypto)");
    } else {
        klog_warn("kssh", "failed to create k_crypto thread");
    }

    kbd_init();
    klog_ok("kbd", "PS/2 keyboard ready");

    if (fb_console_set_zoom(2) == 0) {
        vga_cursor_disable();
        klog_ok("fb", "framebuffer console active");
    }

    {
        int n_blk = 0;
        int blk_rc;

        if (tykid_kobalt_builtin_approved(ty_ctx, "ahci")) {
            blk_rc = ahci_init();
            if (blk_rc > 0) {
                char msg[48];
                ksnprintf(msg, sizeof(msg), "%d SATA drive(s) registered", blk_rc);
                klog_ok("ahci", msg);
                n_blk += blk_rc;
            } else {
                klog_info("ahci", blk_rc == 0 ? "controller found, no drives"
                                               : "no AHCI controller");
            }
        } else {
            klog_info("ahci", "skipped by TYKID (no SATA hardware)");
        }

        if (tykid_kobalt_builtin_approved(ty_ctx, "nvme")) {
            blk_rc = nvme_init();
            if (blk_rc > 0) {
                char msg[48];
                ksnprintf(msg, sizeof(msg), "%d NVMe namespace(s) registered", blk_rc);
                klog_ok("nvme", msg);
                n_blk += blk_rc;
            } else {
                klog_info("nvme", "no NVMe controller");
            }
        } else {
            klog_info("nvme", "skipped by TYKID (no NVMe hardware)");
        }

        if (tykid_kobalt_builtin_approved(ty_ctx, "virtio-blk")) {
            blk_rc = virtio_blk_init();
            if (blk_rc == 0) {
                klog_ok("virtio-blk", "block device registered");
                n_blk++;
            } else {
                klog_info("virtio-blk", "no VirtIO block device");
            }
        } else {
            klog_info("virtio-blk", "skipped by TYKID (no VirtIO block hardware)");
        }

        if (n_blk > 0) {
            char sum[48];
            ksnprintf(sum, sizeof(sum), "%u block device(s) ready",
                      blkdev_count());
            klog_ok("blkdev", sum);
        } else {
            klog_warn("blkdev", "no block storage available");
        }
    }

    swap_init();
    swap_probe_blkdev();

    {
        extern int xhci_ctrl_count(void);
        if (tykid_kobalt_builtin_approved(ty_ctx, "xhci")) {
            usb_subsystem_init();
            g_nxhci = xhci_ctrl_count();
        } else {
            klog_info("xhci", "skipped by TYKID (no xHCI hardware)");
            g_nxhci = 0;
        }
    }

    {
        uint32_t npci = pci_count();
        for (uint32_t i = 0; i < npci; i++) {
            pci_device_t *d = pci_get_device(i);
            if (!d) continue;
            if (d->class_code != 0x0C || d->subclass != 0x03) continue;
            if (d->prog_if != 0x20) continue;
            if (tykid_kobalt_builtin_approved(ty_ctx, "ehci")) {
                if (kobalt_ehci_init(d, &g_ehci) == 0)
                    klog_ok("ehci", "controller ready");
                else
                    klog_fail("ehci", "init failed");
            } else {
                klog_info("ehci", "skipped by TYKID (no EHCI hardware)");
            }
            break;
        }
    }


    {
        devfs_kobalt_init(ty_ctx);

        flatfs_kobalt_mount(ty_ctx);

        if (flatfs_kobalt_is_mounted()) {
            flatfs_vfs_register();
            tmpfs_vfs_register();
            if (devfs_kobalt_is_mounted())
                devfs_vfs_register();
            procfs_vfs_register();
            klog_ok("procfs", "mounted at /proc");
        } else {
            klog_warn("procfs", "skipped -- flatfs not mounted");
        }

        sysfs_vfs_register();
        klog_ok("sysfs", "mounted at /sys");
    }

    {
        fatfs_kobalt_init();
        int nfat = fatfs_kobalt_mounted_count();
        if (nfat > 0) {
            char fat_msg[56];
            ksnprintf(fat_msg, sizeof(fat_msg), "%d FAT/exFAT volume(s) mounted", nfat);
            klog_ok("fatfs", fat_msg);

            for (int vi = 0; vi < FATFS_KOBALT_MAX_VOLS; vi++) {
                if (!fatfs_kobalt_is_mounted(vi)) continue;
                char vmsg[48];
                ksnprintf(vmsg, sizeof(vmsg), "volume %d: blkdev %d", vi, vi);
                klog_info("fatfs", vmsg);
            }
        } else {
            klog_info("fatfs", "no FAT/exFAT filesystem found on any blkdev");
        }
    }

    void *dmar = acpi_find_table("DMAR");
    if (dmar)
        klog_ok("acpi",  "DMAR table present, VT-d remapping confirmed");
    else
        klog_info("acpi", "no DMAR table -- hardware remapping unavailable");

    amx_init_cpu();
    amx_run_tests();
    mm_seal();
    klog_ok("mm", "page protections applied");
    devfs_kobalt_seal();

    {
        if (tykid_kobalt_builtin_approved(ty_ctx, "intel_hda")) {
            sound_status_t snd_st = sound_init();
            if (snd_st == SOUND_OK)
                klog_ok("sound", "HDA ready");
            else if (snd_st == SOUND_ERR_NOT_FOUND)
                klog_info("sound", "no audio hardware found");
            else
                klog_warn("sound", sound_strerror(snd_st));
        } else {
            klog_info("sound", "skipped by TYKID (no HDA hardware)");
        }
    }

    run_init_cfg();

    {
        char dtbuf[20];
        rtc_fmt(&g_boot_time, dtbuf, sizeof(dtbuf));
        vga_clear();
        kputs("The System has successfully loaded. This is a Kernel side Shell.\n\n");
        kprintf("Kobalt x86_64  %s  %s\n", dtbuf, KOBALT_VERSION);
        kputs("User: kernel\n\n");
        kputs("Copyright (c) 2026 Abhranil Dasgupta. This software is Free Software &\n");
        kputs("you can redistribute it and/or modify it under the terms of the GNU\n");
        kputs("General Public License as published by the Free Software Foundation, Version 3 of the License only.\n\n");
		kputs("For More Info Visit Kobalt Kernel's Website.\n\n");
    }

    ksh_run();
}