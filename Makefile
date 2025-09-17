# FrostixOS Build System

PROJECT_NAME := FrostixOS
VERSION := 0.1.0

.DEFAULT_GOAL := all

SRCDIR := src
INCDIR := include
LIBCINC := include/lib/libc
BUILDDIR := build
ISODIR := iso
SCRIPTSDIR := scripts

KERNEL := $(BUILDDIR)/frostix.bin
ISO := frostix.iso

AS := nasm
CC := gcc
LD := gcc

CVER = gnu99
OPT = 2
DEBUG_INFO = 2 # 2 = -g

ARCH_FLAGS := -m32 -march=i686 -mtune=generic
TARGET_FLAGS_COMPILE := -ffreestanding -fno-pic -fno-pie -fno-stack-protector
TARGET_FLAGS_LINK := -ffreestanding -no-pie -fno-stack-protector

ASFLAGS := -felf32 -g$(DEBUG_INFO) -F dwarf
CFLAGS := $(ARCH_FLAGS) $(TARGET_FLAGS_COMPILE) -std=$(CVER) -O$(OPT) -Wall -Wextra -Wno-unused-parameter
CFLAGS += -I$(INCDIR) -I$(LIBCINC) -g$(DEBUG_INFO) -MMD -MP
LDFLAGS := $(ARCH_FLAGS) $(TARGET_FLAGS_LINK) -O$(OPT) -Wl,--build-id=none -nostdlib -lgcc

C_SOURCES := $(shell find $(SRCDIR) -name '*.c' 2>/dev/null)
ASM_SOURCES := $(shell find $(SRCDIR) -name '*.s' 2>/dev/null)

C_OBJECTS := $(C_SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
ASM_OBJECTS := $(ASM_SOURCES:$(SRCDIR)/%.s=$(BUILDDIR)/%.o)
OBJECTS := $(C_OBJECTS) $(ASM_OBJECTS)

DEPFILES := $(C_OBJECTS:.o=.d)

-include $(DEPFILES)

COLOR_RED := \033[31m
COLOR_GREEN := \033[32m
COLOR_YELLOW := \033[33m
COLOR_BLUE := \033[34m
COLOR_MAGENTA := \033[35m
COLOR_CYAN := \033[36m
COLOR_RESET := \033[0m

define print_info
	@printf "$(COLOR_BLUE)[INFO]$(COLOR_RESET) %s\n" $(1)
endef

define print_success
	@printf "$(COLOR_GREEN)[SUCCESS]$(COLOR_RESET) %s\n" $(1)
endef

define print_warning
	@printf "$(COLOR_YELLOW)[WARNING]$(COLOR_RESET) %s\n" $(1)
endef

define print_error
	@printf "$(COLOR_RED)[ERROR]$(COLOR_RESET) %s\n" $(1)
endef

all: $(KERNEL)
	$(call print_success,"$(PROJECT_NAME) kernel built successfully!")

iso: $(ISO)
	$(call print_success,"ISO created: $(ISO)")

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)
	@mkdir -p $(dir $(OBJECTS))

$(KERNEL): $(OBJECTS) | $(BUILDDIR)
	$(call print_info,"Linking kernel...")
	@$(LD) -T $(SCRIPTSDIR)/linker.ld -o $@ $(OBJECTS) $(LDFLAGS)
	$(call print_success,"Kernel linked: $@")

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(call print_info,"Compiling $(notdir $<)...")
	@mkdir -p $(dir $@)
	@$(CC) -c $< -o $@ $(CFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.s | $(BUILDDIR)
	$(call print_info,"Assembling $(notdir $<)...")
	@mkdir -p $(dir $@)
	@$(AS) $(ASFLAGS) $< -o $@

$(ISO): $(KERNEL)
	$(call print_info,"Creating ISO...")
	@mkdir -p $(ISODIR)/boot/grub
	@cp $(KERNEL) $(ISODIR)/boot/
	@cp $(SCRIPTSDIR)/grub.cfg $(ISODIR)/boot/grub/
	@if command -v grub-mkrescue >/dev/null 2>&1; then \
		grub-mkrescue -o $@ $(ISODIR) 2>/dev/null; \
	else \
		$(call print_error,"grub-mkrescue not found. Install GRUB tools."); \
		exit 1; \
	fi

# Check multiboot compliance
check: $(KERNEL)
	$(call print_info,"Checking multiboot compliance...")
	@if command -v grub-file >/dev/null 2>&1; then \
		if grub-file --is-x86-multiboot $(KERNEL); then \
			echo "[SUCCESS] Kernel is multiboot compliant"; \
		else \
			echo "[ERROR] Kernel is NOT multiboot compliant"; \
			exit 1; \
		fi; \
	else \
		echo "[WARNING] grub-file not found, skipping multiboot check"; \
	fi

run: $(ISO)
	$(call print_info,"Starting $(PROJECT_NAME)...")
	@if command -v qemu-system-i386 >/dev/null 2>&1; then \
		qemu-system-i386 -cdrom $(ISO) -m 2G -serial stdio; \
	else \
		$(call print_error,"qemu-system-i386 not found. Install QEMU."); \
		exit 1; \
	fi

run-debug: $(ISO)
	$(call print_info,"Starting $(PROJECT_NAME) with debugging...")
	@if command -v qemu-system-i386 >/dev/null 2>&1; then \
		qemu-system-i386 -cdrom $(ISO) -m 2G -s -S -serial stdio; \
	else \
		$(call print_error,"qemu-system-i386 not found. Install QEMU."); \
		exit 1; \
	fi

info:
	@echo "$(PROJECT_NAME) Build System"
	@echo "============================"
	@echo "Version: $(VERSION)"
	@echo "Kernel: $(KERNEL)"
	@echo "ISO: $(ISO)"
	@echo ""
	@echo "Source files found:"
	@echo "  C sources: $(words $(C_SOURCES)) files"
	@for src in $(C_SOURCES); do echo "    $$src"; done
	@echo "  Assembly sources: $(words $(ASM_SOURCES)) files"
	@for src in $(ASM_SOURCES); do echo "    $$src"; done
	@echo ""
	@echo "Object files to be built:"
	@for obj in $(OBJECTS); do echo "    $$obj"; done

clean:
	$(call print_info,"Cleaning build artifacts...")
	@rm -rf $(BUILDDIR)
	@rm -f $(KERNEL)
	@rm -f $(ISO)
	@rm -rf $(ISODIR)
	$(call print_success,"Build artifacts cleaned")

distclean: clean
	$(call print_info,"Cleaning all generated files...")
	@rm -f $(ISO)
	@rm -rf $(ISODIR)
	$(call print_success,"All generated files cleaned")

install-deps-arch:
	$(call print_info,"Installing dependencies for Arch Linux...")
	@sudo pacman -S --needed base-devel nasm qemu-desktop grub xorriso
	$(call print_success,"Dependencies installed - using host gcc")

install-deps-ubuntu:
	$(call print_info,"Installing dependencies for Ubuntu/Debian...")
	@sudo apt update
	@sudo apt install -y build-essential nasm qemu-system-x86 grub-common grub-pc-bin xorriso
	$(call print_success,"Dependencies installed - using host gcc")

format:
	$(call print_info,"Formatting source code...")
	@find $(SRCDIR) $(INCDIR) -name '*.c' -o -name '*.h' | xargs clang-format -i 2>/dev/null || true

lint:
	$(call print_info,"Running static analysis...")
	@find $(SRCDIR) -name '*.c' | xargs cppcheck --enable=all --std=c99 --platform=unix32 2>/dev/null || true

help:
	@echo "$(PROJECT_NAME) Build System"
	@echo "============================"
	@echo ""
	@echo "Main Targets:"
	@echo "  all              - Build the kernel (default)"
	@echo "  iso              - Create bootable ISO image"
	@echo "  run              - Run in QEMU"
	@echo "  run-debug        - Run in QEMU with debugging"
	@echo "  check            - Check multiboot compliance"
	@echo ""
	@echo "Utility Targets:"
	@echo "  info             - Show project information"
	@echo "  clean            - Remove build artifacts"
	@echo "  distclean        - Remove all generated files"
	@echo "  format           - Format source code (requires clang-format)"
	@echo "  lint             - Run static analysis (requires cppcheck)"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Dependency Installation:"
	@echo "  install-deps-arch    - Install dependencies on Arch Linux"
	@echo "  install-deps-ubuntu  - Install dependencies on Ubuntu/Debian"
	@echo ""
	@echo "Requirements:"
	@echo "  - GCC compiler"
	@echo "  - NASM assembler"
	@echo "  - GRUB tools (grub-mkrescue, grub-file)"
	@echo "  - QEMU (qemu-system-i386)"
	@echo "  - xorriso (for ISO creation)"

.PHONY: all iso clean distclean run run-debug run-text check info help
.PHONY: install-deps-arch install-deps-ubuntu format lint
