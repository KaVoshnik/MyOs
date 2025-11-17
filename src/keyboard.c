#include <keyboard.h>
#include <terminal.h>

static const char keymap[128] = {
    0, 27, '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, '7',
    '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.'
};

void keyboard_init(void) {
    terminal_write_line("[kbd] Keyboard driver ready");
}

void keyboard_handle_scancode(uint8_t scancode) {
    if (scancode & 0x80) {
        return; /* key release */
    }

    char c = 0;
    if (scancode < sizeof(keymap)) {
        c = keymap[scancode];
    }

    if (c) {
        terminal_putc(c);
    }
}

