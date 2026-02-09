# AstraOS Makefile
# x86_64 Hobby Operating System

# Toolchain - uses Linux gcc in freestanding mode
# On Linux/WSL: uses system gcc
# For true cross-compile: install x86_64-elf-gcc and change CC/LD
CC := gcc
AS := nasm
LD := ld

# Compiler flags - CRITICAL for kernel development
CFLAGS := -ffreestanding \
          -fno-stack-protector \
          -fno-stack-check \
          -fno-pie \
          -fno-pic \
          -mno-red-zone \
          -mno-80387 \
          -mno-mmx \
          -mno-sse \
          -mno-sse2 \
          -mcmodel=kernel \
          -Wall \
          -Wextra \
          -Werror \
          -std=gnu11 \
          -O2 \
          -g \
          -Ikernel

ASFLAGS := -f elf64 -g

LDFLAGS := -nostdlib \
           -static \
           -z max-page-size=0x1000 \
           -T linker.ld

# Source files
C_SOURCES := $(shell find kernel -name '*.c')
ASM_SOURCES := $(shell find kernel -name '*.asm')

# Object files
C_OBJECTS := $(C_SOURCES:.c=.o)
ASM_OBJECTS := $(ASM_SOURCES:.asm=.o)
OBJECTS := $(C_OBJECTS) $(ASM_OBJECTS)

# Output files
KERNEL := iso/kernel.elf
ISO := astraos.iso

# Limine bootloader path (adjust after cloning)
LIMINE_DIR := limine

.PHONY: all clean run run-debug iso limine

all: $(KERNEL)

$(KERNEL): $(OBJECTS)
	@mkdir -p iso
	$(LD) $(LDFLAGS) -o $@ $^
	@echo "Kernel built: $@"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

# Clone Limine if not present
limine:
	@if [ ! -d "$(LIMINE_DIR)" ]; then \
		echo "Cloning Limine bootloader..."; \
		git clone https://github.com/limine-bootloader/limine.git --branch=v8.x-binary --depth=1; \
	fi

# Build ISO image
iso: $(KERNEL) limine
	@echo "Building ISO..."
	cp limine.conf iso/
	cp $(LIMINE_DIR)/limine-bios.sys iso/
	cp $(LIMINE_DIR)/limine-bios-cd.bin iso/
	cp $(LIMINE_DIR)/limine-uefi-cd.bin iso/
	mkdir -p iso/EFI/BOOT
	cp $(LIMINE_DIR)/BOOTX64.EFI iso/EFI/BOOT/
	cp $(LIMINE_DIR)/BOOTIA32.EFI iso/EFI/BOOT/
	xorriso -as mkisofs -b limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso -o $(ISO)
	$(LIMINE_DIR)/limine bios-install $(ISO)
	@echo "ISO built: $(ISO)"

# Run in QEMU
run: iso
	qemu-system-x86_64 \
		-cdrom $(ISO) \
		-m 256M \
		-serial stdio \
		-no-reboot \
		-no-shutdown

# Run with interrupt debugging
run-debug: iso
	qemu-system-x86_64 \
		-cdrom $(ISO) \
		-m 256M \
		-serial stdio \
		-d int,cpu_reset \
		-no-reboot \
		-no-shutdown

# Run with GDB support
run-gdb: iso
	qemu-system-x86_64 \
		-cdrom $(ISO) \
		-m 256M \
		-serial stdio \
		-s -S \
		-no-reboot \
		-no-shutdown &
	@echo "QEMU started. Connect with: gdb iso/kernel.elf -ex 'target remote :1234'"

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(KERNEL) $(ISO)
	rm -rf iso/*.sys iso/*.bin iso/*.conf iso/*.elf iso/EFI

# Deep clean (including Limine)
distclean: clean
	rm -rf $(LIMINE_DIR)

# Show help
help:
	@echo "AstraOS Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build kernel"
	@echo "  iso        - Build bootable ISO"
	@echo "  run        - Run in QEMU"
	@echo "  run-debug  - Run with interrupt debugging"
	@echo "  run-gdb    - Run with GDB support"
	@echo "  clean      - Remove build artifacts"
	@echo "  distclean  - Remove everything including Limine"
	@echo "  help       - Show this help"
