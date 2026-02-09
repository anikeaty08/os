/*
 * Limine Bootloader Protocol Header
 * BSD Zero Clause License - See https://github.com/limine-bootloader/limine
 *
 * This is a minimal subset of the Limine protocol for AstraOS
 */

#ifndef _LIMINE_H
#define _LIMINE_H

#include <stdint.h>

/* Limine magic number markers */
#define LIMINE_COMMON_MAGIC_1 0xc7b1dd30df4c8b88
#define LIMINE_COMMON_MAGIC_2 0x0a82e883a194f07b

/* Request markers for linker sections */
#define LIMINE_REQUESTS_START_MARKER \
    uint64_t __limine_reqs_start[4] = { 0xf6b8f4b39de7d1ae, 0xfab91a6940fcb9cf, \
                                        0x785c6ed015d3e316, 0x181e920a7852b9d9 }
#define LIMINE_REQUESTS_END_MARKER \
    uint64_t __limine_reqs_end[2] = { 0xadc0e0531bb10d03, 0x9572709f31764c62 }

/* Base revision */
#define LIMINE_BASE_REVISION(N) \
    uint64_t __limine_base_revision[3] = { 0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, (N) }

/* Helper macro for pointers */
#ifdef __cplusplus
#define LIMINE_PTR(TYPE) TYPE
#else
#define LIMINE_PTR(TYPE) TYPE
#endif

/*
 * UUID structure
 */
struct limine_uuid {
    uint32_t a;
    uint16_t b;
    uint16_t c;
    uint8_t d[8];
};

/*
 * File structure (for modules, kernel file info, etc.)
 */
struct limine_file {
    uint64_t revision;
    LIMINE_PTR(void *) address;
    uint64_t size;
    LIMINE_PTR(char *) path;
    LIMINE_PTR(char *) cmdline;
    uint32_t media_type;
    uint32_t unused;
    uint32_t tftp_ip;
    uint32_t tftp_port;
    uint32_t partition_index;
    uint32_t mbr_disk_id;
    struct limine_uuid gpt_disk_uuid;
    struct limine_uuid gpt_part_uuid;
    struct limine_uuid part_uuid;
};

/*
 * Bootloader Info Request
 */
#define LIMINE_BOOTLOADER_INFO_REQUEST { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, \
    0xf55038d8e2a1202f, 0x279426fcf5f59740 }

struct limine_bootloader_info_response {
    uint64_t revision;
    LIMINE_PTR(char *) name;
    LIMINE_PTR(char *) version;
};

struct limine_bootloader_info_request {
    uint64_t id[4];
    uint64_t revision;
    LIMINE_PTR(struct limine_bootloader_info_response *) response;
};

/*
 * Framebuffer Request
 */
#define LIMINE_FRAMEBUFFER_REQUEST { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, \
    0x9d5827dcd881dd75, 0xa3148604f6fab11b }

struct limine_video_mode {
    uint64_t pitch;
    uint64_t width;
    uint64_t height;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
    uint8_t unused;
};

struct limine_framebuffer {
    LIMINE_PTR(void *) address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
    uint8_t unused[7];
    uint64_t edid_size;
    LIMINE_PTR(void *) edid;
    uint64_t mode_count;
    LIMINE_PTR(struct limine_video_mode **) modes;
};

struct limine_framebuffer_response {
    uint64_t revision;
    uint64_t framebuffer_count;
    LIMINE_PTR(struct limine_framebuffer **) framebuffers;
};

struct limine_framebuffer_request {
    uint64_t id[4];
    uint64_t revision;
    LIMINE_PTR(struct limine_framebuffer_response *) response;
};

/*
 * Higher Half Direct Map (HHDM) Request
 */
#define LIMINE_HHDM_REQUEST { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, \
    0x48dcf1cb8ad2b852, 0x63984e959a98244b }

struct limine_hhdm_response {
    uint64_t revision;
    uint64_t offset;
};

struct limine_hhdm_request {
    uint64_t id[4];
    uint64_t revision;
    LIMINE_PTR(struct limine_hhdm_response *) response;
};

/*
 * Memory Map Request
 */
#define LIMINE_MEMMAP_REQUEST { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, \
    0x67cf3d9d378a806f, 0xe304acdfc50c3c62 }

#define LIMINE_MEMMAP_USABLE                 0
#define LIMINE_MEMMAP_RESERVED               1
#define LIMINE_MEMMAP_ACPI_RECLAIMABLE       2
#define LIMINE_MEMMAP_ACPI_NVS               3
#define LIMINE_MEMMAP_BAD_MEMORY             4
#define LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE 5
#define LIMINE_MEMMAP_KERNEL_AND_MODULES     6
#define LIMINE_MEMMAP_FRAMEBUFFER            7

struct limine_memmap_entry {
    uint64_t base;
    uint64_t length;
    uint64_t type;
};

struct limine_memmap_response {
    uint64_t revision;
    uint64_t entry_count;
    LIMINE_PTR(struct limine_memmap_entry **) entries;
};

struct limine_memmap_request {
    uint64_t id[4];
    uint64_t revision;
    LIMINE_PTR(struct limine_memmap_response *) response;
};

/*
 * Kernel Address Request
 */
#define LIMINE_KERNEL_ADDRESS_REQUEST { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, \
    0x71ba76863cc55f63, 0xb2644a48c516a487 }

struct limine_kernel_address_response {
    uint64_t revision;
    uint64_t physical_base;
    uint64_t virtual_base;
};

struct limine_kernel_address_request {
    uint64_t id[4];
    uint64_t revision;
    LIMINE_PTR(struct limine_kernel_address_response *) response;
};

/*
 * RSDP (ACPI) Request
 */
#define LIMINE_RSDP_REQUEST { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, \
    0xc5e77b6b397e7b43, 0x27637845accdcf3c }

struct limine_rsdp_response {
    uint64_t revision;
    LIMINE_PTR(void *) address;
};

struct limine_rsdp_request {
    uint64_t id[4];
    uint64_t revision;
    LIMINE_PTR(struct limine_rsdp_response *) response;
};

/*
 * Boot Time Request
 */
#define LIMINE_BOOT_TIME_REQUEST { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, \
    0x502746e184c088aa, 0xfbc5ec83e6327893 }

struct limine_boot_time_response {
    uint64_t revision;
    int64_t boot_time;
};

struct limine_boot_time_request {
    uint64_t id[4];
    uint64_t revision;
    LIMINE_PTR(struct limine_boot_time_response *) response;
};

/*
 * Kernel File Request
 */
#define LIMINE_KERNEL_FILE_REQUEST { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, \
    0xad97e90e83f1ed67, 0x31eb5d1c5ff23b69 }

struct limine_kernel_file_response {
    uint64_t revision;
    LIMINE_PTR(struct limine_file *) kernel_file;
};

struct limine_kernel_file_request {
    uint64_t id[4];
    uint64_t revision;
    LIMINE_PTR(struct limine_kernel_file_response *) response;
};

/*
 * Module Request
 */
#define LIMINE_MODULE_REQUEST { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, \
    0x3e7e279702be32af, 0xca1c4f3bd1280cee }

#define LIMINE_INTERNAL_MODULE_REQUIRED (1 << 0)
#define LIMINE_INTERNAL_MODULE_COMPRESSED (1 << 1)

struct limine_internal_module {
    LIMINE_PTR(const char *) path;
    LIMINE_PTR(const char *) cmdline;
    uint64_t flags;
};

struct limine_module_response {
    uint64_t revision;
    uint64_t module_count;
    LIMINE_PTR(struct limine_file **) modules;
};

struct limine_module_request {
    uint64_t id[4];
    uint64_t revision;
    LIMINE_PTR(struct limine_module_response *) response;
    uint64_t internal_module_count;
    LIMINE_PTR(struct limine_internal_module **) internal_modules;
};

/*
 * SMP (Symmetric Multi-Processing) Request
 */
#define LIMINE_SMP_REQUEST { LIMINE_COMMON_MAGIC_1, LIMINE_COMMON_MAGIC_2, \
    0x95a67b819a1b857e, 0xa0b61b723b6a73e0 }

struct limine_smp_info {
    uint32_t processor_id;
    uint32_t lapic_id;
    uint64_t reserved;
    LIMINE_PTR(void (*)(struct limine_smp_info *)) goto_address;
    uint64_t extra_argument;
};

struct limine_smp_response {
    uint64_t revision;
    uint32_t flags;
    uint32_t bsp_lapic_id;
    uint64_t cpu_count;
    LIMINE_PTR(struct limine_smp_info **) cpus;
};

struct limine_smp_request {
    uint64_t id[4];
    uint64_t revision;
    LIMINE_PTR(struct limine_smp_response *) response;
    uint64_t flags;
};

#define LIMINE_SMP_X2APIC (1 << 0)

#endif /* _LIMINE_H */
