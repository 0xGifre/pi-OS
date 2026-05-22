# PI OS (simple Meaty-Skeleton-ish Makefile, no .sh)
# NOTE: Avoid building from a path that contains spaces. GNU make handles spaces poorly.

# -----------------------
# Toolchain (cross)
# -----------------------
CROSS_PREFIX ?= $(firstword \
	$(foreach prefix,i686-elf x86_64-elf, \
		$(if $(shell command -v $(prefix)-gcc 2>/dev/null),$(prefix))))

ifeq ($(CROSS_PREFIX),)
$(error No ELF cross compiler found. Install i686-elf-gcc/binutils or x86_64-elf-gcc/binutils)
endif

CC := $(CROSS_PREFIX)-gcc
LD := $(CROSS_PREFIX)-ld
HOSTCC ?= cc

# export PATH="$$HOME/opt/cross/bin:$$PATH

# -----------------------
# Project dirs / outputs
# -----------------------
BOOT_DIR   := boot
SRC_DIR    := src
LIBC_DIR   := libc
INITRD_DIR := initrd
TOOLS_DIR  := tools

BUILD_DIR  := build
ISO_ROOT   := $(BUILD_DIR)/isodir
ISO_BOOT   := $(ISO_ROOT)/boot
ISO_GRUB   := $(ISO_BOOT)/grub
GRUB_CFG   := $(ISO_GRUB)/grub.cfg
INITRD_IMAGE := $(ISO_BOOT)/initrd.bin
INITRD_TOOL  := $(BUILD_DIR)/tools/mkinitrd

KERNEL_ELF := $(BUILD_DIR)/kernel.elf
ISO_IMAGE  := $(BUILD_DIR)/pi-os.iso
DISK_IMAGE := $(BUILD_DIR)/disk.img

# -----------------------
# Flags
# -----------------------
CFLAGS ?=
CFLAGS += -ffreestanding -m32 -fno-pie -fno-stack-protector -nostdlib \
          -mno-sse -mno-sse2 -mno-mmx -mfpmath=387 \
          -D__is_libk \
          -Wall -Wextra -Werror=implicit-function-declaration \
          -I$(SRC_DIR)/include -I$(LIBC_DIR)/include \
          -MMD -MP

# bootloader.s is GAS/AT&T -> compile with gcc
ASFLAGS := -ffreestanding -m32

LDFLAGS := -m elf_i386 -T $(BOOT_DIR)/linker.ld --oformat elf32-i386

QEMU      ?= qemu-system-i386
QEMU_DISPLAY ?= -display cocoa,zoom-to-fit=on
QEMU_WINDOW_WIDTH ?= 800
QEMU_WINDOW_HEIGHT ?= 450
QEMU_RESIZE_DELAY ?= 1
QEMUFLAGS ?= $(QEMU_DISPLAY)
QEMU_ISOFLAGS ?= $(QEMUFLAGS) -boot d

PODMAN        := $(shell command -v podman 2>/dev/null)
DOCKER        := $(shell command -v docker 2>/dev/null)

# -----------------------
# Sources / objects
# -----------------------
KERNEL_C_SRCS := $(shell find $(SRC_DIR) -type f -name '*.c')
LIBC_C_SRCS   := $(shell find $(LIBC_DIR) -type f -name '*.c')
INITRD_FILES  := $(shell find $(INITRD_DIR) -type f | sort)

BOOT_SRC := $(BOOT_DIR)/bootloader.s
ASM_SRCS := $(shell find $(SRC_DIR) -type f -name '*.s')

KERNEL_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_C_SRCS))
LIBC_OBJS   := $(patsubst %.c,$(BUILD_DIR)/%.o,$(LIBC_C_SRCS))
BOOT_OBJ    := $(BUILD_DIR)/$(BOOT_SRC:.s=.o)
ASM_OBJS    := $(patsubst %.s,$(BUILD_DIR)/%.o,$(ASM_SRCS))

OBJS := $(BOOT_OBJ) $(KERNEL_OBJS) $(LIBC_OBJS) $(ASM_OBJS)
DEPS := $(OBJS:.o=.d)

# -----------------------
# High-level targets
# -----------------------
.PHONY: all kernel iso emu-elf emu-iso clean disk img test FORCE

all: 
	$(MAKE) clean
	$(MAKE) kernel
	$(MAKE) iso
	$(MAKE) emu-iso

test:
	$(MAKE) clean
	$(MAKE) kernel
	$(MAKE) emu-elf

kernel: $(KERNEL_ELF)

iso: $(ISO_IMAGE)

emu-elf: $(KERNEL_ELF)
	@{ sleep $(QEMU_RESIZE_DELAY); osascript -e 'tell application "System Events" to tell process "qemu-system-i386" to set size of front window to {$(QEMU_WINDOW_WIDTH), $(QEMU_WINDOW_HEIGHT)}' >/dev/null 2>&1 || true; } &
	$(QEMU) $(QEMUFLAGS) -kernel $(KERNEL_ELF)

emu-iso:
	@{ sleep $(QEMU_RESIZE_DELAY); osascript -e 'tell application "System Events" to tell process "qemu-system-i386" to set size of front window to {$(QEMU_WINDOW_WIDTH), $(QEMU_WINDOW_HEIGHT)}' >/dev/null 2>&1 || true; } &
	$(QEMU) $(QEMU_ISOFLAGS) -drive file=$(DISK_IMAGE),format=raw,if=ide,index=0,media=disk -cdrom $(ISO_IMAGE)

clean:
	rm -rf $(BUILD_DIR)



# Create an empty raw disk image (for later work)
DISK_SIZE_MIB ?= 64
img: disk
disk: $(DISK_IMAGE)




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
	$(CC) $(ASFLAGS) -c $< -o $@

# Kernel assembly
$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

# Link kernel ELF
$(KERNEL_ELF): $(OBJS) $(BOOT_DIR)/linker.ld
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)


# Generate grub.cfg if missing
$(GRUB_CFG): FORCE
	@mkdir -p $(ISO_GRUB)
	@printf '%s\n' \
		'' \
		'menuentry "PI OS" {' \
		'    multiboot /boot/kernel.elf' \
		'    module /boot/initrd.bin initrd' \
		'    boot' \
		'}' > $(GRUB_CFG)

$(INITRD_TOOL): $(TOOLS_DIR)/mkinitrd.c
	@mkdir -p $(dir $@)
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o $@ $<

$(INITRD_IMAGE): $(INITRD_TOOL) $(INITRD_FILES)
	@mkdir -p $(ISO_BOOT)
	$(INITRD_TOOL) $@ $(INITRD_FILES)


# Build ISO (GRUB)
$(ISO_IMAGE): $(KERNEL_ELF) $(GRUB_CFG) $(INITRD_IMAGE)
	@mkdir -p $(ISO_BOOT)
	cp $(KERNEL_ELF) $(ISO_BOOT)/kernel.elf
	@if [ -n "$(PODMAN)" ] && $(PODMAN) info >/dev/null 2>&1; then \
	  $(PODMAN) run --rm --arch amd64 \
	    -v "$$(pwd)":/work -w /work docker.io/library/ubuntu:24.04 \
	    bash -lc 'apt-get update && apt-get install -y grub-pc-bin grub-common xorriso mtools && grub-mkrescue -o $(ISO_IMAGE) $(ISO_ROOT)'; \
	elif [ -n "$(DOCKER)" ] && $(DOCKER) info >/dev/null 2>&1; then \
	  $(DOCKER) run --rm --platform linux/amd64 \
	    -v "$$(pwd)":/work -w /work docker.io/library/ubuntu:24.04 \
	    bash -lc 'apt-get update && apt-get install -y grub-pc-bin grub-common xorriso mtools && grub-mkrescue -o $(ISO_IMAGE) $(ISO_ROOT)'; \
	else \
	  printf '%s\n' 'No running ISO builder found.'; \
	  printf '%s\n' 'Start Docker Desktop, or run: colima start --arch x86_64'; \
	  printf '%s\n' 'Then retry: make iso'; \
	  false; \
	fi


# Create raw disk image
$(DISK_IMAGE):
	@mkdir -p $(BUILD_DIR)
	dd if=/dev/zero of=$(DISK_IMAGE) bs=1M count=$(DISK_SIZE_MIB)

-include $(DEPS)
