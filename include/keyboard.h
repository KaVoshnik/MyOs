#ifndef _MYOS_KEYBOARD_H
#define _MYOS_KEYBOARD_H

#include <stdint.h>

void keyboard_init(void);
void keyboard_handle_scancode(uint8_t scancode);

#endif /* _MYOS_KEYBOARD_H */

