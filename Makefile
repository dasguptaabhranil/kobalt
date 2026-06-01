# Copyright (C) 2026 Abhranil Dasgupta
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

CC       := clang
LD       := ld.lld
AS       := nasm
READELF  := readelf
HOST_CC  := gcc

BIN      := kobalt.elf
KFS_IMG  := kobalt.kfs
BLK_IMG  := kobalt_disk.img
SRC_DIR  := src
BUILD_DIR := build
TOOLS_DIR := tools

LWIP_PATH := $(SRC_DIR)/net/lwip
ifeq ($(wildcard $(LWIP_PATH)/src/core),)
 LWIP_BASE := $(LWIP_PATH)
else
 LWIP_BASE := $(LWIP_PATH)/src
endif

LWIP_INC  := $(LWIP_BASE)/include
LWIP_ARCH := $(SRC_DIR)/net/arch

CLANG_RESOURCE_DIR := $(shell $(CC) -print-resource-dir)
INTERNAL_INC := -isystem $(CLANG_RESOURCE_DIR)/include

BEARSSL_DIR      := $(SRC_DIR)/security/bearssl
BEARSSL_SRC_DIR  := $(BEARSSL_DIR)/src
BEARSSL_INC_DIR  := $(BEARSSL_DIR)/inc
BEARSSL_PORT_DIR := $(BEARSSL_DIR)/port

BEARSSL_SRCS := $(shell find $(BEARSSL_SRC_DIR) -name "*.c" 2>/dev/null \
 | grep -v 'sysrng\.c') \
 $(wildcard $(BEARSSL_PORT_DIR)/br_entropy.c)
BEARSSL_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(BEARSSL_SRCS))

BEARSSL_CFLAGS := -target x86_64-pc-elf -std=c11 -O2 -ffreestanding \
 -nostdinc $(INTERNAL_INC) \
 -fno-stack-protector -fno-pic -fno-pie -mcmodel=large \
 -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
 -I$(BEARSSL_INC_DIR) \
 -I$(BEARSSL_SRC_DIR) \
 -I$(SRC_DIR)/inc \
 -include $(abspath $(BEARSSL_PORT_DIR)/br_port.h) \
 -w -MMD

KOBALT_KERNEL_IDENT ?= 0xA3F7C219DE40B851ULL

TYKID_DIR     := $(SRC_DIR)/security/tykid
TYKID_INC_DIR := $(TYKID_DIR)/inc
TYKID_SRC_DIR := $(TYKID_DIR)/src

TYKID_SRCS := \
 $(TYKID_SRC_DIR)/tykid_core.c \
 $(TYKID_SRC_DIR)/tykid_hw.c \
 $(TYKID_SRC_DIR)/tykid_gate.c \
 $(TYKID_SRC_DIR)/tykid_registry.c \
 $(TYKID_SRC_DIR)/tykid_acpi.c \
 $(TYKID_SRC_DIR)/tykid_attest.c \
 $(TYKID_SRC_DIR)/tykid_audit.c \
 $(TYKID_SRC_DIR)/tykid_fuzz.c \
 $(TYKID_SRC_DIR)/tykid_iommu.c \
 $(TYKID_SRC_DIR)/tykid_policy.c \
 $(TYKID_SRC_DIR)/tykid_sandbox.c \
 $(TYKID_SRC_DIR)/tykid_watchdog.c \
 $(TYKID_SRC_DIR)/tykid_crypto.c \
 $(TYKID_SRC_DIR)/tykid_selector.c \
 $(TYKID_SRC_DIR)/tykid_threat.c \
 $(TYKID_SRC_DIR)/tykid_usb.c \
 $(TYKID_SRC_DIR)/tykid_kobalt_glue.c \
 $(TYKID_SRC_DIR)/platform.c

TYKID_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(TYKID_SRCS))

TYKID_CFLAGS := -target x86_64-pc-elf -std=c11 -O2 -ffreestanding \
 -nostdinc $(INTERNAL_INC) \
 -fno-stack-protector -fno-pic -fno-pie -mcmodel=large \
 -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
 -fno-common -fno-exceptions \
 -I$(TYKID_INC_DIR) \
 -I$(TYKID_SRC_DIR) \
 -I$(SRC_DIR)/inc \
 -I$(SRC_DIR)/fs \
 -I$(SRC_DIR)/fs/kfs \
 -I$(SRC_DIR)/fs/vfs \
 -I$(BEARSSL_INC_DIR) \
 -I$(SRC_DIR)/drivers/usb/inc \
 -D__KERNEL__ \
 -DKOBALT_KERNEL_IDENT=$(KOBALT_KERNEL_IDENT) \
 -w -MMD

FATFS_DIR := $(SRC_DIR)/fs/fatfs

FATFS_SRCS := \
	$(FATFS_DIR)/ff.c \
	$(FATFS_DIR)/ffunicode.c \
	$(FATFS_DIR)/diskio.c \
	$(FATFS_DIR)/fatfs_kobalt.c

FATFS_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(FATFS_SRCS))

FATFS_CFLAGS := -target x86_64-pc-elf -std=c11 -O2 -ffreestanding \
	-nostdinc $(INTERNAL_INC) \
	-fno-stack-protector -fno-pic -fno-pie -mcmodel=large \
	-mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
	-fno-common -fno-exceptions \
	-I$(FATFS_DIR) \
	-I$(SRC_DIR)/inc \
	-D__KERNEL__ \
	-w -MMD

IXGBE_DIR  := $(SRC_DIR)/drivers/net/ixgbe

IXGBE_SRCS := \
	$(IXGBE_DIR)/ixgbe_api.c \
	$(IXGBE_DIR)/ixgbe_common.c \
	$(IXGBE_DIR)/ixgbe_mbx.c \
	$(IXGBE_DIR)/ixgbe_phy.c \
	$(IXGBE_DIR)/ixgbe_82598.c \
	$(IXGBE_DIR)/ixgbe_82599.c \
	$(IXGBE_DIR)/ixgbe_x540.c \
	$(IXGBE_DIR)/ixgbe_x550.c \
	$(IXGBE_DIR)/ixgbe_e610.c \
	$(IXGBE_DIR)/ixgbe_osdep.c \
	$(IXGBE_DIR)/kobalt_ixgbe.c

IXGBE_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(IXGBE_SRCS))

IXGBE_CFLAGS := -target x86_64-pc-elf -std=c11 -O2 -ffreestanding \
	-nostdinc $(INTERNAL_INC) \
	-fno-stack-protector -fno-pic -fno-pie -mcmodel=large \
	-mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
	-fno-common -fno-exceptions \
	-I$(IXGBE_DIR) \
	-I$(SRC_DIR)/inc \
	-D__KERNEL__ \
	-w -MMD

IGC_DIR  := $(SRC_DIR)/drivers/net/igc

IGC_SRCS := \
	$(IGC_DIR)/igc_api.c \
	$(IGC_DIR)/igc_base.c \
	$(IGC_DIR)/igc_i225.c \
	$(IGC_DIR)/igc_mac.c \
	$(IGC_DIR)/igc_nvm.c \
	$(IGC_DIR)/igc_osdep.c \
	$(IGC_DIR)/igc_phy.c \
	$(IGC_DIR)/kobalt_igc.c

IGC_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(IGC_SRCS))

IGC_CFLAGS := -target x86_64-pc-elf -std=c11 -O2 -ffreestanding \
	-nostdinc $(INTERNAL_INC) \
	-fno-stack-protector -fno-pic -fno-pie -mcmodel=large \
	-mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
	-fno-common -fno-exceptions \
	-I$(IGC_DIR) \
	-I$(SRC_DIR)/inc \
	-D__KERNEL__ \
	-w -MMD

AMX_INC_DIR  := $(SRC_DIR)/arch/x86_64/amx

KAPI_DIR     := $(SRC_DIR)/kapi
KAPI_INC_DIR := $(KAPI_DIR)/inc
KAPI_SRC_DIR := $(KAPI_DIR)/src

KAPI_SRCS_C := \
 $(KAPI_SRC_DIR)/kposixz_syscall.c \
 $(KAPI_SRC_DIR)/kposixz_proc.c \
 $(KAPI_SRC_DIR)/kposixz_fd.c \
 $(KAPI_SRC_DIR)/kposixz_kobalt_glue.c \
 $(KAPI_SRC_DIR)/kposixz_net.c \
 $(KAPI_SRC_DIR)/kposixz_io.c \
 $(KAPI_SRC_DIR)/kposixz_adv.c \
 $(KAPI_SRC_DIR)/kposixz_amx.c

KAPI_SRCS_S := \
 $(KAPI_SRC_DIR)/kposixz_entry.S

KAPI_OBJS := \
 $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(KAPI_SRCS_C)) \
 $(patsubst $(SRC_DIR)/%.S,$(BUILD_DIR)/%.o,$(KAPI_SRCS_S))

KAPI_CFLAGS := -target x86_64-pc-elf -std=c11 -O2 -ffreestanding \
 -nostdinc $(INTERNAL_INC) \
 -fno-stack-protector -fno-pic -fno-pie -mcmodel=large \
 -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
 -fno-omit-frame-pointer \
 -fno-common -fno-exceptions \
 -I$(KAPI_INC_DIR) \
 -I$(KAPI_SRC_DIR) \
 -I$(SRC_DIR)/inc \
 -I$(SRC_DIR)/fs \
 -I$(SRC_DIR)/fs/kfs \
 -I$(SRC_DIR)/fs/vfs \
 -I$(SRC_DIR)/net \
 -I$(LWIP_INC) \
 -I$(LWIP_ARCH) \
 -I$(TYKID_INC_DIR) \
 -I$(AMX_INC_DIR) \
 -D__KERNEL__ \
 -DKOBALT_KERNEL_IDENT=$(KOBALT_KERNEL_IDENT) \
 -w -MMD

UACPI_DIR      := $(SRC_DIR)/kern/acpi/uacpi
UACPI_INC_DIR  := $(UACPI_DIR)/include
UACPI_SRC_DIR  := $(UACPI_DIR)/source

UACPI_SRCS := $(wildcard $(UACPI_SRC_DIR)/*.c)
UACPI_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(UACPI_SRCS))

UACPI_CFLAGS := -target x86_64-pc-elf -std=c11 -O2 -ffreestanding \
 -nostdinc $(INTERNAL_INC) \
 -fno-stack-protector -fno-pic -fno-pie -mcmodel=large \
 -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
 -fno-common \
 -I$(UACPI_INC_DIR) \
 -I$(SRC_DIR)/inc \
 -D__KERNEL__ \
 -w -MMD

CFLAGS := -target x86_64-pc-elf -std=c11 -O3 -ffreestanding \
 -nostdinc $(INTERNAL_INC) \
 -fno-stack-protector -fno-pic -fno-pie -mcmodel=large \
 -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
 -fno-omit-frame-pointer \
 -Wall -Wextra -Werror -Wno-unused-parameter \
 -I$(SRC_DIR)/inc \
 -I$(SRC_DIR)/arch/x86_64 \
 -I$(AMX_INC_DIR) \
 -I$(SRC_DIR)/net \
 -I$(LWIP_INC) \
 -I$(LWIP_ARCH) \
 -I$(BEARSSL_INC_DIR) \
 -I$(BEARSSL_PORT_DIR) \
 -I$(TYKID_INC_DIR) \
 -I$(TYKID_DIR) \
 -I$(KAPI_INC_DIR) \
 -I$(SRC_DIR)/fs \
 -I$(SRC_DIR)/fs/kfs \
 -I$(SRC_DIR)/fs/flatfs \
 -I$(SRC_DIR)/fs/tmpfs \
 -I$(SRC_DIR)/fs/fatfs \
 -I$(SRC_DIR)/fs/devfs \
 -I$(SRC_DIR)/fs/sysfs \
 -I$(SRC_DIR)/fs/vfs \
 -I$(SRC_DIR)/sound \
 -I$(SRC_DIR)/drivers/usb/inc \
 -I$(SRC_DIR)/drivers/net/e1000 \
 -I$(SRC_DIR)/drivers/net/igc \
 -I$(SRC_DIR)/drivers/net/ixgbe \
 -I$(SRC_DIR)/drivers/usb/ehci \
 -I$(SRC_DIR)/drivers/vga \
 -I$(SRC_DIR)/drivers/ahci \
 -I$(SRC_DIR)/drivers/nvme \
 -I$(SRC_DIR)/drivers/ps2 \
 -I$(SRC_DIR)/drivers/virtio/storage \
 -I$(UACPI_INC_DIR) \
 -D__KERNEL__ \
 -DKOBALT_KERNEL_IDENT=$(KOBALT_KERNEL_IDENT) \
 -MMD

ASFLAGS := -f elf64
LDFLAGS := -T linker.ld --nostdlib --static --gc-sections \
 -z max-page-size=0x200000 -z separate-loadable-segments \
 --no-rosegment --build-id=none

SMP_CPU_COUNT ?= 4

SRCS_C := $(shell find $(SRC_DIR) -name "*.c" \
 -not -path "$(SRC_DIR)/net/lwip/*" \
 -not -path "$(SRC_DIR)/security/bearssl/src/*" \
 -not -path "$(SRC_DIR)/security/bearssl/port/br_entropy.c" \
 -not -path "$(SRC_DIR)/security/tykid/*" \
 -not -path "$(SRC_DIR)/kapi/*" \
 -not -path "$(SRC_DIR)/fs/kfs/*" \
 -not -path "$(SRC_DIR)/fs/vfs/*" \
 -not -path "$(SRC_DIR)/fs/tmpfs/*" \
 -not -path "$(SRC_DIR)/fs/flatfs/*" \
 -not -path "$(SRC_DIR)/fs/fatfs/*" \
 -not -path "$(SRC_DIR)/fs/procfs/*" \
 -not -path "$(SRC_DIR)/fs/devfs/*" \
 -not -path "$(SRC_DIR)/fs/sysfs/*" \
 -not -path "$(SRC_DIR)/sound/*" \
 -not -path "$(SRC_DIR)/arch/x86_64/amx/*" \
 -not -path "$(SRC_DIR)/kern/amx_sched.c" \
 -not -path "$(SRC_DIR)/mm/amx_mem.c" \
 -not -path "$(SRC_DIR)/drivers/net/ixgbe/*" \
 -not -path "$(SRC_DIR)/drivers/net/igc/*" \
 -not -path "$(UACPI_SRC_DIR)/*")
SRCS_C += $(shell find $(LWIP_BASE)/core -name "*.c")
SRCS_C += $(shell find $(LWIP_BASE)/netif -name "*.c")
SRCS_C += $(shell find $(LWIP_BASE)/api -name "*.c")
SRCS_C := $(filter-out %/slipif.c %/ppp.c %/pppos.c %/6lowpan.c %/zepif.c, $(SRCS_C))
SRCS_S := $(shell find $(SRC_DIR) -name "*.S" \
 -not -path "$(SRC_DIR)/kapi/*" \
 -not -path "$(SRC_DIR)/fs/kfs/*" \
 -not -path "$(SRC_DIR)/fs/vfs/*" \
 -not -path "$(SRC_DIR)/sound/*" \
 -not -path "$(SRC_DIR)/arch/x86_64/amx_context.S")

SOUND_DIR  := $(SRC_DIR)/sound
SOUND_OBJS := $(BUILD_DIR)/sound/sound.o \
 $(BUILD_DIR)/sound/intel/hda.o \
 $(BUILD_DIR)/sound/intel/hda_codec.o \
 $(BUILD_DIR)/sound/intel/hda_mixer.o

DEVFS_SRCS := $(filter-out $(SRC_DIR)/fs/devfs/devfs_dir.c $(SRC_DIR)/fs/devfs/devfs_vfs.c,$(wildcard $(SRC_DIR)/fs/devfs/*.c))
DEVFS_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(DEVFS_SRCS))

SYSFS_OBJS := \
 $(BUILD_DIR)/fs/sysfs/sysfs.o \
 $(BUILD_DIR)/fs/sysfs/sysfs_dir.o \
 $(BUILD_DIR)/fs/sysfs/sysfs_file.o \
 $(BUILD_DIR)/fs/sysfs/sysfs_kobalt.o

FS_OBJS := \
 $(BUILD_DIR)/fs/vfs/vfs.o \
 $(BUILD_DIR)/fs/vfs/tmpfs_vfs.o \
 $(BUILD_DIR)/fs/vfs/devfs_vfs.o \
 $(BUILD_DIR)/fs/vfs/procfs_vfs.o \
 $(BUILD_DIR)/fs/vfs/sysfs_vfs.o \
 $(BUILD_DIR)/fs/tmpfs/tmpfs.o \
 $(BUILD_DIR)/fs/tmpfs/tmpfs_inode.o \
 $(BUILD_DIR)/fs/tmpfs/tmpfs_file.o \
 $(BUILD_DIR)/fs/tmpfs/tmpfs_dir.o \
 $(BUILD_DIR)/fs/flatfs/flatfs.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_alloc.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_arbiter.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_btree.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_btree_balance.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_btree_iter.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_btree_node.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_crc.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_dir.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_file.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_freelist.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_handoff.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_hash.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_inline.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_inode.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_inode_cache.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_integrity.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_journal.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_journal_commit.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_journal_recovery.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_journal_tx.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_monitor.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_super.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_kobalt.o \
 $(BUILD_DIR)/fs/flatfs/flatfs_tykid.o \
 $(BUILD_DIR)/fs/vfs/flatfs_vfs.o \
 $(FATFS_OBJS)

PROCFS_OBJS := \
 $(BUILD_DIR)/fs/procfs/procfs.o \
 $(BUILD_DIR)/fs/procfs/procfs_file.o

OBJS := $(sort $(SRCS_C:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o) \
 $(SRCS_S:$(SRC_DIR)/%.S=$(BUILD_DIR)/%.o)) \
 $(BEARSSL_OBJS) \
 $(TYKID_OBJS) \
 $(KAPI_OBJS) \
 $(FS_OBJS) \
 $(PROCFS_OBJS) \
 $(SYSFS_OBJS) \
 $(SOUND_OBJS) \
 $(DEVFS_OBJS) \
 $(UACPI_OBJS) \
 $(IXGBE_OBJS) \
 $(IGC_OBJS)

AMX_OBJS := \
 $(BUILD_DIR)/arch/x86_64/amx/amx_init.o \
 $(BUILD_DIR)/arch/x86_64/amx/amx_tile.o \
 $(BUILD_DIR)/arch/x86_64/amx/amx_state.o \
 $(BUILD_DIR)/arch/x86_64/amx/amx_tmul.o \
 $(BUILD_DIR)/arch/x86_64/amx/amx_bf16.o \
 $(BUILD_DIR)/arch/x86_64/amx/amx_int8.o \
 $(BUILD_DIR)/arch/x86_64/amx/amx_xcr.o \
 $(BUILD_DIR)/arch/x86_64/amx_context.o \
 $(BUILD_DIR)/kern/amx_sched.o \
 $(BUILD_DIR)/mm/amx_mem.o \
 $(BUILD_DIR)/tools/amx_test.o

OBJS += $(AMX_OBJS)
DEPS := $(OBJS:.o=.d)


MKFS_FLATFS := $(TOOLS_DIR)/mkfs_flatfs
FSCK_FLATFS := $(TOOLS_DIR)/fsck_flatfs

BLK_IMG_SIZE_MB := 64

all: $(BIN) $(MKFS_FLATFS) $(FSCK_FLATFS) verify



$(MKFS_FLATFS): $(TOOLS_DIR)/mkfs_flatfs.c
	@mkdir -p $(TOOLS_DIR)
	@echo " [HOST_CC] $<"
	@$(HOST_CC) -O2 -Wall -Wextra -I$(SRC_DIR)/fs/flatfs $< -o $@

$(FSCK_FLATFS): $(TOOLS_DIR)/fsck_flatfs.c
	@mkdir -p $(TOOLS_DIR)
	@echo " [HOST_CC] $<"
	@$(HOST_CC) -O2 -Wall -Wextra -I$(SRC_DIR)/fs/flatfs $< -o $@


$(BLK_IMG):
	@echo " [DD] Creating $@ ($(BLK_IMG_SIZE_MB) MiB raw disk)"
	@dd if=/dev/zero of=$@ bs=1M count=$(BLK_IMG_SIZE_MB) status=none

$(BIN): $(OBJS)
	@echo " [LD] $@"
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD_DIR)/tools/amx_test.o: $(TOOLS_DIR)/amx_test.c
	@mkdir -p $(dir $@)
	@echo " [CC] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/sound/%.o: $(SRC_DIR)/sound/%.c
	@mkdir -p $(dir $@)
	@echo " [CC] $<"
	@$(CC) $(CFLAGS) -I$(SRC_DIR)/sound -c $< -o $@

$(BUILD_DIR)/fs/devfs/%.o: $(SRC_DIR)/fs/devfs/%.c
	@mkdir -p $(dir $@)
	@echo " [CC] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/fs/procfs/%.o: $(SRC_DIR)/fs/procfs/%.c
	@mkdir -p $(dir $@)
	@echo " [CC] $<"
	@$(CC) $(CFLAGS) -I$(SRC_DIR)/fs/procfs -c $< -o $@

$(BUILD_DIR)/fs/sysfs/%.o: $(SRC_DIR)/fs/sysfs/%.c
	@mkdir -p $(dir $@)
	@echo " [CC] $<"
	@$(CC) $(CFLAGS) -I$(SRC_DIR)/fs/sysfs -c $< -o $@

$(BUILD_DIR)/fs/fatfs/%.o: $(SRC_DIR)/fs/fatfs/%.c
	@mkdir -p $(dir $@)
	@echo " [CC] $<"
	@$(CC) $(FATFS_CFLAGS) -c $< -o $@

$(BUILD_DIR)/kern/acpi/uacpi/source/%.o: $(SRC_DIR)/kern/acpi/uacpi/source/%.c
	@mkdir -p $(dir $@)
	@echo " [CC] $<"
	@$(CC) $(UACPI_CFLAGS) -c $< -o $@

$(BUILD_DIR)/arch/x86_64/amx/%.o: $(SRC_DIR)/arch/x86_64/amx/%.c
	@mkdir -p $(dir $@)
	@echo " [CC] $<"
	@$(CC) $(CFLAGS) -I$(SRC_DIR)/arch/x86_64/amx -c $< -o $@

$(BUILD_DIR)/drivers/net/ixgbe/%.o: $(SRC_DIR)/drivers/net/ixgbe/%.c
	@mkdir -p $(dir $@)
	@echo " [CC] $<"
	@$(CC) $(IXGBE_CFLAGS) -c $< -o $@

$(BUILD_DIR)/drivers/net/igc/%.o: $(SRC_DIR)/drivers/net/igc/%.c
	@mkdir -p $(dir $@)
	@echo " [CC] $<"
	@$(CC) $(IGC_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo " [CC] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/security/bearssl/src/%.o: $(SRC_DIR)/security/bearssl/src/%.c
	@mkdir -p $(dir $@)
	@echo " [CC] $<"
	@$(CC) $(BEARSSL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/security/bearssl/port/br_entropy.o: $(SRC_DIR)/security/bearssl/port/br_entropy.c
	@mkdir -p $(dir $@)
	@echo " [CC] $<"
	@$(CC) $(BEARSSL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/security/tykid/src/%.o: $(SRC_DIR)/security/tykid/src/%.c
	@mkdir -p $(dir $@)
	@echo " [CC] $<"
	@$(CC) $(TYKID_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.S
	@mkdir -p $(dir $@)
	@echo " [AS] $<"
	@$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/kapi/src/%.o: $(SRC_DIR)/kapi/src/%.c
	@mkdir -p $(dir $@)
	@echo " [CC] $<"
	@$(CC) $(KAPI_CFLAGS) -c $< -o $@

$(BUILD_DIR)/kapi/src/%.o: $(SRC_DIR)/kapi/src/%.S
	@mkdir -p $(dir $@)
	@echo " [AS] $<"
	@$(CC) $(KAPI_CFLAGS) -c $< -o $@

-include $(DEPS)

verify: $(BIN)
	@$(READELF) -l $(BIN) | grep -q "NOTE" || exit 1

QEMU_COMMON := \
	-machine q35,accel=kvm:tcg,kernel-irqchip=split \
	-device intel-iommu,intremap=on,aw-bits=48 \
	-kernel $(BIN) \
	-m 1G \
	-serial stdio \
	-monitor none \
	-vga std \
	-device qemu-xhci,id=xhci \
	-device intel-hda \
	-device hda-duplex \
	-d int,cpu_reset -D /tmp/qemu_fault.log \
	-no-reboot

QEMU_NET_VIRTIO := \
	-netdev user,id=net0,net=10.0.2.0/24,dhcpstart=10.0.2.15 \
	-device virtio-net-pci,netdev=net0,iommu_platform=on,disable-legacy=on

QEMU_NET_TAP := \
	-netdev tap,id=net0,ifname=tap0,script=no,downscript=no \
	-device virtio-net-pci,netdev=net0,iommu_platform=on,disable-legacy=on

QEMU_NET_TAP_E1000 := \
	-netdev tap,id=net0,ifname=tap0,script=no,downscript=no \
	-device e1000,netdev=net0

QEMU_BLK_VIRTIO := \
	-drive id=blk0,file=$(BLK_IMG),format=raw,if=none \
	-device virtio-blk-pci,drive=blk0

QEMU_SMP      := -smp $(SMP_CPU_COUNT),sockets=1,cores=$(SMP_CPU_COUNT),threads=1
QEMU_SMP_ONE  := -smp 1

run: all $(BLK_IMG)
	qemu-system-x86_64 \
		$(QEMU_COMMON) $(QEMU_SMP) \
		$(QEMU_NET_VIRTIO) \
		$(QEMU_BLK_VIRTIO)

run-debug: all $(BLK_IMG)
	qemu-system-x86_64 \
		$(QEMU_COMMON) $(QEMU_SMP) \
		$(QEMU_NET_VIRTIO) \
		$(QEMU_BLK_VIRTIO) \
		-d int,cpu_reset -D /tmp/qemu.log

run-ahci: all $(BLK_IMG)
	qemu-system-x86_64 \
		$(QEMU_COMMON) $(QEMU_SMP) \
		$(QEMU_NET_VIRTIO) \
		-device ahci,id=ahci0 \
		-drive id=sata0,file=$(BLK_IMG),format=raw,if=none \
		-device ide-hd,drive=sata0,bus=ahci0.0

run-nvme: all $(BLK_IMG)
	qemu-system-x86_64 \
		$(QEMU_COMMON) $(QEMU_SMP) \
		$(QEMU_NET_VIRTIO) \
		-drive id=nvme0,file=$(BLK_IMG),format=raw,if=none \
		-device nvme,drive=nvme0,serial=kobalt-nvme0

run-e1000: all $(BLK_IMG)
	qemu-system-x86_64 \
		$(QEMU_COMMON) $(QEMU_SMP) \
		-netdev user,id=net0 \
		-device e1000,netdev=net0 \
		$(QEMU_BLK_VIRTIO)

run-smp1: all $(BLK_IMG)
	qemu-system-x86_64 \
		$(QEMU_COMMON) $(QEMU_SMP_ONE) \
		$(QEMU_NET_VIRTIO) \
		$(QEMU_BLK_VIRTIO)

run-tap: all $(BLK_IMG)
	qemu-system-x86_64 \
		$(QEMU_COMMON) $(QEMU_SMP) \
		$(QEMU_NET_TAP) \
		$(QEMU_BLK_VIRTIO)

run-tap-e1000: all $(BLK_IMG)
	qemu-system-x86_64 \
		$(QEMU_COMMON) $(QEMU_SMP) \
		$(QEMU_NET_TAP_E1000) \
		$(QEMU_BLK_VIRTIO)

QEMU_NET_TAP_IGC := \
	-netdev tap,id=net0,ifname=tap0,script=no,downscript=no \
	-device e1000e,netdev=net0

run-igc: all $(BLK_IMG)
	@echo "NOTE: QEMU has no I225/I226 (igc) device -- using e1000e as substitute"
	qemu-system-x86_64 \
		$(QEMU_COMMON) $(QEMU_SMP) \
		-netdev user,id=net0 \
		-device e1000e,netdev=net0 \
		$(QEMU_BLK_VIRTIO)

run-tap-igc: all $(BLK_IMG)
	@echo "NOTE: QEMU has no I225/I226 (igc) device -- using e1000e as substitute"
	qemu-system-x86_64 \
		$(QEMU_COMMON) $(QEMU_SMP) \
		$(QEMU_NET_TAP_IGC) \
		$(QEMU_BLK_VIRTIO)

clean:
	@rm -rf $(BUILD_DIR) $(BIN) $(MKFS_FLATFS) $(FSCK_FLATFS) init.cfg

clean-disk:
	@rm -f $(BLK_IMG)
	@echo " [CLEAN] $(BLK_IMG) removed"

.PHONY: all clean clean-disk run run-debug run-ahci run-nvme run-e1000 run-smp1 run-tap run-tap-e1000 run-igc run-tap-igc verify