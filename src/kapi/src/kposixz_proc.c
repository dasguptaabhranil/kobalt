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

#include "kposixz_internal.h"

static volatile s32 kpz_next_pid = 1;

kpz_pid_t kpz_proc_next_pid(void)
{
    return (kpz_pid_t)kpz_atomic_inc(&kpz_next_pid);
}

static kposixz_proc_t kpz_proc_pool[KPOSIXZ_MAX_PROCS] KPZ_ALIGNED(64);
static u8             kpz_proc_pool_used[KPOSIXZ_MAX_PROCS];
static kpz_spinlock_t kpz_proc_pool_lock = KPZ_SPINLOCK_INIT;

kposixz_proc_t *kpz_proc_alloc(kpz_pid_t ppid)
{
    kpz_spin_lock(&kpz_proc_pool_lock);

    kposixz_proc_t *proc = (void *)0;
    for (s32 i = 1; i < KPOSIXZ_MAX_PROCS; i++) {
        if (!kpz_proc_pool_used[i]) {
            kpz_proc_pool_used[i] = 1;
            proc = &kpz_proc_pool[i];
            break;
        }
    }

    kpz_spin_unlock(&kpz_proc_pool_lock);

    if (!proc) return (void *)0;

    kpz_memzero(proc, sizeof(*proc));

    proc->pid      = kpz_proc_next_pid();
    proc->ppid     = ppid;
    proc->uid      = 0;
    proc->gid      = 0;
    proc->state    = KPZ_PROC_EMBRYO;
    proc->refcount = 1;
    proc->start_ns = kobalt_acpi_timer_ns();
    kpz_strncpy(proc->cwd, "/", sizeof(proc->cwd));

    proc->kstack = (u8 *)kmalloc(KPOSIXZ_KERNEL_STACK);
    if (!proc->kstack) {

        kpz_spin_lock(&kpz_proc_pool_lock);
        kpz_proc_pool_used[proc - kpz_proc_pool] = 0;
        kpz_spin_unlock(&kpz_proc_pool_lock);
        return (void *)0;
    }

    proc->kstack_top = (uptr)(proc->kstack + KPOSIXZ_KERNEL_STACK - 8);

    kpz_spin_lock(&kpz_proc_table_lock);
    if (proc->pid < KPOSIXZ_MAX_PROCS)
        kpz_proc_table[proc->pid] = proc;
    kpz_spin_unlock(&kpz_proc_table_lock);

    return proc;
}

kposixz_proc_t *kpz_proc_lookup(kpz_pid_t pid)
{
    if (pid <= 0 || pid >= KPOSIXZ_MAX_PROCS) return (void *)0;
    return kpz_atomic_load(&kpz_proc_table[pid]);
}

void kpz_proc_free(kposixz_proc_t *proc)
{
    if (!proc) return;

    kpz_spin_lock(&kpz_proc_table_lock);
    if (proc->pid < KPOSIXZ_MAX_PROCS)
        kpz_proc_table[proc->pid] = (void *)0;
    kpz_spin_unlock(&kpz_proc_table_lock);

    kpz_spin_lock(&proc->fds.lock);
    for (s32 i = 0; i < KPOSIXZ_MAX_FD; i++) {
        if (proc->fds.files[i]) {
            kpz_fd_put(proc->fds.files[i]);
            proc->fds.files[i] = (void *)0;
        }
    }
    kpz_spin_unlock(&proc->fds.lock);

    if (proc->kstack) {
        kfree(proc->kstack);
        proc->kstack = (void *)0;
    }

    kpz_spin_lock(&kpz_proc_pool_lock);
    s32 idx = (s32)(proc - kpz_proc_pool);
    if (idx >= 0 && idx < KPOSIXZ_MAX_PROCS)
        kpz_proc_pool_used[idx] = 0;
    kpz_spin_unlock(&kpz_proc_pool_lock);
}

KPZ_NORETURN void kpz_proc_exit(kposixz_proc_t *proc, s32 code)
{
    proc->exit_code = code;
    kpz_atomic_store(&proc->state, (u8)KPZ_PROC_ZOMBIE);

    kpz_set_current((void *)0);

    kpz_proc_free(proc);

    kobalt_sched_yield();

    __asm__ volatile("cli\n1: hlt\n jmp 1b" ::: "memory");
    __builtin_unreachable();
}

#define ELF_MAG     "\x7f""ELF"
#define ET_EXEC     2
#define ET_DYN      3
#define PT_LOAD     1
#define PT_GNU_STACK 0x6474e551
#define EM_X86_64   62
#define ELFCLASS64  2
#define ELFDATA2LSB 1

typedef struct KPZ_PACKED {
    u8  e_ident[16];
    u16 e_type, e_machine;
    u32 e_version;
    u64 e_entry, e_phoff;
    u64 e_shoff;
    u32 e_flags;
    u16 e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Elf64_Ehdr;

typedef struct KPZ_PACKED {
    u32 p_type, p_flags;
    u64 p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
} Elf64_Phdr;

#define PF_X 1
#define PF_W 2
#define PF_R 4

static u32 elf_prot(u32 pflags) {
    u32 p = 0;
    if (pflags & PF_R) p |= KPZ_PROT_READ;
    if (pflags & PF_W) p |= KPZ_PROT_WRITE;
    if (pflags & PF_X) p |= KPZ_PROT_EXEC;
    return p;
}

#define USER_STACK_SIZE  (2U * 1024U * 1024U)
#define USER_STACK_TOP   0x7fff00000000ULL

kpz_pid_t
kposixz_spawn(const char *elf_path,
              const void *elf_image, usz elf_size,
              const char *const argv[],
              const char *const envp[])
{

    const u8 *img  = (const u8 *)elf_image;
    usz        isz = elf_size;

    kposixz_file_t *elf_file = (void *)0;
    if (!img && elf_path) {
        elf_file = kpz_kfs_open(elf_path, KPZ_O_RDONLY);
        if (!elf_file) return (kpz_pid_t)KPZ_ERR(KPZE_NOENT);

        img = (const u8 *)elf_file->priv;

        kpz_stat_t st;
        elf_file->ops->stat(elf_file, &st);
        isz = (usz)st.st_size;
    }

    if (!img || isz < sizeof(Elf64_Ehdr)) {
        if (elf_file) kpz_fd_put(elf_file);
        return (kpz_pid_t)KPZ_ERR(KPZE_NOENT);
    }

    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)img;
    if (kpz_strncmp((const char *)eh->e_ident, ELF_MAG, 4) != 0 ||
        eh->e_ident[4] != ELFCLASS64 ||
        eh->e_ident[5] != ELFDATA2LSB ||
        eh->e_machine   != EM_X86_64  ||
        (eh->e_type != ET_EXEC && eh->e_type != ET_DYN) ||
        eh->e_phentsize != sizeof(Elf64_Phdr)) {
        if (elf_file) kpz_fd_put(elf_file);
        return (kpz_pid_t)KPZ_ERR(KPZE_NOEXEC);
    }

    kposixz_proc_t *proc = kpz_proc_alloc(0 );
    if (!proc) {
        if (elf_file) kpz_fd_put(elf_file);
        return (kpz_pid_t)KPZ_ERR(KPZE_NOMEM);
    }

    uptr load_base = 0;
    uptr brk_end   = 0;

    const Elf64_Phdr *ph = (const Elf64_Phdr *)(img + eh->e_phoff);
    for (u16 i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD) continue;

        uptr seg_vaddr  = (uptr)ph[i].p_vaddr;
        uptr seg_end    = seg_vaddr + (uptr)ph[i].p_memsz;
        uptr page_start = seg_vaddr & ~0xFFFULL;
        uptr page_end   = (seg_end   + 0xFFFULL) & ~0xFFFULL;
        usz  map_sz     = page_end - page_start;

        u32 prot = elf_prot(ph[i].p_flags);

        uptr mapped = kobalt_vmm_alloc(page_start, map_sz,
                                        KPZ_PROT_READ | KPZ_PROT_WRITE);
        if (!mapped) {
            kpz_proc_free(proc);
            if (elf_file) kpz_fd_put(elf_file);
            return (kpz_pid_t)KPZ_ERR(KPZE_NOMEM);
        }

        kpz_memzero((void *)mapped, map_sz);
        if (ph[i].p_filesz)
            kpz_memcpy((void *)seg_vaddr,
                       img + ph[i].p_offset,
                       (usz)ph[i].p_filesz);

        if (prot != (KPZ_PROT_READ | KPZ_PROT_WRITE))
            kobalt_vmm_protect(mapped, map_sz, prot);

        if (!load_base || page_start < load_base) load_base = page_start;
        if (seg_end > brk_end) brk_end = seg_end;
    }

    proc->mm_base      = load_base;
    proc->mm_brk_start = (brk_end + 0xFFFULL) & ~0xFFFULL;
    proc->mm_brk       = proc->mm_brk_start;

    uptr stack_base = USER_STACK_TOP - USER_STACK_SIZE;
    uptr stack_map  = kobalt_vmm_alloc(stack_base, USER_STACK_SIZE,
                                        KPZ_PROT_READ | KPZ_PROT_WRITE);
    if (!stack_map) {
        kpz_proc_free(proc);
        if (elf_file) kpz_fd_put(elf_file);
        return (kpz_pid_t)KPZ_ERR(KPZE_NOMEM);
    }
    kpz_memzero((void *)stack_base, USER_STACK_SIZE);
    proc->stack_top = USER_STACK_TOP;

    u64 *sp = (u64 *)(USER_STACK_TOP - 8);

    s32 argc = 0, envc = 0;
    if (argv) while (argv[argc]) argc++;
    if (envp) while (envp[envc]) envc++;

    u64 argv_ptrs[64], envp_ptrs[64];
    char *str_ptr = (char *)(USER_STACK_TOP - 8);

    for (s32 i = envc - 1; i >= 0; i--) {
        usz slen = kpz_strlen(envp[i]) + 1;
        str_ptr -= slen;
        kpz_memcpy(str_ptr, envp[i], slen);
        envp_ptrs[i] = (u64)(uptr)str_ptr;
    }

    for (s32 i = argc - 1; i >= 0; i--) {
        usz slen = kpz_strlen(argv[i]) + 1;
        str_ptr -= slen;
        kpz_memcpy(str_ptr, argv[i], slen);
        argv_ptrs[i] = (u64)(uptr)str_ptr;
    }

    sp = (u64 *)((uptr)str_ptr & ~0xFULL);

    #define AT_NULL   0ULL
    #define AT_PAGESZ 6ULL
    #define AT_ENTRY  9ULL
    *(--sp) = 0;
    *(--sp) = AT_NULL;
    *(--sp) = (u64)eh->e_entry;
    *(--sp) = AT_ENTRY;
    *(--sp) = 4096ULL;
    *(--sp) = AT_PAGESZ;

    *(--sp) = 0;
    for (s32 i = envc - 1; i >= 0; i--) *(--sp) = envp_ptrs[i];

    *(--sp) = 0;
    for (s32 i = argc - 1; i >= 0; i--) *(--sp) = argv_ptrs[i];

    *(--sp) = (u64)argc;

    proc->stack_top = (uptr)sp;

    kpz_spin_lock(&proc->fds.lock);
    proc->fds.files[0] = kpz_devfs_open_stdin();
    proc->fds.files[1] = kpz_devfs_open_stdout();
    proc->fds.files[2] = kpz_devfs_open_stderr();
    kpz_spin_unlock(&proc->fds.lock);

    kpz_atomic_store(&proc->state, (u8)KPZ_PROC_RUNNING);

    kpz_set_current(proc);

    if (elf_file) kpz_fd_put(elf_file);

    return proc->pid;
}
