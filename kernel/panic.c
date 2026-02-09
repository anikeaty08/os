/*
 * AstraOS - Kernel Panic Implementation
 * Early panic mechanism for fatal errors
 */

#include "panic.h"
#include "drivers/serial.h"
#include "arch/x86_64/cpu.h"

/* Forward declaration for framebuffer output (if available) */
extern void fb_puts(const char *s) __attribute__((weak));

/*
 * panic - Halt system with error message
 * Outputs to serial and framebuffer (if initialized)
 * Never returns
 */
__attribute__((noreturn))
void panic(const char *message) {
    /* Disable interrupts immediately */
    cpu_cli();

    /* Output to serial */
    serial_puts("\n\n");
    serial_puts("========================================\n");
    serial_puts("          !! KERNEL PANIC !!            \n");
    serial_puts("========================================\n");
    serial_puts("\n");
    serial_puts("FATAL ERROR: ");
    serial_puts(message);
    serial_puts("\n\n");
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
