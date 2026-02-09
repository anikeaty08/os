/*
 * AstraOS - Serial Port Driver Header
 * COM1 serial output for debug messages
 */

#ifndef _ASTRA_DRIVERS_SERIAL_H
#define _ASTRA_DRIVERS_SERIAL_H

#include <stdint.h>
#include <stddef.h>

/* Serial port base addresses */
#define SERIAL_COM1 0x3F8
#define SERIAL_COM2 0x2F8
#define SERIAL_COM3 0x3E8
#define SERIAL_COM4 0x2E8

/*
 * serial_init - Initialize serial port
 * Returns 0 on success, -1 on failure
 */
int serial_init(void);

/*
 * serial_putchar - Write single character to serial port
 */
void serial_putchar(char c);

/*
 * serial_puts - Write string to serial port
 */
void serial_puts(const char *s);

/*
 * serial_write - Write buffer to serial port
 */
void serial_write(const char *buf, size_t len);

/*
 * serial_read - Read character from serial port (blocking)
 */
char serial_read(void);

/*
 * serial_available - Check if data is available to read
 */
int serial_available(void);

#endif /* _ASTRA_DRIVERS_SERIAL_H */
