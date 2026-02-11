# AstraOS

A hobby x86_64 operating system written in C from scratch.

![Architecture](https://img.shields.io/badge/arch-x86__64-blue)
![Language](https://img.shields.io/badge/lang-C-orange)
![Bootloader](https://img.shields.io/badge/boot-Limine-purple)
![License](https://img.shields.io/badge/license-MIT-green)

---

## Features

### Core Kernel
- **Limine bootloader** - UEFI/BIOS support, handles long mode setup
- **Higher-half kernel** - Loaded at `0xFFFFFFFF80000000`
- **GDT with TSS** - Kernel/user segments, task state segment
- **IDT** - Full exception handling (0-31) and IRQ support (32-47)
- **8259 PIC** - Remapped IRQs, abstracted for future APIC support

### Memory Management
- **Physical Memory Manager** - Bitmap allocator, 4KB pages
- **Virtual Memory Manager** - 4-level paging (PML4)
- **Kernel Heap** - `kmalloc()`/`kfree()` with block coalescing

### Process Management
- **Process Control Blocks** - PID, state, kernel stack
- **Round-Robin Scheduler** - Preemptive multitasking
- **Context Switching** - Full register save/restore

### Drivers
- **Framebuffer Console** - Text output with 8x8 font
- **PS/2 Keyboard** - Scancode translation, modifier keys
- **PIT Timer** - 1000 Hz tick, lightweight IRQ handler
- **Serial Port** - COM1 debug output at 115200 baud
- **ATA Disk** - PIO mode read-only driver
- **ACPI** - Power off support

### File System
- **VFS Layer** - Abstract file operations
- **FAT16** - Read-only filesystem support

### Shell
Built-in commands:
| Command | Description |
|---------|-------------|
| `help` | List available commands |
| `clear` | Clear the screen |
| `echo` | Print text |
| `mem` | Show memory usage |
| `uptime` | Display system uptime |
| `cpuinfo` | Show CPU information |
| `ls` | List directory contents |
| `cat` | Display file contents |
| `ps` | List processes |
| `version` | Show OS version |
| `test` | Run system tests |
| `reboot` | Restart system |
| `shutdown` | Power off (ACPI) |

---

## Project Structure

```
os/
├── Makefile
├── linker.ld
├── limine.conf
└── kernel/
    ├── main.c              # Entry point
    ├── panic.c/h           # Kernel panic
    ├── limine.h            # Bootloader protocol
    ├── arch/x86_64/
    │   ├── gdt.c/h/asm     # Global Descriptor Table
    │   ├── idt.c/h/asm     # Interrupt Descriptor Table
    │   ├── isr.c/h         # Interrupt handlers
    │   ├── pic.c/h         # 8259 PIC driver
    │   ├── irq.c/h         # IRQ abstraction
    │   ├── cpu.h           # CPU operations
    │   └── io.h            # Port I/O
    ├── sync/
    │   └── spinlock.c/h    # Spinlock primitives
    ├── mm/
    │   ├── pmm.c/h         # Physical memory
    │   ├── vmm.c/h         # Virtual memory
    │   └── heap.c/h        # Kernel heap
    ├── proc/
    │   ├── process.c/h     # Process management
    │   ├── scheduler.c/h   # Scheduler
    │   └── context.asm     # Context switch
    ├── drivers/
    │   ├── serial.c/h      # Serial port
    │   ├── pit.c/h         # Timer
    │   ├── keyboard.c/h    # Keyboard
    │   ├── ata.c/h         # Disk driver
    │   └── acpi.c/h        # ACPI power management
    ├── fs/
    │   ├── vfs.c/h         # Virtual filesystem
    │   └── fat.c/h         # FAT16 driver
    ├── lib/
    │   ├── string.c/h      # String functions
    │   └── stdio.c/h       # kprintf
    └── shell/
        ├── shell.c/h       # Command interpreter
        └── commands.c/h    # Built-in commands
```

---

## Building

### Requirements
- GCC (or x86_64-elf-gcc cross compiler)
- NASM
- xorriso
- QEMU
- Git

### On Linux/WSL

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install build-essential nasm xorriso qemu-system-x86 git

# Clone Limine bootloader
git clone https://github.com/limine-bootloader/limine.git --branch=v8.x-binary --depth=1

# Build
make iso

# Run
make run
```

### Make Targets
| Target | Description |
|--------|-------------|
| `make` | Build kernel |
| `make iso` | Create bootable ISO |
| `make run` | Run in QEMU |
| `make run-debug` | Run with interrupt debugging |
| `make clean` | Remove build artifacts |

---

## Running in QEMU

```bash
# Basic run
qemu-system-x86_64 -cdrom astraos.iso -m 256M -serial stdio

# With FAT16 disk image
qemu-system-x86_64 -cdrom astraos.iso -m 256M -serial stdio \
    -drive file=disk.img,format=raw,if=ide

# Debug mode
qemu-system-x86_64 -cdrom astraos.iso -m 256M -serial stdio \
    -d int -no-reboot -no-shutdown
```

---

## Creating a Test Disk

```bash
# Create 32MB FAT16 disk image
dd if=/dev/zero of=disk.img bs=1M count=32
mkfs.fat -F 16 disk.img

# Add files
echo "Hello from AstraOS!" > hello.txt
mcopy -i disk.img hello.txt ::

# Run with disk
make run DISK=disk.img
```

---

## Design Principles

1. **Lightweight ISRs** - Interrupt handlers only acknowledge hardware and queue work
2. **Spinlock Protection** - All shared data structures protected
3. **Timer Safety** - Timer ISR only sets flags, no heavy logic
4. **Architecture Separation** - All x86 code in `arch/x86_64/`
5. **Read-Only Filesystem** - FAT16 is read-only to prevent corruption

---

## Future Plans

- [ ] User mode (Ring 3)
- [ ] System calls
- [ ] ELF program loader
- [ ] APIC/IOAPIC support
- [ ] SMP (multi-core)
- [ ] Network stack
- [ ] GUI subsystem

---

## Resources

- [OSDev Wiki](https://wiki.osdev.org/)
- [Limine Protocol](https://github.com/limine-bootloader/limine/blob/trunk/PROTOCOL.md)
- [Intel SDM](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)

---

## License

MIT License - Free to use, modify, and learn from.
