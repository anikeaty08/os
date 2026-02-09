/*
 * AstraOS - Serial Port Driver Implementation
 * COM1 serial output for debug messages
 */

#include "serial.h"
#include "../arch/x86_64/io.h"

/* COM1 port registers */
#define COM1_DATA       (SERIAL_COM1 + 0)   /* Data register (R/W) */
#define COM1_INT_EN     (SERIAL_COM1 + 1)   /* Interrupt enable */
#define COM1_FIFO_CTRL  (SERIAL_COM1 + 2)   /* FIFO control */
#define COM1_LINE_CTRL  (SERIAL_COM1 + 3)   /* Line control */
#define COM1_MODEM_CTRL (SERIAL_COM1 + 4)   /* Modem control */
#define COM1_LINE_STAT  (SERIAL_COM1 + 5)   /* Line status */
#define COM1_MODEM_STAT (SERIAL_COM1 + 6)   /* Modem status */
#define COM1_SCRATCH    (SERIAL_COM1 + 7)   /* Scratch register */

/* Divisor registers (when DLAB=1) */
#define COM1_DIV_LOW    (SERIAL_COM1 + 0)
#define COM1_DIV_HIGH   (SERIAL_COM1 + 1)

/* Line status bits */
#define LSR_DATA_READY   0x01
#define LSR_OVERRUN      0x02
#define LSR_PARITY_ERR   0x04
#define LSR_FRAMING_ERR  0x08
#define LSR_BREAK        0x10
#define LSR_THR_EMPTY    0x20   /* Transmit holding register empty */
#define LSR_TSR_EMPTY    0x40   /* Transmitter empty */

/*
 * serial_init - Initialize COM1 serial port
 * 115200 baud, 8N1 configuration
 */
int serial_init(void) {
    /* Disable interrupts */
    outb(COM1_INT_EN, 0x00);

    /* Enable DLAB to set baud rate */
    outb(COM1_LINE_CTRL, 0x80);

    /* Set divisor to 1 (115200 baud) */
    /* Divisor = 115200 / baud_rate */
    /* For 115200: divisor = 1 */
    /* For 38400: divisor = 3 */
    outb(COM1_DIV_LOW, 0x01);
    outb(COM1_DIV_HIGH, 0x00);

    /* 8 bits, no parity, 1 stop bit (8N1) */
    outb(COM1_LINE_CTRL, 0x03);

    /* Enable FIFO, clear buffers, 14-byte threshold */
    outb(COM1_FIFO_CTRL, 0xC7);

    /* Enable DTR, RTS, and OUT2 (required for interrupts) */
    outb(COM1_MODEM_CTRL, 0x0B);

    /* Test serial chip with loopback mode */
    outb(COM1_MODEM_CTRL, 0x1E);  /* Enable loopback */
    outb(COM1_DATA, 0xAE);         /* Send test byte */

    /* Check if we receive the same byte */
    if (inb(COM1_DATA) != 0xAE) {
        return -1;  /* Serial port faulty */
    }

    /* Disable loopback, set normal operation */
    outb(COM1_MODEM_CTRL, 0x0F);

    return 0;
}

/*
 * Wait for transmit buffer to be empty
 */
static inline void serial_wait_tx_ready(void) {
    while ((inb(COM1_LINE_STAT) & LSR_THR_EMPTY) == 0) {
        /* Busy wait */
    }
}

/*
 * serial_putchar - Write single character
 */
void serial_putchar(char c) {
    serial_wait_tx_ready();
    outb(COM1_DATA, c);
}

/*
 * serial_puts - Write null-terminated string
 */
void serial_puts(const char *s) {
    while (*s) {
        if (*s == '\n') {
            serial_putchar('\r');  /* CR before LF */
        }
        serial_putchar(*s++);
    }
}

/*
 * serial_write - Write buffer of specified length
 */
void serial_write(const char *buf, size_t len) {
    while (len--) {
        if (*buf == '\n') {
            serial_putchar('\r');
        }
        serial_putchar(*buf++);
    }
}

/*
 * serial_available - Check if data is available
 */
int serial_available(void) {
    return (inb(COM1_LINE_STAT) & LSR_DATA_READY) != 0;
}

/*
 * serial_read - Read character (blocking)
 */
char serial_read(void) {
    while (!serial_available()) {
        /* Busy wait */
    }
    return inb(COM1_DATA);
}
