/*
 * AstraOS - Standard I/O Implementation
 * kprintf and related functions
 */

#include "stdio.h"
#include "string.h"
#include "../drivers/serial.h"

/* Forward declaration for framebuffer output */
extern void fb_putchar(char c) __attribute__((weak));
extern void fb_puts(const char *s) __attribute__((weak));

/*
 * Output character to both serial and framebuffer
 */
static void kputchar(char c) {
    serial_putchar(c);
    if (fb_putchar) {
        fb_putchar(c);
    }
}

/*
 * Output string to both serial and framebuffer
 */
static void kputs(const char *s) {
    while (*s) {
        kputchar(*s++);
    }
}

/*
 * Convert integer to string
 */
static int int_to_str(char *buf, int64_t value, int base, int uppercase, int width, char pad) {
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char temp[65];
    int i = 0;
    int negative = 0;

    if (value < 0 && base == 10) {
        negative = 1;
        value = -value;
    }

    uint64_t uvalue = (uint64_t)value;

    /* Generate digits in reverse */
    do {
        temp[i++] = digits[uvalue % base];
        uvalue /= base;
    } while (uvalue > 0);

    /* Calculate padding */
    int len = i + negative;
    int padding = (width > len) ? width - len : 0;

    int j = 0;

    /* Add padding */
    if (pad == '0' && negative) {
        buf[j++] = '-';
        negative = 0;
    }

    while (padding-- > 0) {
        buf[j++] = pad;
    }

    if (negative) {
        buf[j++] = '-';
    }

    /* Copy digits in correct order */
    while (i > 0) {
        buf[j++] = temp[--i];
    }

    buf[j] = '\0';
    return j;
}

/*
 * Convert unsigned integer to string
 */
static int uint_to_str(char *buf, uint64_t value, int base, int uppercase, int width, char pad) {
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char temp[65];
    int i = 0;

    /* Generate digits in reverse */
    do {
        temp[i++] = digits[value % base];
        value /= base;
    } while (value > 0);

    /* Calculate padding */
    int padding = (width > i) ? width - i : 0;

    int j = 0;

    /* Add padding */
    while (padding-- > 0) {
        buf[j++] = pad;
    }

    /* Copy digits in correct order */
    while (i > 0) {
        buf[j++] = temp[--i];
    }

    buf[j] = '\0';
    return j;
}

/*
 * Core vprintf implementation
 */
int kvprintf(const char *format, va_list args) {
    int count = 0;
    char buf[65];

    while (*format) {
        if (*format != '%') {
            kputchar(*format++);
            count++;
            continue;
        }

        format++;  /* Skip '%' */

        /* Parse flags */
        char pad = ' ';
        if (*format == '0') {
            pad = '0';
            format++;
        }

        /* Parse width */
        int width = 0;
        while (*format >= '0' && *format <= '9') {
            width = width * 10 + (*format - '0');
            format++;
        }

        /* Parse length modifier */
        int is_long = 0;
        int is_longlong = 0;
        if (*format == 'l') {
            format++;
            is_long = 1;
            if (*format == 'l') {
                format++;
                is_longlong = 1;
            }
        }

        /* Parse conversion specifier */
        switch (*format) {
            case 'd':
            case 'i': {
                int64_t value;
                if (is_longlong) {
                    value = va_arg(args, int64_t);
                } else if (is_long) {
                    value = va_arg(args, long);
                } else {
                    value = va_arg(args, int);
                }
                int len = int_to_str(buf, value, 10, 0, width, pad);
                kputs(buf);
                count += len;
                break;
            }

            case 'u': {
                uint64_t value;
                if (is_longlong) {
                    value = va_arg(args, uint64_t);
                } else if (is_long) {
                    value = va_arg(args, unsigned long);
                } else {
                    value = va_arg(args, unsigned int);
                }
                int len = uint_to_str(buf, value, 10, 0, width, pad);
                kputs(buf);
                count += len;
                break;
            }

            case 'x': {
                uint64_t value;
                if (is_longlong) {
                    value = va_arg(args, uint64_t);
                } else if (is_long) {
                    value = va_arg(args, unsigned long);
                } else {
                    value = va_arg(args, unsigned int);
                }
                int len = uint_to_str(buf, value, 16, 0, width, pad);
                kputs(buf);
                count += len;
                break;
            }

            case 'X': {
                uint64_t value;
                if (is_longlong) {
                    value = va_arg(args, uint64_t);
                } else if (is_long) {
                    value = va_arg(args, unsigned long);
                } else {
                    value = va_arg(args, unsigned int);
                }
                int len = uint_to_str(buf, value, 16, 1, width, pad);
                kputs(buf);
                count += len;
                break;
            }

            case 'p': {
                uint64_t value = (uint64_t)va_arg(args, void *);
                kputs("0x");
                count += 2;
                int len = uint_to_str(buf, value, 16, 0, 16, '0');
                kputs(buf);
                count += len;
                break;
            }

            case 'c': {
                char c = (char)va_arg(args, int);
                kputchar(c);
                count++;
                break;
            }

            case 's': {
                const char *s = va_arg(args, const char *);
                if (!s) s = "(null)";
                int len = strlen(s);
                int padding = (width > len) ? width - len : 0;
                while (padding-- > 0) {
                    kputchar(' ');
                    count++;
                }
                kputs(s);
                count += len;
                break;
            }

            case '%':
                kputchar('%');
                count++;
                break;

            default:
                kputchar('%');
                kputchar(*format);
                count += 2;
                break;
        }

        format++;
    }

    return count;
}

/*
 * kprintf - kernel printf
 */
int kprintf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int count = kvprintf(format, args);
    va_end(args);
    return count;
}

/*
 * ksprintf - format to buffer
 */
int ksprintf(char *buf, const char *format, ...) {
    va_list args;
    va_start(args, format);
    /* Simple implementation - just use ksnprintf with large size */
    int count = ksnprintf(buf, 4096, format, args);
    va_end(args);
    return count;
}

/*
 * ksnprintf - format to buffer with size limit
 */
int ksnprintf(char *buf, size_t size, const char *format, ...) {
    if (size == 0) return 0;

    va_list args;
    va_start(args, format);

    size_t pos = 0;
    char temp[65];

    while (*format && pos < size - 1) {
        if (*format != '%') {
            buf[pos++] = *format++;
            continue;
        }

        format++;

        /* Parse width */
        int width = 0;
        char pad = ' ';
        if (*format == '0') {
            pad = '0';
            format++;
        }
        while (*format >= '0' && *format <= '9') {
            width = width * 10 + (*format - '0');
            format++;
        }

        /* Length modifier */
        int is_long = 0;
        if (*format == 'l') {
            format++;
            is_long = 1;
            if (*format == 'l') {
                format++;
            }
        }

        switch (*format) {
            case 'd':
            case 'i': {
                int64_t value = is_long ? va_arg(args, int64_t) : va_arg(args, int);
                int_to_str(temp, value, 10, 0, width, pad);
                for (char *p = temp; *p && pos < size - 1; p++) {
                    buf[pos++] = *p;
                }
                break;
            }
            case 'u': {
                uint64_t value = is_long ? va_arg(args, uint64_t) : va_arg(args, unsigned int);
                uint_to_str(temp, value, 10, 0, width, pad);
                for (char *p = temp; *p && pos < size - 1; p++) {
                    buf[pos++] = *p;
                }
                break;
            }
            case 'x':
            case 'X': {
                uint64_t value = is_long ? va_arg(args, uint64_t) : va_arg(args, unsigned int);
                uint_to_str(temp, value, 16, *format == 'X', width, pad);
                for (char *p = temp; *p && pos < size - 1; p++) {
                    buf[pos++] = *p;
                }
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                if (!s) s = "(null)";
                while (*s && pos < size - 1) {
                    buf[pos++] = *s++;
                }
                break;
            }
            case 'c':
                buf[pos++] = (char)va_arg(args, int);
                break;
            case '%':
                buf[pos++] = '%';
                break;
            default:
                break;
        }
        format++;
    }

    buf[pos] = '\0';
    va_end(args);
    return pos;
}
