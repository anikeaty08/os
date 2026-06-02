/*
 * AstraOS - Kernel Panic Implementation
 * Early panic mechanism for fatal errors
 */

#include "panic.h"
#include "drivers/serial.h"
#include "arch/x86_64/cpu.h"

#define CRASH_LOG_SIZE 4096

static char crash_log[CRASH_LOG_SIZE];
static size_t crash_log_next;
static size_t crash_log_len;

/* Forward declaration for framebuffer output (if available) */
extern void fb_puts(const char *s) __attribute__((weak));

void crash_log_write(const char *message) {
    uint64_t flags;

    if (!message) {
        message = "(null)";
    }

    flags = cpu_save_flags();
    cpu_cli();

    while (*message) {
        crash_log[crash_log_next] = *message++;
        crash_log_next = (crash_log_next + 1) % CRASH_LOG_SIZE;
        if (crash_log_len < CRASH_LOG_SIZE) {
            crash_log_len++;
        }
    }

    cpu_restore_flags(flags);
}

void crash_log_dump(void) {
    uint64_t flags = cpu_save_flags();
    size_t start;

    cpu_cli();

    serial_puts("Crash log:\n");
    serial_puts("----------------------------------------\n");

    if (crash_log_len == 0) {
        serial_puts("(empty)\n");
    } else {
        start = (crash_log_next + CRASH_LOG_SIZE - crash_log_len) % CRASH_LOG_SIZE;
        for (size_t i = 0; i < crash_log_len; i++) {
            char c = crash_log[(start + i) % CRASH_LOG_SIZE];
            serial_putchar(c);
        }
        if (crash_log[(crash_log_next + CRASH_LOG_SIZE - 1) % CRASH_LOG_SIZE] != '\n') {
            serial_putchar('\n');
        }
    }

    serial_puts("----------------------------------------\n");

    cpu_restore_flags(flags);
}

/*
 * panic - Halt system with error message
 * Outputs to serial and framebuffer (if initialized)
 * Never returns
 */
__attribute__((noreturn))
void panic(const char *message) {
    if (!message) {
        message = "(null)";
    }

    /* Disable interrupts immediately */
    cpu_cli();

    crash_log_write("PANIC: ");
    crash_log_write(message);
    crash_log_write("\n");

    /* Output to serial */
    serial_puts("\n\n");
    serial_puts("========================================\n");
    serial_puts("          !! KERNEL PANIC !!            \n");
    serial_puts("========================================\n");
    serial_puts("\n");
    serial_puts("FATAL ERROR: ");
    serial_puts(message);
    serial_puts("\n\n");
    crash_log_dump();
    serial_puts("\n");
    serial_puts("System halted.\n");

    /* Output to framebuffer if available */
    if (fb_puts) {
        fb_puts("\n\n!! KERNEL PANIC !!\n\n");
        fb_puts("FATAL ERROR: ");
        fb_puts(message);
        fb_puts("\n\nSystem halted.\n");
    }

    /* Halt forever */
    cpu_halt_forever();
}
