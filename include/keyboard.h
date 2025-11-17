#ifndef _MYOS_KEYBOARD_H
#define _MYOS_KEYBOARD_H

#include <stdint.h>
#include <stddef.h>

void keyboard_init(void);
void keyboard_handle_scancode(uint8_t scancode);
int keyboard_read_char(char *out_char);
size_t keyboard_read_line(char *buffer, size_t buffer_size);

#endif /* _MYOS_KEYBOARD_H */

