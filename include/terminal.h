#ifndef _MYOS_TERMINAL_H
#define _MYOS_TERMINAL_H

#include <stdint.h>
#include <stddef.h>

enum terminal_color {
    TERMINAL_COLOR_BLACK = 0,
    TERMINAL_COLOR_BLUE = 1,
    TERMINAL_COLOR_GREEN = 2,
    TERMINAL_COLOR_CYAN = 3,
    TERMINAL_COLOR_RED = 4,
    TERMINAL_COLOR_MAGENTA = 5,
    TERMINAL_COLOR_BROWN = 6,
    TERMINAL_COLOR_LIGHT_GREY = 7,
    TERMINAL_COLOR_DARK_GREY = 8,
    TERMINAL_COLOR_LIGHT_BLUE = 9,
    TERMINAL_COLOR_LIGHT_GREEN = 10,
    TERMINAL_COLOR_LIGHT_CYAN = 11,
    TERMINAL_COLOR_LIGHT_RED = 12,
    TERMINAL_COLOR_LIGHT_MAGENTA = 13,
    TERMINAL_COLOR_LIGHT_BROWN = 14,
    TERMINAL_COLOR_WHITE = 15,
};

void terminal_initialize(void);
void terminal_set_color(enum terminal_color fg, enum terminal_color bg);
void terminal_write(const char *data);
void terminal_write_line(const char *data);
void terminal_putc(char c);

#endif /* _MYOS_TERMINAL_H */

