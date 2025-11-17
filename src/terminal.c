#include <terminal.h>
#include <string.h>

static volatile uint16_t *const VGA_MEMORY = (uint16_t *)0xB8000;
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

static size_t terminal_row = 0;
static size_t terminal_column = 0;
static uint8_t terminal_color = (TERMINAL_COLOR_LIGHT_GREY | (TERMINAL_COLOR_BLACK << 4));

static inline uint8_t make_color(enum terminal_color fg, enum terminal_color bg) {
    return (uint8_t)(fg | (bg << 4));
}

static inline uint16_t make_vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static void terminal_scroll(void) {
    for (size_t row = 1; row < VGA_HEIGHT; ++row) {
        for (size_t col = 0; col < VGA_WIDTH; ++col) {
            VGA_MEMORY[(row - 1) * VGA_WIDTH + col] = VGA_MEMORY[row * VGA_WIDTH + col];
        }
    }

    for (size_t col = 0; col < VGA_WIDTH; ++col) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = make_vga_entry(' ', terminal_color);
    }

    terminal_row = VGA_HEIGHT - 1;
    terminal_column = 0;
}

void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = make_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);

    for (size_t row = 0; row < VGA_HEIGHT; ++row) {
        for (size_t col = 0; col < VGA_WIDTH; ++col) {
            VGA_MEMORY[row * VGA_WIDTH + col] = make_vga_entry(' ', terminal_color);
        }
    }
}

void terminal_set_color(enum terminal_color fg, enum terminal_color bg) {
    terminal_color = make_color(fg, bg);
}

static void terminal_newline(void) {
    terminal_column = 0;
    if (++terminal_row >= VGA_HEIGHT) {
        terminal_scroll();
    }
}

void terminal_putc(char c) {
    if (c == '\n') {
        terminal_newline();
        return;
    }

    VGA_MEMORY[terminal_row * VGA_WIDTH + terminal_column] = make_vga_entry(c, terminal_color);

    if (++terminal_column >= VGA_WIDTH) {
        terminal_newline();
    }
}

void terminal_write(const char *data) {
    while (*data) {
        terminal_putc(*data++);
    }
}

void terminal_write_line(const char *data) {
    terminal_write(data);
    terminal_putc('\n');
}

