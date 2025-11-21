#include <keyboard.h>
#include <terminal.h>
#include <stddef.h>

#define KEYBOARD_BUFFER_SIZE 128

static const char keymap_lower[128] = {
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

static const char keymap_upper[128] = {
    0, 27, '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}', '\n', 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*',
    0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, '7',
    '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.'
};

static volatile uint16_t key_buffer[KEYBOARD_BUFFER_SIZE];
static volatile size_t buffer_head = 0;
static volatile size_t buffer_tail = 0;
static int shift_pressed = 0;
static int ctrl_pressed = 0;
static int e0_sequence = 0;

static void buffer_push(uint16_t code) {
    size_t next_head = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
    if (next_head == buffer_tail) {
        return; /* buffer full, drop */
    }
    key_buffer[buffer_head] = code;
    buffer_head = next_head;
}

static int buffer_pop(uint16_t *code) {
    if (buffer_head == buffer_tail) {
        return 0;
    }
    *code = key_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return 1;
}

void keyboard_init(void) {
    shift_pressed = 0;
    ctrl_pressed = 0;
    e0_sequence = 0;
    buffer_head = buffer_tail = 0;
    terminal_write_line("[kbd] Keyboard driver ready");
}

void keyboard_handle_scancode(uint8_t scancode) {
    if (scancode == 0xE0) {
        e0_sequence = 1;
        return;
    }

    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        return;
    }

    if (scancode == 0x1D) {
        ctrl_pressed = 1;
        return;
    }
    if (scancode == 0x9D) {
        ctrl_pressed = 0;
        return;
    }

    if (scancode & 0x80) {
        e0_sequence = 0;
        return; /* key release */
    }

    if (e0_sequence) {
        e0_sequence = 0;
        switch (scancode) {
            case 0x48: buffer_push(KEY_SPECIAL_UP); return;
            case 0x50: buffer_push(KEY_SPECIAL_DOWN); return;
            case 0x4B: buffer_push(KEY_SPECIAL_LEFT); return;
            case 0x4D: buffer_push(KEY_SPECIAL_RIGHT); return;
            default: return;
        }
    }

    if (scancode == 0x0F) {
        buffer_push(KEY_SPECIAL_TAB);
        return;
    }

    if (ctrl_pressed && scancode == 0x13) {
        buffer_push(KEY_SPECIAL_CTRL_R);
        return;
    }

    char c = 0;
    if (scancode < sizeof(keymap_lower)) {
        c = shift_pressed ? keymap_upper[scancode] : keymap_lower[scancode];
    }

    if (c) {
        buffer_push((uint16_t)(unsigned char)c);
    }
}

int keyboard_read_char(char *out_char) {
    uint16_t code;
    while (!buffer_pop(&code)) {
        __asm__ volatile("hlt");
    }
    if (code < 256) {
        *out_char = (char)code;
        return 1;
    }
    *out_char = 0;
    return 0;
}

int keyboard_read_char_extended(uint16_t *out_char) {
    while (!buffer_pop(out_char)) {
        __asm__ volatile("hlt");
    }
    return 1;
}

int keyboard_try_read_char_extended(uint16_t *out_char) {
    return buffer_pop(out_char);
}

size_t keyboard_read_line(char *buffer, size_t buffer_size) {
    if (buffer_size == 0) {
        return 0;
    }

    size_t length = 0;
    while (1) {
        char c;
        keyboard_read_char(&c);

        if (c == '\r') {
            c = '\n';
        }

        if (c == '\b') {
            if (length > 0) {
                --length;
                terminal_putc('\b');
                terminal_putc(' ');
                terminal_putc('\b');
            }
            continue;
        }

        if (c == '\n') {
            terminal_putc('\n');
            buffer[length] = '\0';
            return length;
        }

        if (length + 1 < buffer_size) {
            buffer[length++] = c;
            terminal_putc(c);
        }
    }
}

