#include "mouse.h"
#include "io.h"
#include "terminal.h"
#include <string.h>

static mouse_state_t mouse_state = {0};

static inline int mouse_can_read_from_mouse(void) {
    uint8_t status = inb(PS2_STATUS_PORT);
    return (status & STATUS_OUTPUT_FULL) && (status & STATUS_OUTPUT_FROM_MOUSE);
}

static void mouse_write(uint8_t command) {
    mouse_wait(1);
    outb(PS2_COMMAND_PORT, PS2_CMD_WRITE_TO_MOUSE);
    mouse_wait(1);
    outb(PS2_DATA_PORT, command);
}

static uint8_t mouse_read(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if (mouse_can_read_from_mouse()) {
            return inb(PS2_DATA_PORT);
        }
    }
    return 0;
}

void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    while (timeout--) {
        uint8_t status = inb(PS2_STATUS_PORT);
        
        if (type == 0) {
            if (status & STATUS_OUTPUT_FULL) {
                return;
            }
        } else {
            if (!(status & STATUS_INPUT_FULL)) {
                return;
            }
        }
    }
}

static void enable_scroll_wheel(void) {
    mouse_write(MOUSE_CMD_SET_SAMPLE_RATE);
    mouse_read(); // ACK
    mouse_write(200);
    mouse_read(); // ACK
    
    mouse_write(MOUSE_CMD_SET_SAMPLE_RATE);
    mouse_read(); // ACK
    mouse_write(100);
    mouse_read(); // ACK
    
    mouse_write(MOUSE_CMD_SET_SAMPLE_RATE);
    mouse_read(); // ACK
    mouse_write(80);
    mouse_read(); // ACK
    
    mouse_write(MOUSE_CMD_GET_DEVICE_ID);
    mouse_read(); // ACK
    uint8_t device_id = mouse_read();
    
    if (device_id == 0x03) {
        mouse_state.packet_size = 4; // wheel present
    } else {
        mouse_state.packet_size = 3;
    }
}

void mouse_init(void) {
    uint8_t status;
    
    memset(&mouse_state, 0, sizeof(mouse_state));
    mouse_state.packet_size = 3;
    /* Стартовая позиция мыши — центр экрана в символах */
    size_t cols = 80, rows = 25;
    terminal_get_size(&cols, &rows);
    mouse_state.col = cols / 2;
    mouse_state.row = rows / 2;
    
    mouse_wait(1);
    outb(PS2_COMMAND_PORT, 0xA8);
    
    mouse_wait(1);
    outb(PS2_COMMAND_PORT, 0x20);
    mouse_wait(0);
    status = inb(PS2_DATA_PORT);
    status |= 0x02; // mouse IRQ
    status |= 0x01; // keyboard IRQ
    mouse_wait(1);
    outb(PS2_COMMAND_PORT, 0x60);
    mouse_wait(1);
    outb(PS2_DATA_PORT, status);
    
    mouse_write(MOUSE_CMD_SET_STREAM_MODE);
    mouse_read(); // ACK
    
    enable_scroll_wheel();
    
    mouse_write(MOUSE_CMD_ENABLE);
    mouse_read(); // ACK
    
    mouse_state.initialized = 1;
}

void mouse_handler(void) {
    static uint8_t mouse_cycle = 0;
    static uint8_t packet_size = 3;
    static uint8_t mouse_packet[4];
    
    if (!mouse_state.initialized) {
        return;
    }
    packet_size = mouse_state.packet_size ? mouse_state.packet_size : 3;

    if (!mouse_can_read_from_mouse()) {
        return;
    }

    uint8_t data = inb(PS2_DATA_PORT);
    
    if (mouse_cycle == 0) {
        if ((data & 0x08) == 0) {
            return;
        }
    }

    mouse_packet[mouse_cycle++] = data;
    if (mouse_cycle < packet_size) {
        return;
    }
    mouse_cycle = 0;

    uint8_t b0 = mouse_packet[0];
    uint8_t b1 = mouse_packet[1];
    uint8_t b2 = mouse_packet[2];

    if (b0 & 0xC0) {
        return;
    }

    int32_t dx = (int32_t)((int8_t)b1);
    int32_t dy = (int32_t)((int8_t)b2);

    mouse_state.buttons = b0 & 0x07;
    mouse_state.x += dx;
    mouse_state.y -= dy; /* screen coords: up is negative dy */

    /* Prevent runaway growth of raw coords */
    const int32_t RAW_LIMIT = 32767;
    if (mouse_state.x > RAW_LIMIT) mouse_state.x = RAW_LIMIT;
    if (mouse_state.x < -RAW_LIMIT) mouse_state.x = -RAW_LIMIT;
    if (mouse_state.y > RAW_LIMIT) mouse_state.y = RAW_LIMIT;
    if (mouse_state.y < -RAW_LIMIT) mouse_state.y = -RAW_LIMIT;

    /* Update character-cell cursor */
    size_t cols = 80, rows = 25;
    terminal_get_size(&cols, &rows);
    int32_t new_col = (int32_t)mouse_state.col + (dx / 8);
    int32_t new_row = (int32_t)mouse_state.row - (dy / 8);
    if (new_col < 0) new_col = 0;
    if (new_col >= (int32_t)cols) new_col = (int32_t)cols - 1;
    if (new_row < 0) new_row = 0;
    if (new_row >= (int32_t)rows) new_row = (int32_t)rows - 1;
    mouse_state.col = (size_t)new_col;
    mouse_state.row = (size_t)new_row;

    if (packet_size == 4) {
        int8_t wheel = (int8_t)(mouse_packet[3] & 0x0F);
        if (wheel & 0x08) {
            wheel |= (int8_t)0xF0; // sign-extend
        }

        if (wheel != 0) {
            mouse_state.scroll += (int32_t)wheel;
            if (wheel > 0) {
                terminal_scroll_up((size_t)wheel);
            } else {
                terminal_scroll_down((size_t)(-wheel));
            }
        }
    }
}

mouse_state_t get_mouse_state(void) {
    return mouse_state;
}

mouse_state_t get_mouse_state_and_clear_scroll(void) {
    mouse_state_t s = mouse_state;
    mouse_state.scroll = 0;
    return s;
}