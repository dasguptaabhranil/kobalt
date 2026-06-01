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

#define ACPI_SIG_RSDP 0x50445352UL
#define ACPI_SIG_RSDT 0x54445352UL
#define ACPI_SIG_XSDT 0x54445358UL
#define ACPI_SIG_DSDT 0x54445344UL
#define ACPI_SIG_SSDT 0x54445353UL
#define ACPI_SIG_FADT 0x50434146UL
#define ACPI_SIG_MADT 0x43495041UL
#define ACPI_SIG_MCFG 0x4746434DUL

typedef struct TYKID_PACKED {
    u32  signature;
    u32  length;
    u8   revision;
    u8   checksum;
    u8   oem_id[6];
    u8   oem_table_id[8];
    u32  oem_revision;
    u32  creator_id;
    u32  creator_revision;
} ty_acpi_header_t;

typedef struct TYKID_PACKED {
 u64 base_address;
    u16  segment_group;
    u8   start_bus;
    u8   end_bus;
    u32  _reserved;
} ty_mcfg_entry_t;

typedef struct {
 u32 eisaid;
    tykid_hwclass_t  ty_class;
    const char      *desc;
} ty_acpi_hid_entry_t;

static const ty_acpi_hid_entry_t TY_ACPI_HID_TABLE[] = {

    { 0x030AD041U, TYKID_HW_UNKNOWN,       "PCI Root Bus"                  },

    { 0x080AD041U, TYKID_HW_UNKNOWN,       "PCIe Root Bus"                 },

    { 0xA033EC17U, TYKID_HW_SMBUS,         "Intel Smart Connect"           },

    { 0x140F8608U, TYKID_HW_STORAGE_SATA,  "Intel BayTrail SD"             },

    { 0x030000ECU, TYKID_HW_THERMAL,       "ACPI AC Adapter"               },

    { 0x070000ECU, TYKID_HW_UNKNOWN,       "ACPI CPU"                      },

    { 0x0C0000ECU, TYKID_HW_UNKNOWN,       "ACPI TAD"                      },

    { 0x0AC00D41U, TYKID_HW_THERMAL,       "ACPI Battery"                  },

    { 0x0BC00D41U, TYKID_HW_THERMAL,       "ACPI Fan"                      },

    { 0x090CC041U, TYKID_HW_SMBUS,         "ACPI EC"                       },

    { 0x9910C817U, TYKID_HW_INPUT_KBD,     "Intel GPIO"                    },

    { 0x01014D53U, TYKID_HW_UNKNOWN,       "TPM 2.0"                       },
};
#define TY_ACPI_HID_TABLE_SZ \
    (sizeof(TY_ACPI_HID_TABLE)/sizeof(TY_ACPI_HID_TABLE[0]))

typedef struct {
    ty_acpi_header_t  *ptr;
    usz                mapped_len;
} ty_acpi_table_ref_t;

extern tykid_status_t kobalt_acpi_get_table(u32 sig, u32 index,
                                              ty_acpi_table_ref_t *out);
extern tykid_status_t kobalt_acpi_eval_hid(const char *acpi_path,
                                             u32 *eisaid_out,
                                             char *str_hid_out, usz str_cap);
extern tykid_status_t kobalt_acpi_pci_slot(const char *acpi_path,
                                             u8 *bus, u8 *slot, u8 *func);
extern void kobalt_acpi_release_table(ty_acpi_table_ref_t *ref);

static TYKID_PURE bool8
ty_acpi_checksum_ok(const ty_acpi_header_t *hdr)
{
    if (!hdr || hdr->length < sizeof(ty_acpi_header_t)) return TYKID_FALSE;
    const u8 *p = (const u8 *)hdr;
    u8 sum = 0;
    for (u32 i = 0; i < hdr->length; i++) sum += p[i];
    return (sum == 0) ? TYKID_TRUE : TYKID_FALSE;
}

typedef struct {
 u64 acpi_table_hash;
 u32 validated_tables;
 u32 failed_tables;
 u32 pci_acpi_mismatches;
 u32 acpi_pci_mismatches;
    bool8 mcfg_valid;
    u8   mcfg_segment_count;
} ty_acpi_audit_t;

typedef struct {
    ty_acpi_audit_t audit;
 u64 mcfg_bases[8];
    u8              mcfg_base_count;
    bool8           validated;
} ty_acpi_ctx_t;

static void
ty_acpi_hash_table(tykid_gate_ctx_t *ctx,
                   ty_acpi_ctx_t *ac,
                   const ty_acpi_header_t *hdr)
{
    if (!ty_acpi_checksum_ok(hdr)) {
        ac->audit.failed_tables++;

        ac->audit.acpi_table_hash ^= 0xDEADC0DE13374B42ULL
                                   ^ (u64)hdr->signature;
        TY_LOG(ctx, TY_LOG_ERROR,
               "ACPI table '%c%c%c%c' FAILED checksum — topology poisoned",
               (char)(hdr->signature & 0xFF),
               (char)((hdr->signature >> 8) & 0xFF),
               (char)((hdr->signature >> 16) & 0xFF),
               (char)((hdr->signature >> 24) & 0xFF));
        return;
    }

    u64 h = ty_siphash24(ctx->session_key, hdr, hdr->length);
    ac->audit.acpi_table_hash = ty_rotl64(ac->audit.acpi_table_hash ^ h, 27);

    ac->audit.validated_tables++;
}

static tykid_status_t
ty_acpi_parse_mcfg(tykid_gate_ctx_t *ctx, ty_acpi_ctx_t *ac)
{
    ty_acpi_table_ref_t ref;
    tykid_status_t st = kobalt_acpi_get_table(ACPI_SIG_MCFG, 0, &ref);
    if (st != TYKID_OK) {
        TY_LOG(ctx, TY_LOG_WARN, "ACPI MCFG not found — PCIe base unverified");
 return TYKID_OK;
    }

    ty_acpi_hash_table(ctx, ac, ref.ptr);

    const u8 *body = (const u8 *)ref.ptr + 44;
    u32 body_len   = ref.ptr->length > 44 ? ref.ptr->length - 44 : 0;
    u32 entry_count = body_len / sizeof(ty_mcfg_entry_t);
    if (entry_count > 8) entry_count = 8;

    for (u32 i = 0; i < entry_count; i++) {
        const ty_mcfg_entry_t *e = (const ty_mcfg_entry_t *)(body + i * sizeof(*e));
        if (i < 8) {
            ac->mcfg_bases[ac->mcfg_base_count++] = e->base_address;
        }
        TY_LOG(ctx, TY_LOG_DEBUG,
               "MCFG[%u]: base=%016llx seg=%u bus=%u..%u",
               i, (unsigned long long)e->base_address,
               e->segment_group, e->start_bus, e->end_bus);
    }

    ac->audit.mcfg_valid    = TYKID_TRUE;
    ac->audit.mcfg_segment_count = (u8)entry_count;
    kobalt_acpi_release_table(&ref);
    return TYKID_OK;
}

static void
ty_acpi_crosscheck_device(tykid_gate_ctx_t *ctx,
                           ty_acpi_ctx_t *ac,
                           tykid_hw_device_t *dev)
{
    u8 acpi_bus, acpi_slot, acpi_func;

    char acpi_path[64];

    extern tykid_status_t kobalt_acpi_path_for_bdf(u8 bus, u8 slot, u8 func,
                                                     char *out, usz cap);
    tykid_status_t st = kobalt_acpi_path_for_bdf(dev->bus, dev->slot, dev->func,
                                                   acpi_path, sizeof(acpi_path));
    if (st != TYKID_OK) {

        bool8 bus_managed = TYKID_FALSE;
        for (u8 i = 0; i < ac->mcfg_base_count; i++) {

            if (ac->audit.mcfg_valid) {
                bus_managed = TYKID_TRUE;
                break;
            }
        }

        if (bus_managed) {
            TY_LOG(ctx, TY_LOG_WARN,
                   "PCI %02x:%02x.%x ('%s') has NO ACPI object on managed bus — SUSPICIOUS",
                   dev->bus, dev->slot, dev->func, dev->name);

            dev->hardware_fingerprint ^= 0xBADACCE55DEADULL;
            ac->audit.pci_acpi_mismatches++;
        }
        return;
    }

    st = kobalt_acpi_pci_slot(acpi_path, &acpi_bus, &acpi_slot, &acpi_func);
    if (st == TYKID_OK) {
        if (acpi_bus != dev->bus || acpi_slot != dev->slot || acpi_func != dev->func) {
            TY_LOG(ctx, TY_LOG_ERROR,
                   "ACPI/PCI COORDINATE MISMATCH: PCI=%02x:%02x.%x ACPI=%02x:%02x.%x "
                   "device='%s' — blacklisting",
                   dev->bus, dev->slot, dev->func,
                   acpi_bus, acpi_slot, acpi_func,
                   dev->name);
            dev->hardware_fingerprint ^= 0xCAFEBABEDEADBEEFULL;
            ac->audit.pci_acpi_mismatches++;
        }
    }
}

static tykid_status_t
ty_acpi_sweep_tables(tykid_gate_ctx_t *ctx, ty_acpi_ctx_t *ac)
{
    static const u32 critical_sigs[] = {
        ACPI_SIG_DSDT, ACPI_SIG_FADT, ACPI_SIG_MADT, ACPI_SIG_MCFG
    };
    static const char * const sig_names[] = { "DSDT", "FACP", "APIC", "MCFG" };

    for (u32 i = 0; i < sizeof(critical_sigs)/sizeof(critical_sigs[0]); i++) {
        ty_acpi_table_ref_t ref;
        tykid_status_t st = kobalt_acpi_get_table(critical_sigs[i], 0, &ref);
        if (st != TYKID_OK) {
            if (critical_sigs[i] == ACPI_SIG_DSDT || critical_sigs[i] == ACPI_SIG_FADT) {
                TY_LOG(ctx, TY_LOG_ERROR, "Critical ACPI table '%s' MISSING",
                       sig_names[i]);
                ac->audit.failed_tables++;

                ac->audit.acpi_table_hash ^= (u64)critical_sigs[i] * 0x9E3779B97F4A7C15ULL;
            } else {
                TY_LOG(ctx, TY_LOG_WARN, "ACPI table '%s' not present (non-fatal)",
                       sig_names[i]);
            }
            continue;
        }
        ty_acpi_hash_table(ctx, ac, ref.ptr);
        TY_LOG(ctx, TY_LOG_DEBUG, "ACPI '%s': length=%u rev=%u chk=%s",
               sig_names[i], ref.ptr->length, ref.ptr->revision,
               ty_acpi_checksum_ok(ref.ptr) ? "OK" : "FAIL");
        kobalt_acpi_release_table(&ref);
    }

    for (u32 idx = 0; idx < 16; idx++) {
        ty_acpi_table_ref_t ref;
        if (kobalt_acpi_get_table(ACPI_SIG_SSDT, idx, &ref) != TYKID_OK) break;
        ty_acpi_hash_table(ctx, ac, ref.ptr);
        kobalt_acpi_release_table(&ref);
    }

    return TYKID_OK;
}

TYKID_INTERNAL tykid_status_t
tykid_acpi_validate(tykid_gate_ctx_t *ctx, tykid_hw_enumset_t *hw)
{
    if (!ctx || !hw) return TYKID_ERR_GENERIC;

    ty_acpi_ctx_t ac;
    ty_memzero_secure(&ac, sizeof(ac));

    TY_LOG(ctx, TY_LOG_INFO, "ACPI cross-validation starting");

    ty_acpi_sweep_tables(ctx, &ac);

    ty_acpi_parse_mcfg(ctx, &ac);

    for (u32 i = 0; i < hw->count; i++) {
        ty_acpi_crosscheck_device(ctx, &ac, &hw->devices[i]);
    }

    hw->bus_topology_hash ^= ty_rotl64(ac.audit.acpi_table_hash, 13);

    ac.validated = TYKID_TRUE;

    TY_LOG(ctx, TY_LOG_INFO,
           "ACPI validation complete: valid_tables=%u failed=%u "
           "pci_acpi_mismatches=%u mcfg=%s",
           ac.audit.validated_tables, ac.audit.failed_tables,
           ac.audit.pci_acpi_mismatches,
           ac.audit.mcfg_valid ? "YES" : "NO");

    if (ac.audit.failed_tables > 0) {
        TY_LOG(ctx, TY_LOG_WARN,
               "%u ACPI tables failed checksum — topology hash poisoned",
               ac.audit.failed_tables);
    }
    if (ac.audit.pci_acpi_mismatches > 0) {
        TY_LOG(ctx, TY_LOG_WARN,
               "%u PCI devices have no valid ACPI counterpart",
               ac.audit.pci_acpi_mismatches);
    }

    ty_memzero_secure(&ac, sizeof(ac));

    return ac.audit.validated_tables == 0 ? TYKID_ERR_HW_ENUM : TYKID_OK;
}

TYKID_INTERNAL tykid_hwclass_t
tykid_acpi_hid_to_class(u32 eisaid)
{
    for (usz i = 0; i < TY_ACPI_HID_TABLE_SZ; i++) {
        if (TY_ACPI_HID_TABLE[i].eisaid == eisaid)
            return TY_ACPI_HID_TABLE[i].ty_class;
    }
    return TYKID_HW_UNKNOWN;
}
