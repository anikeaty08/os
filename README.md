# 🌌 AstraOS 🚀

![Architecture](https://img.shields.io/badge/Architecture-x86__64-blue)
![Language](https://img.shields.io/badge/Language-C-orange)
![Bootloader](https://img.shields.io/badge/Bootloader-Limine-purple)
![CPU Mode](https://img.shields.io/badge/CPU%20Mode-Long%20Mode-success)
![Kernel](https://img.shields.io/badge/Kernel-Monolithic-red)
![Scheduler](https://img.shields.io/badge/Scheduler-Round--Robin-yellow)
![Filesystem](https://img.shields.io/badge/Filesystem-FAT16%20(Read--Only)-lightgrey)
![Status](https://img.shields.io/badge/Status-Active%20Development-brightgreen)
![License](https://img.shields.io/badge/License-Educational-lightgrey)

🧠 **AstraOS** is a modern, minimal **x86_64 operating system** built from scratch in **C**, focused on understanding kernel internals, memory management, multitasking, and low-level system design.

---

## ✨ Features 🧩

- 🚀 Limine bootloader (UEFI + BIOS support)
- 🧠 Higher-half x86_64 kernel (Long Mode)
- 🧱 GDT, IDT, exception & IRQ handling
- 🧮 Physical Memory Manager (bitmap-based)
- 🗺️ Virtual Memory Manager (4-level paging)
- 🧰 Kernel heap (`kmalloc`, `kfree`)
- 🔁 Round-robin scheduler
- 🔄 Context switching
- ⏱️ Timer-driven preemption
- 🖥️ Framebuffer console
- ⌨️ PS/2 keyboard driver
- 🧪 Serial debugging (COM1)
- 💽 ATA disk driver (PIO)
- 🗂️ Virtual File System (VFS)
- 📁 FAT16 filesystem (read-only)
- 💻 Interactive shell with built-in commands

---

## 🏗️ Project Structure 🧱

AstraOS/
├── Makefile  
├── linker.ld  
├── limine.conf  
├── kernel/  
│   ├── main.c  
│   ├── arch/x86_64/  
│   │   ├── gdt/  
│   │   ├── idt/  
│   │   ├── interrupt/  
│   │   └── io.h  
│   ├── mm/  
│   │   ├── pmm.c  
│   │   ├── vmm.c  
│   │   └── heap.c  
│   ├── proc/  
│   │   ├── process.c  
│   │   ├── scheduler.c  
│   │   └── context.asm  
│   ├── drivers/  
│   │   ├── framebuffer.c  
│   │   ├── keyboard.c  
│   │   ├── pit.c  
│   │   ├── serial.c  
│   │   └── ata.c  
│   ├── fs/  
│   │   ├── vfs.c  
│   │   └── fat.c  
│   ├── shell/  
│   │   ├── shell.c  
│   │   └── commands.c  
│   └── lib/  
│       ├── string.c  
│       └── stdio.c  
└── iso/  

---

## 🛠️ Build Requirements 🧰

- x86_64-elf-gcc  
- nasm  
- xorriso  
- qemu-system-x86_64  
- make  

---

## 🚀 Building & Running 💡

make clean  
make  
make iso  
qemu-system-x86_64 -cdrom astraos.iso -m 256M -serial stdio  

---

## 🧪 Debugging 🔍

qemu-system-x86_64 -cdrom astraos.iso -m 256M -s -S &  
gdb kernel.elf  

Useful QEMU flags:
- `-serial stdio` — serial debugging  
- `-d int` — interrupt tracing  
- `-no-reboot` — catch triple faults  

---

## 🗺️ Roadmap 🧭

- 👤 User mode (Ring 3)
- 📞 System call interface
- 📦 ELF64 program loader
- 🧠 Per-process virtual address space
- ⏲️ APIC + HPET timers
- 🧵 SMP (multi-core support)
- ✍️ Read/write filesystem support
- 🪟 GUI subsystem

---

## 🎓 Educational Goals 📘

AstraOS is designed to explore modern operating system concepts, including memory management, multitasking, interrupts, and low-level hardware interaction.

---

## ⚠️ Disclaimer ❗

AstraOS is an **educational operating system** and is **not intended for production use**.

---

## 📚 References 🔗

- OSDev Wiki — https://wiki.osdev.org  
- Limine Bootloader — https://github.com/limine-bootloader/limine  
- Intel® 64 and IA-32 Architectures Software Developer Manuals  

---

## 📝 License 📜

Released for educational purposes. Free to use, modify, and learn from.
