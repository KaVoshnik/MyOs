#ifndef _MYOS_KEYBOARD_H
#define _MYOS_KEYBOARD_H

#include <stdint.h>
#include <stddef.h>

#define KEY_SPECIAL_UP     0x100
#define KEY_SPECIAL_DOWN   0x101
#define KEY_SPECIAL_LEFT  0x102
#define KEY_SPECIAL_RIGHT 0x103
#define KEY_SPECIAL_TAB   0x104
#define KEY_SPECIAL_CTRL_R 0x105

void keyboard_init(void);
void keyboard_handle_scancode(uint8_t scancode);
int keyboard_read_char(char *out_char);
int keyboard_read_char_extended(uint16_t *out_char);
size_t keyboard_read_line(char *buffer, size_t buffer_size);
size_t keyboard_read_line_with_history(char *buffer, size_t buffer_size, 
                                       const char **history, size_t history_size, 
                                       size_t *history_index, int *tab_complete);

#endif /* _MYOS_KEYBOARD_H */

