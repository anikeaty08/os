/*
 * AstraOS - PS/2 Keyboard Driver Implementation
 *
 * IMPORTANT: IRQ handler is lightweight per design constraints.
 * It only reads the scancode and queues it.
 * Character processing happens in keyboard_getchar().
 */

#include "keyboard.h"
#include "../arch/x86_64/io.h"
#include "../arch/x86_64/irq.h"
#include "../arch/x86_64/cpu.h"

/*
 * Keyboard buffer
 */
#define KBD_BUFFER_SIZE 256
static volatile uint8_t kbd_buffer[KBD_BUFFER_SIZE];
static volatile uint32_t kbd_buffer_head = 0;
static volatile uint32_t kbd_buffer_tail = 0;

/*
 * Modifier key states
 */
static volatile bool shift_pressed = false;
static volatile bool ctrl_pressed = false;
static volatile bool alt_pressed = false;
static volatile bool capslock_on = false;

/*
 * US QWERTY scancode to ASCII mapping (lowercase)
 */
static const char scancode_to_ascii[128] = {
    0,    0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*',  0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   '-', 0,   0,   0,   '+', 0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0
};

/*
 * Shifted scancode to ASCII mapping
 */
static const char scancode_to_ascii_shift[128] = {
    0,    0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*',  0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   '-', 0,   0,   0,   '+', 0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0
};

/*
 * Keyboard IRQ handler - MUST BE FAST!
 * Only reads scancode and adds to buffer
 */
static void keyboard_irq_handler(uint8_t irq) {
    (void)irq;

    /* Read scancode from data port */
    uint8_t scancode = inb(KBD_DATA_PORT);

    /* Add to buffer if space available */
    uint32_t next_head = (kbd_buffer_head + 1) % KBD_BUFFER_SIZE;
    if (next_head != kbd_buffer_tail) {
        kbd_buffer[kbd_buffer_head] = scancode;
        kbd_buffer_head = next_head;
    }
    /* If buffer full, scancode is dropped */
}

/*
 * Initialize keyboard
 */
void keyboard_init(void) {
    /* Clear buffer */
    kbd_buffer_head = 0;
    kbd_buffer_tail = 0;

    /* Flush keyboard buffer */
    while (inb(KBD_STATUS_PORT) & 0x01) {
        inb(KBD_DATA_PORT);
    }

    /* Register IRQ handler */
    irq_register(IRQ_KEYBOARD, keyboard_irq_handler);

    /* Enable keyboard IRQ */
    irq_enable(IRQ_KEYBOARD);
}

/*
 * Get raw scancode (non-blocking)
 */
uint8_t keyboard_get_scancode(void) {
    if (kbd_buffer_head == kbd_buffer_tail) {
        return 0;  /* No data */
    }

    uint8_t scancode = kbd_buffer[kbd_buffer_tail];
    kbd_buffer_tail = (kbd_buffer_tail + 1) % KBD_BUFFER_SIZE;
    return scancode;
}

/*
 * Check if key available
 */
bool keyboard_has_key(void) {
    return kbd_buffer_head != kbd_buffer_tail;
}

/*
 * Process scancode and update modifier states
 * Returns ASCII character or 0 for non-printable
 */
static char process_scancode(uint8_t scancode) {
    /* Key release (high bit set) */
    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;

        switch (released) {
            case KEY_LSHIFT:
            case KEY_RSHIFT:
                shift_pressed = false;
                break;
            case KEY_LCTRL:
                ctrl_pressed = false;
                break;
            case KEY_LALT:
                alt_pressed = false;
                break;
        }
        return 0;
    }

    /* Key press */
    switch (scancode) {
        case KEY_LSHIFT:
        case KEY_RSHIFT:
            shift_pressed = true;
            return 0;
        case KEY_LCTRL:
            ctrl_pressed = true;
            return 0;
        case KEY_LALT:
            alt_pressed = true;
            return 0;
        case KEY_CAPSLOCK:
            capslock_on = !capslock_on;
            return 0;
    }

    /* Get ASCII character */
    char c;
    if (shift_pressed) {
        c = scancode_to_ascii_shift[scancode];
    } else {
        c = scancode_to_ascii[scancode];
    }

    /* Apply capslock to letters */
    if (capslock_on && c >= 'a' && c <= 'z') {
        c -= 32;  /* To uppercase */
    } else if (capslock_on && c >= 'A' && c <= 'Z') {
        c += 32;  /* To lowercase (shift + capslock) */
    }

    return c;
}

/*
 * Get character (blocking)
 */
char keyboard_getchar(void) {
    while (1) {
        /* Wait for scancode */
        while (!keyboard_has_key()) {
            cpu_hlt();  /* Wait for interrupt */
        }

        uint8_t scancode = keyboard_get_scancode();
        char c = process_scancode(scancode);

        if (c != 0) {
            return c;
        }
        /* Non-printable key, keep waiting */
    }
}

/*
 * Modifier state getters
 */
bool keyboard_shift_pressed(void) {
    return shift_pressed;
}

bool keyboard_ctrl_pressed(void) {
    return ctrl_pressed;
}

bool keyboard_alt_pressed(void) {
    return alt_pressed;
}

bool keyboard_capslock_on(void) {
    return capslock_on;
}
