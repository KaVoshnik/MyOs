#include <terminal.h>
#include <string.h>
#include <io.h>
#include <stdint.h>

static volatile uint16_t *const VGA_MEMORY = (uint16_t *)0xB8000;
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

static size_t terminal_row = 0;
static size_t terminal_column = 0;
static uint8_t terminal_color = (TERMINAL_COLOR_LIGHT_GREY | (TERMINAL_COLOR_BLACK << 4));
static uint8_t terminal_default_color = (TERMINAL_COLOR_LIGHT_GREY | (TERMINAL_COLOR_BLACK << 4));
static int terminal_bold = 0;
static int terminal_cursor_visible = 1;

static inline uint8_t make_color(enum terminal_color fg, enum terminal_color bg) {
    return (uint8_t)(fg | (bg << 4));
}

static inline uint16_t make_vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static void terminal_update_cursor(void) {
    uint16_t position = (uint16_t)(terminal_row * VGA_WIDTH + terminal_column);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(position & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((position >> 8) & 0xFF));
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
    terminal_update_cursor();
}

void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = make_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);
    terminal_default_color = terminal_color;
    terminal_bold = 0;
    terminal_cursor_visible = 1;

    for (size_t row = 0; row < VGA_HEIGHT; ++row) {
        for (size_t col = 0; col < VGA_WIDTH; ++col) {
            VGA_MEMORY[row * VGA_WIDTH + col] = make_vga_entry(' ', terminal_color);
        }
    }
    terminal_update_cursor();
}

void terminal_set_color(enum terminal_color fg, enum terminal_color bg) {
    terminal_color = make_color(fg, bg);
    terminal_default_color = terminal_color;
    terminal_bold = 0;
}

void terminal_clear(void) {
    for (size_t row = 0; row < VGA_HEIGHT; ++row) {
        for (size_t col = 0; col < VGA_WIDTH; ++col) {
            VGA_MEMORY[row * VGA_WIDTH + col] = make_vga_entry(' ', terminal_color);
        }
    }
    terminal_row = 0;
    terminal_column = 0;
    terminal_update_cursor();
}

static void terminal_newline(void) {
    terminal_column = 0;
    if (++terminal_row >= VGA_HEIGHT) {
        terminal_scroll();
        return;
    }
    terminal_update_cursor();
}

static enum terminal_color ansi_to_terminal_color(int ansi_code, int is_bright) {
    /* ANSI colors: 30-37 normal, 90-97 bright */
    int base = ansi_code % 10;
    if (is_bright && base < 8) {
        base += 8; /* Convert to bright colors */
    }
    
    switch (base) {
        case 0: return TERMINAL_COLOR_BLACK;
        case 1: return TERMINAL_COLOR_BLUE;
        case 2: return TERMINAL_COLOR_GREEN;
        case 3: return TERMINAL_COLOR_CYAN;
        case 4: return TERMINAL_COLOR_RED;
        case 5: return TERMINAL_COLOR_MAGENTA;
        case 6: return TERMINAL_COLOR_BROWN;
        case 7: return TERMINAL_COLOR_LIGHT_GREY;
        case 8: return TERMINAL_COLOR_DARK_GREY;
        case 9: return TERMINAL_COLOR_LIGHT_BLUE;
        case 10: return TERMINAL_COLOR_LIGHT_GREEN;
        case 11: return TERMINAL_COLOR_LIGHT_CYAN;
        case 12: return TERMINAL_COLOR_LIGHT_RED;
        case 13: return TERMINAL_COLOR_LIGHT_MAGENTA;
        case 14: return TERMINAL_COLOR_LIGHT_BROWN;
        case 15: return TERMINAL_COLOR_WHITE;
        default: return TERMINAL_COLOR_LIGHT_GREY;
    }
}

static void terminal_apply_ansi_code(int code) {
    if (code == 0) {
        /* Reset all attributes */
        terminal_color = terminal_default_color;
        terminal_bold = 0;
        return;
    }
    
    if (code == 1) {
        /* Bold */
        terminal_bold = 1;
        /* Make foreground color bright if it's not already */
        enum terminal_color fg = (enum terminal_color)(terminal_color & 0x0F);
        enum terminal_color bg = (enum terminal_color)((terminal_color >> 4) & 0x0F);
        if (fg < 8) {
            fg = (enum terminal_color)(fg + 8);
        }
        terminal_color = make_color(fg, bg);
        return;
    }
    
    if (code == 3) {
        /* Italic - not supported by VGA, ignore */
        return;
    }
    
    if (code == 4) {
        /* Underline - not directly supported, use different color */
        return;
    }
    
    if (code == 7) {
        /* Invert colors */
        enum terminal_color fg = (enum terminal_color)(terminal_color & 0x0F);
        enum terminal_color bg = (enum terminal_color)((terminal_color >> 4) & 0x0F);
        terminal_color = make_color(bg, fg);
        return;
    }
    
    if (code >= 30 && code <= 37) {
        /* Foreground color (normal) */
        enum terminal_color fg = ansi_to_terminal_color(code, 0);
        enum terminal_color bg = (enum terminal_color)((terminal_color >> 4) & 0x0F);
        if (terminal_bold && fg < 8) {
            fg = (enum terminal_color)(fg + 8);
        }
        terminal_color = make_color(fg, bg);
        return;
    }
    
    if (code >= 40 && code <= 47) {
        /* Background color */
        enum terminal_color fg = (enum terminal_color)(terminal_color & 0x0F);
        enum terminal_color bg = ansi_to_terminal_color(code - 10, 0);
        terminal_color = make_color(fg, bg);
        return;
    }
    
    if (code >= 90 && code <= 97) {
        /* Bright foreground color */
        enum terminal_color fg = ansi_to_terminal_color(code, 1);
        enum terminal_color bg = (enum terminal_color)((terminal_color >> 4) & 0x0F);
        terminal_color = make_color(fg, bg);
        return;
    }
    
    if (code >= 100 && code <= 107) {
        /* Bright background color */
        enum terminal_color fg = (enum terminal_color)(terminal_color & 0x0F);
        enum terminal_color bg = ansi_to_terminal_color(code - 10, 1);
        terminal_color = make_color(fg, bg);
        return;
    }
}

static void terminal_clear_line_from_cursor(void) {
    for (size_t col = terminal_column; col < VGA_WIDTH; ++col) {
        VGA_MEMORY[terminal_row * VGA_WIDTH + col] = make_vga_entry(' ', terminal_color);
    }
}

static void terminal_clear_line_to_cursor(void) {
    for (size_t col = 0; col <= terminal_column; ++col) {
        VGA_MEMORY[terminal_row * VGA_WIDTH + col] = make_vga_entry(' ', terminal_color);
    }
}

static void terminal_clear_entire_line(void) {
    for (size_t col = 0; col < VGA_WIDTH; ++col) {
        VGA_MEMORY[terminal_row * VGA_WIDTH + col] = make_vga_entry(' ', terminal_color);
    }
}

static int terminal_parse_ansi_sequence(const char **data_ptr) {
    const char *data = *data_ptr;
    
    /* Check for ESC [ */
    if (data[0] != '\x1B' || data[1] != '[') {
        return 0;
    }
    
    data += 2; /* Skip ESC [ */
    
    /* Parse parameters */
    int params[16] = {0};
    int param_count = 0;
    int current_param = 0;
    int has_param = 0;
    
    while (*data && param_count < 16) {
        if (*data >= '0' && *data <= '9') {
            current_param = current_param * 10 + (*data - '0');
            has_param = 1;
        } else if (*data == ';') {
            if (has_param) {
                params[param_count++] = current_param;
            } else {
                params[param_count++] = 0; /* Default parameter */
            }
            current_param = 0;
            has_param = 0;
        } else {
            /* End of sequence */
            if (has_param) {
                params[param_count++] = current_param;
            } else if (param_count == 0) {
                params[param_count++] = 0; /* Default parameter */
            }
            break;
        }
        ++data;
    }
    
    if (param_count == 0) {
        params[param_count++] = 0;
    }
    
    char command = *data;
    if (!command) {
        return 0;
    }
    ++data;
    
    /* Handle different ANSI commands */
    switch (command) {
        case 'm': {
            /* SGR - Set Graphics Rendition */
            for (int i = 0; i < param_count; ++i) {
                terminal_apply_ansi_code(params[i]);
            }
            *data_ptr = data;
            return 1;
        }
        
        case 'A': {
            /* Cursor Up */
            int n = params[0] ? params[0] : 1;
            if (terminal_row >= (size_t)n) {
                terminal_row -= n;
            } else {
                terminal_row = 0;
            }
            terminal_update_cursor();
            *data_ptr = data;
            return 1;
        }
        
        case 'B': {
            /* Cursor Down */
            int n = params[0] ? params[0] : 1;
            terminal_row += n;
            if (terminal_row >= VGA_HEIGHT) {
                terminal_row = VGA_HEIGHT - 1;
            }
            terminal_update_cursor();
            *data_ptr = data;
            return 1;
        }
        
        case 'C': {
            /* Cursor Forward */
            int n = params[0] ? params[0] : 1;
            terminal_column += n;
            if (terminal_column >= VGA_WIDTH) {
                terminal_column = VGA_WIDTH - 1;
            }
            terminal_update_cursor();
            *data_ptr = data;
            return 1;
        }
        
        case 'D': {
            /* Cursor Backward */
            int n = params[0] ? params[0] : 1;
            if (terminal_column >= (size_t)n) {
                terminal_column -= n;
            } else {
                terminal_column = 0;
            }
            terminal_update_cursor();
            *data_ptr = data;
            return 1;
        }
        
        case 'H':
        case 'f': {
            /* Cursor Position */
            int row = params[0] ? params[0] : 1;
            int col = params[1] ? params[1] : 1;
            if (row > 0 && col > 0) {
                terminal_set_cursor((size_t)(row - 1), (size_t)(col - 1));
            }
            *data_ptr = data;
            return 1;
        }
        
        case 'J': {
            /* Erase Display */
            int mode = params[0];
            if (mode == 0 || mode == 1) {
                /* Clear from cursor to end/beginning of screen */
                if (mode == 0) {
                    /* Clear from cursor to end */
                    terminal_clear_line_from_cursor();
                    for (size_t r = terminal_row + 1; r < VGA_HEIGHT; ++r) {
                        for (size_t c = 0; c < VGA_WIDTH; ++c) {
                            VGA_MEMORY[r * VGA_WIDTH + c] = make_vga_entry(' ', terminal_color);
                        }
                    }
                } else {
                    /* Clear from beginning to cursor */
                    terminal_clear_line_to_cursor();
                    for (size_t r = 0; r < terminal_row; ++r) {
                        for (size_t c = 0; c < VGA_WIDTH; ++c) {
                            VGA_MEMORY[r * VGA_WIDTH + c] = make_vga_entry(' ', terminal_color);
                        }
                    }
                }
            } else if (mode == 2) {
                /* Clear entire screen */
                terminal_clear();
            }
            *data_ptr = data;
            return 1;
        }
        
        case 'K': {
            /* Erase Line */
            int mode = params[0];
            if (mode == 0) {
                terminal_clear_line_from_cursor();
            } else if (mode == 1) {
                terminal_clear_line_to_cursor();
            } else if (mode == 2) {
                terminal_clear_entire_line();
            }
            *data_ptr = data;
            return 1;
        }
        
        case '?': {
            /* Private mode sequences */
            if (*data == '2' && data[1] == '5') {
                data += 2;
                if (*data == 'l') {
                    /* Hide cursor */
                    terminal_cursor_visible = 0;
                    /* Note: VGA cursor visibility is controlled by hardware */
                } else if (*data == 'h') {
                    /* Show cursor */
                    terminal_cursor_visible = 1;
                }
                ++data;
                *data_ptr = data;
                return 1;
            }
            return 0;
        }
        
        default:
            /* Unknown command, skip the sequence */
            *data_ptr = data;
            return 1;
    }
}

void terminal_putc(char c) {
    if (c == '\n') {
        terminal_newline();
        return;
    }

    if (c == '\b') {
        if (terminal_column > 0) {
            --terminal_column;
        } else if (terminal_row > 0) {
            --terminal_row;
            terminal_column = VGA_WIDTH - 1;
        }
        VGA_MEMORY[terminal_row * VGA_WIDTH + terminal_column] = make_vga_entry(' ', terminal_color);
        terminal_update_cursor();
        return;
    }

    VGA_MEMORY[terminal_row * VGA_WIDTH + terminal_column] = make_vga_entry(c, terminal_color);

    if (++terminal_column >= VGA_WIDTH) {
        terminal_newline();
        return;
    }
    terminal_update_cursor();
}

void terminal_write(const char *data) {
    while (*data) {
        /* Check for ANSI escape sequence */
        if (*data == '\x1B' && data[1] == '[') {
            if (terminal_parse_ansi_sequence(&data)) {
                continue; /* Sequence was processed */
            }
        }
        
        terminal_putc(*data++);
    }
}

void terminal_write_line(const char *data) {
    terminal_write(data);
    terminal_putc('\n');
}

void terminal_get_cursor(size_t *row, size_t *column) {
    if (row) {
        *row = terminal_row;
    }
    if (column) {
        *column = terminal_column;
    }
}

void terminal_set_cursor(size_t row, size_t column) {
    if (row >= VGA_HEIGHT) {
        row = VGA_HEIGHT - 1;
    }
    if (column >= VGA_WIDTH) {
        column = VGA_WIDTH - 1;
    }
    terminal_row = row;
    terminal_column = column;
    terminal_update_cursor();
}

