/*
 * AstraOS - Standard I/O Header
 * kprintf and related functions
 */

#ifndef _ASTRA_LIB_STDIO_H
#define _ASTRA_LIB_STDIO_H

#include <stdarg.h>
#include <stddef.h>

/*
 * Kernel printf - outputs to framebuffer and serial
 */
int kprintf(const char *format, ...);

/*
 * Kernel sprintf - format to buffer
 */
int ksprintf(char *buf, const char *format, ...);

/*
 * Kernel snprintf - format to buffer with size limit
 */
int ksnprintf(char *buf, size_t size, const char *format, ...);

/*
 * Kernel vprintf - va_list version
 */
int kvprintf(const char *format, va_list args);

/*
 * Check if a character is available from any input source (keyboard, serial)
 */
int khaschar(void);

/*
 * Get a character from any input source (blocking)
 */
char kgetc(void);

#endif /* _ASTRA_LIB_STDIO_H */
