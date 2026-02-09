/*
 * AstraOS - PS/2 Keyboard Driver Header
 */

#ifndef _ASTRA_DRIVERS_KEYBOARD_H
#define _ASTRA_DRIVERS_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Keyboard ports
 */
#define KBD_DATA_PORT   0x60
#define KBD_STATUS_PORT 0x64
#define KBD_COMMAND_PORT 0x64

/*
 * Special key codes
 */
#define KEY_ESCAPE      0x01
#define KEY_BACKSPACE   0x0E
#define KEY_TAB         0x0F
#define KEY_ENTER       0x1C
#define KEY_LCTRL       0x1D
#define KEY_LSHIFT      0x2A
#define KEY_RSHIFT      0x36
#define KEY_LALT        0x38
#define KEY_CAPSLOCK    0x3A
#define KEY_F1          0x3B
#define KEY_F2          0x3C
#define KEY_F3          0x3D
#define KEY_F4          0x3E
#define KEY_F5          0x3F
#define KEY_F6          0x40
#define KEY_F7          0x41
#define KEY_F8          0x42
#define KEY_F9          0x43
#define KEY_F10         0x44
#define KEY_F11         0x57
#define KEY_F12         0x58
#define KEY_NUMLOCK     0x45
#define KEY_SCROLLLOCK  0x46
#define KEY_HOME        0x47
#define KEY_UP          0x48
#define KEY_PAGEUP      0x49
#define KEY_LEFT        0x4B
#define KEY_RIGHT       0x4D
#define KEY_END         0x4F
#define KEY_DOWN        0x50
#define KEY_PAGEDOWN    0x51
#define KEY_INSERT      0x52
#define KEY_DELETE      0x53

/*
 * Initialize keyboard driver
 */
void keyboard_init(void);

/*
 * Get character from keyboard (blocking)
 * Waits until a key is pressed
 */
char keyboard_getchar(void);

/*
 * Check if a key is available
 */
bool keyboard_has_key(void);

/*
 * Get raw scancode (non-blocking)
 * Returns 0 if no key available
 */
uint8_t keyboard_get_scancode(void);

/*
 * Check modifier key states
 */
bool keyboard_shift_pressed(void);
bool keyboard_ctrl_pressed(void);
bool keyboard_alt_pressed(void);
bool keyboard_capslock_on(void);

#endif /* _ASTRA_DRIVERS_KEYBOARD_H */
