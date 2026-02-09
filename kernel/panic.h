/*
 * AstraOS - Kernel Panic Header
 * Early panic mechanism for fatal errors
 */

#ifndef _ASTRA_PANIC_H
#define _ASTRA_PANIC_H

#include <stdint.h>

/*
 * panic - Halt system with error message
 * This function never returns
 */
__attribute__((noreturn))
void panic(const char *message);

/*
 * panic_assert - Assert with panic on failure
 */
#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            panic("Assertion failed: " #condition); \
        } \
    } while (0)

/*
 * panic_unreachable - Panic for unreachable code
 */
#define UNREACHABLE() panic("Unreachable code reached")

#endif /* _ASTRA_PANIC_H */
