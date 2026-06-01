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

static __inline__ __attribute__((always_inline))
uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static __inline__ __attribute__((always_inline))
void wrmsr(uint32_t msr, uint64_t val)
{
    __asm__ volatile ("wrmsr"
        : : "c"(msr), "a"((uint32_t)val), "d"((uint32_t)(val >> 32)));
}

#define MSR_IA32_EFER           UINT32_C(0xC0000080)
#define  EFER_SCE               (1U << 0)
#define  EFER_LME               (1U << 8)
#define  EFER_LMA               (1U << 10)
#define  EFER_NXE               (1U << 11)
#define  EFER_SVME              (1U << 12)

#define MSR_STAR                UINT32_C(0xC0000081)
#define MSR_LSTAR               UINT32_C(0xC0000082)
#define MSR_CSTAR               UINT32_C(0xC0000083)
#define MSR_FMASK               UINT32_C(0xC0000084)

#define MSR_FS_BASE             UINT32_C(0xC0000100)
#define MSR_GS_BASE             UINT32_C(0xC0000101)
#define MSR_KERNEL_GS_BASE      UINT32_C(0xC0000102)
#define MSR_TSC_AUX             UINT32_C(0xC0000103)

#define MSR_IA32_APIC_BASE      UINT32_C(0x0000001B)
#define  APIC_BSP_BIT           (1U << 8)
#define  APIC_X2APIC_EN         (1U << 10)
#define  APIC_GLOBAL_EN         (1U << 11)

#define MSR_IA32_SPEC_CTRL      UINT32_C(0x00000048)
#define  SPEC_CTRL_IBRS         (1U << 0)
#define  SPEC_CTRL_STIBP        (1U << 1)
#define  SPEC_CTRL_SSBD         (1U << 2)

#define MSR_IA32_PRED_CMD       UINT32_C(0x00000049)
#define  PRED_CMD_IBPB          (1U << 0)

#define MSR_IA32_ARCH_CAPS      UINT32_C(0x0000010A)
#define  ARCH_CAPS_RDCL_NO      (1U << 0)
#define  ARCH_CAPS_IBRS_ALL     (1U << 1)
#define  ARCH_CAPS_RSBA         (1U << 2)
#define  ARCH_CAPS_SSB_NO       (1U << 4)
#define  ARCH_CAPS_MDS_NO       (1U << 5)
#define  ARCH_CAPS_TAA_NO       (1U << 8)

#define MSR_IA32_FLUSH_CMD      UINT32_C(0x0000010B)
#define  FLUSH_CMD_L1D          (1U << 0)

#define MSR_IA32_PAT            UINT32_C(0x00000277)

#define MSR_IA32_SYSENTER_CS    UINT32_C(0x00000174)
#define MSR_IA32_SYSENTER_ESP   UINT32_C(0x00000175)
#define MSR_IA32_SYSENTER_EIP   UINT32_C(0x00000176)

#define MSR_IA32_MCG_CAP        UINT32_C(0x00000179)
#define MSR_IA32_MCG_STATUS     UINT32_C(0x0000017A)

#define MSR_IA32_DEBUGCTL       UINT32_C(0x000001D9)
#define  DEBUGCTL_LBR           (1U << 0)
#define  DEBUGCTL_BTF           (1U << 1)
#define  DEBUGCTL_TR            (1U << 6)
#define  DEBUGCTL_BTS           (1U << 7)

#define MSR_IA32_XSS            UINT32_C(0x00000DA0)
#define MSR_IA32_BIOS_SIGN_ID   UINT32_C(0x0000008B)
#define MSR_PLATFORM_INFO       UINT32_C(0x000000CE)
#define MSR_IA32_VMX_BASIC      UINT32_C(0x00000480)
#define MSR_IA32_TSC_DEADLINE   UINT32_C(0x000006E0)

#define MSR_IA32_ENERGY_PERF_BIAS UINT32_C(0x000001B0)
#define MSR_IA32_PERF_GLOBAL_CTRL UINT32_C(0x0000038F)
#define MSR_IA32_FIXED_CTR_CTRL   UINT32_C(0x0000038D)
