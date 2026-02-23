# Sonarix-OS (simple Meaty-Skeleton-ish Makefile, no .sh)
# NOTE: Avoid building from a path that contains spaces. GNU make handles spaces poorly.

# -----------------------
# Toolchain (cross)
# -----------------------
CC := i686-elf-gcc
LD := i686-elf-ld
AR := i686-elf-ar

# Fail early (prevents falling back to system cc)
ifeq ($(strip $(shell command -v $(CC) 2>/dev/null)),)
$(error $(CC) not found. Do: export PATH="$$HOME/opt/cross/bin:$$PATH")
endif
ifeq ($(strip $(shell command -v $(LD) 2>/dev/null)),)
$(error $(LD) not found. Do: export PATH="$$HOME/opt/cross/bin:$$PATH")
endif

# -----------------------
# Project dirs / outputs
# -----------------------
BOOT_DIR   := boot
SRC_DIR    := src
LIBC_DIR   := libc

BUILD_DIR  := build
ISO_ROOT   := $(BUILD_DIR)/isodir
ISO_BOOT   := $(ISO_ROOT)/boot
ISO_GRUB   := $(ISO_BOOT)/grub
GRUB_CFG   := $(ISO_GRUB)/grub.cfg

KERNEL_ELF := $(BUILD_DIR)/kernel.elf
ISO_IMAGE  := $(BUILD_DIR)/sonarix.iso
DISK_IMAGE := $(BUILD_DIR)/disk.img

# -----------------------
# Flags
# -----------------------
CFLAGS ?=
CFLAGS += -ffreestanding -m32 -fno-pie -fno-stack-protector -nostdlib \
          -D__is_libk \
          -Wall -Wextra -Werror=implicit-function-declaration \
          -I$(SRC_DIR)/include -I$(LIBC_DIR)/include \
          -MMD -MP

# bootloader.s is GAS/AT&T -> compile with gcc
BOOT_ASFLAGS := -ffreestanding -m32

LDFLAGS := -m elf_i386 -T $(BOOT_DIR)/linker.ld --oformat elf32-i386

QEMU      ?= qemu-system-i386
QEMUFLAGS ?= -no-reboot -no-shutdown -boot d
QEMUDEBUGFLAGS ?= -monitor stdio -d int,cpu_reset -D $(BUILD_DIR)/qemu.log

GRUB_MKRESCUE := $(shell command -v grub-mkrescue 2>/dev/null)
PODMAN        := $(shell command -v podman 2>/dev/null)
ISO_BUILDER   ?= host

# -----------------------
# Sources / objects
# -----------------------
KERNEL_C_SRCS := $(shell find $(SRC_DIR) -type f -name '*.c')
LIBC_C_SRCS   := $(shell find $(LIBC_DIR) -type f -name '*.c')

BOOT_SRC := $(BOOT_DIR)/bootloader.s

KERNEL_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_C_SRCS))
LIBC_OBJS   := $(patsubst %.c,$(BUILD_DIR)/%.o,$(LIBC_C_SRCS))
BOOT_OBJ    := $(BUILD_DIR)/$(BOOT_SRC:.s=.o)

OBJS := $(BOOT_OBJ) $(KERNEL_OBJS) $(LIBC_OBJS)
DEPS := $(OBJS:.o=.d)

# -----------------------
# High-level targets
# -----------------------
.PHONY: all kernel iso run run-debug disk img clean distclean setup print-vars

all: iso
kernel: $(KERNEL_ELF)
iso: $(ISO_IMAGE)

run: iso
	$(QEMU) $(QEMUFLAGS) -cdrom $(ISO_IMAGE)

run-debug: iso
	$(QEMU) $(QEMUFLAGS) $(QEMUDEBUGFLAGS) -cdrom $(ISO_IMAGE)

# Create an empty raw disk image (for later work)
DISK_SIZE_MIB ?= 64
img: disk
disk: $(DISK_IMAGE)

clean:
	rm -rf $(BUILD_DIR)

distclean: clean

setup: $(GRUB_CFG)

print-vars:
	@echo CC=$(CC)
	@echo LD=$(LD)
	@echo KERNEL_ELF=$(KERNEL_ELF)
	@echo ISO_IMAGE=$(ISO_IMAGE)

# -----------------------
# Build rules
# -----------------------

# C compilation (kernel + libc)
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Bootloader (.s)
$(BOOT_OBJ): $(BOOT_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(BOOT_ASFLAGS) -c $< -o $@

# Link kernel ELF
$(KERNEL_ELF): $(OBJS) $(BOOT_DIR)/linker.ld
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# Generate grub.cfg if missing
$(GRUB_CFG):
	@mkdir -p $(ISO_GRUB)
	@printf '%s\n' \
		'set timeout=0' \
		'set default=0' \
		'' \
		'menuentry "Sonarix OS" {' \
		'    multiboot /boot/kernel.elf' \
		'    boot' \
		'}' > $(GRUB_CFG)

# Build ISO (GRUB)
$(ISO_IMAGE): $(KERNEL_ELF) $(GRUB_CFG)
	@mkdir -p $(ISO_BOOT)
	cp $(KERNEL_ELF) $(ISO_BOOT)/kernel.elf
	@if [ "$(ISO_BUILDER)" = "podman" ]; then \
		if [ -z "$(PODMAN)" ]; then \
			echo "error: ISO_BUILDER=podman but podman is not installed"; \
			exit 1; \
		fi; \
		echo "[iso] building via podman"; \
		$(PODMAN) run --rm --arch amd64 \
		  -v "$$(pwd)":/work -w /work docker.io/library/ubuntu:24.04 \
		  bash -lc 'apt-get update && apt-get install -y grub-pc-bin grub-common xorriso mtools && grub-mkrescue -o $(ISO_IMAGE) $(ISO_ROOT)'; \
	elif [ "$(ISO_BUILDER)" = "host" ]; then \
		if [ -z "$(GRUB_MKRESCUE)" ]; then \
			echo "error: ISO_BUILDER=host but grub-mkrescue is not installed"; \
			exit 1; \
		fi; \
		echo "[iso] building via host grub-mkrescue"; \
		$(GRUB_MKRESCUE) -o $(ISO_IMAGE) $(ISO_ROOT); \
	else \
		echo "error: ISO_BUILDER must be 'podman' or 'host'"; \
		exit 1; \
	fi

# Create raw disk image
$(DISK_IMAGE):
	@mkdir -p $(BUILD_DIR)
	dd if=/dev/zero of=$(DISK_IMAGE) bs=1M count=$(DISK_SIZE_MIB)

-include $(DEPS)
