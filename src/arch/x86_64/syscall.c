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

#include "syscall.h"
#include "msr.h"
#include "../../inc/kernel.h"
#include "../../inc/percpu.h"

void syscall_init(void)
{

    uint64_t efer = rdmsr(MSR_IA32_EFER);
    efer |= EFER_SCE;
    wrmsr(MSR_IA32_EFER, efer);

    uint64_t star = ((uint64_t)SYSCALL_STAR_USER << 48)
                  | ((uint64_t)SYSCALL_STAR_KERN << 32);
    wrmsr(MSR_STAR, star);

    wrmsr(MSR_LSTAR, (uint64_t)(uintptr_t)syscall_entry);

    wrmsr(MSR_FMASK, SYSCALL_FMASK);

    percpu_t *p = percpu_get();
    wrmsr(MSR_KERNEL_GS_BASE, (uint64_t)(uintptr_t)p);
}

int64_t syscall_dispatch(syscall_frame_t *frame)
{
    (void)frame;
    return -38LL;
}
