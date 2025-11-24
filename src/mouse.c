#include <mouse.h>
#include <io.h>
#include <terminal.h>
#include <string.h>

#define MOUSE_DATA_PORT    0x60
#define MOUSE_STATUS_PORT  0x64
#define MOUSE_COMMAND_PORT 0x64

#define MOUSE_BUFFER_SIZE  32

/* PS/2 Mouse Commands */
#define MOUSE_CMD_RESET           0xFF
#define MOUSE_CMD_ENABLE          0xF4
#define MOUSE_CMD_DISABLE         0xF5
#define MOUSE_CMD_SET_SAMPLE_RATE 0xF3
#define MOUSE_CMD_GET_ID          0xF2
#define MOUSE_CMD_SET_REMOTE_MODE 0xF0
#define MOUSE_CMD_SET_WRAP_MODE   0xEE
#define MOUSE_CMD_RESET_WRAP_MODE 0xEC
#define MOUSE_CMD_READ_DATA       0xEB
#define MOUSE_CMD_SET_STREAM_MODE 0xEA
#define MOUSE_CMD_STATUS_REQUEST  0xE9
#define MOUSE_CMD_SET_RESOLUTION  0xE8

/* Mouse responses */
#define MOUSE_ACK                 0xFA
#define MOUSE_NACK                0xFE
#define MOUSE_ERROR               0xFC

static volatile mouse_event_t mouse_buffer[MOUSE_BUFFER_SIZE];
static volatile size_t mouse_buffer_head = 0;
static volatile size_t mouse_buffer_tail = 0;
static int mouse_available = 0;
static int mouse_packet_index = 0;
static uint8_t mouse_packet[3] = {0};
static int mouse_waiting_ack = 0;

/* static void mouse_wait_input(void) {
    uint32_t timeout = 100000;
    while (timeout-- > 0) {
        if ((inb(MOUSE_STATUS_PORT) & 0x01) != 0) {
            return;
        }
    }
}

static void mouse_wait_output(void) {
    uint32_t timeout = 100000;
    while (timeout-- > 0) {
        if ((inb(MOUSE_STATUS_PORT) & 0x02) == 0) {
            return;
        }
    }
}

static int mouse_send_command(uint8_t command) {
    mouse_wait_output();
    outb(MOUSE_COMMAND_PORT, 0xD4);
    mouse_wait_output();
    outb(MOUSE_DATA_PORT, command);
    
    mouse_wait_input();
    uint8_t response = inb(MOUSE_DATA_PORT);
    
    if (response == MOUSE_ACK) {
        return 0;
    }
    return -1;
} */

static void mouse_buffer_push(const mouse_event_t *event) {
    size_t next_head = (mouse_buffer_head + 1) % MOUSE_BUFFER_SIZE;
    if (next_head == mouse_buffer_tail) {
        return; /* Buffer full, drop event */
    }
    mouse_buffer[mouse_buffer_head] = *event;
    mouse_buffer_head = next_head;
}

/* Public function for interrupt handler */
void mouse_buffer_push_direct(mouse_event_t *event) {
    mouse_buffer_push(event);
}

static int mouse_buffer_pop(mouse_event_t *event) {
    if (mouse_buffer_head == mouse_buffer_tail) {
        return 0; /* Buffer empty */
    }
    *event = mouse_buffer[mouse_buffer_tail];
    mouse_buffer_tail = (mouse_buffer_tail + 1) % MOUSE_BUFFER_SIZE;
    return 1;
}

void mouse_handle_packet(uint8_t packet[3]) {
    if (!packet) {
        return;
    }
    
    uint8_t flags = packet[0];
    int8_t x_delta = (int8_t)packet[1];
    int8_t y_delta = (int8_t)packet[2];
    
    int buttons = 0;
    if (flags & 0x01) {
        buttons |= MOUSE_EVENT_BUTTON_LEFT;
    }
    if (flags & 0x02) {
        buttons |= MOUSE_EVENT_BUTTON_RIGHT;
    }
    if (flags & 0x04) {
        buttons |= MOUSE_EVENT_BUTTON_MIDDLE;
    }
    
    mouse_event_t event = {
        .x = x_delta,
        .y = y_delta,
        .buttons = buttons,
        .scroll = 0
    };
    
    mouse_buffer_push(&event);
}

void mouse_init(void) {
    mouse_available = 0;
    mouse_buffer_head = mouse_buffer_tail = 0;
    mouse_packet_index = 0;
    memset(mouse_packet, 0, sizeof(mouse_packet));
    mouse_waiting_ack = 0;
    
    /* Simple initialization - just enable mouse port and interrupts */
    /* Don't do full reset to avoid blocking or interfering with keyboard */
    
    /* Enable mouse device */
    if ((inb(MOUSE_STATUS_PORT) & 0x02) == 0) {
        outb(MOUSE_COMMAND_PORT, 0xA8); /* Enable mouse */
    }
    
    /* Enable interrupts for mouse in controller config */
    if ((inb(MOUSE_STATUS_PORT) & 0x02) == 0) {
        outb(MOUSE_COMMAND_PORT, 0x20); /* Read controller configuration */
        uint32_t timeout = 100;
        while (timeout-- > 0 && (inb(MOUSE_STATUS_PORT) & 0x01) == 0) {
            /* Wait for data */
        }
        if (timeout > 0) {
            uint8_t config = inb(MOUSE_DATA_PORT);
            config |= 0x02; /* Enable mouse interrupt */
            config |= 0x01; /* Keep keyboard interrupt enabled */
            
            if ((inb(MOUSE_STATUS_PORT) & 0x02) == 0) {
                outb(MOUSE_COMMAND_PORT, 0x60); /* Write controller configuration */
                if ((inb(MOUSE_STATUS_PORT) & 0x02) == 0) {
                    outb(MOUSE_DATA_PORT, config);
                    mouse_available = 1; /* Assume mouse is available */
                }
            }
        }
    }
    
    if (mouse_available) {
        terminal_write_line("[mouse] Mouse driver ready");
    } else {
        terminal_write_line("[mouse] Mouse initialization skipped");
    }
}

int mouse_is_available(void) {
    return mouse_available;
}

int mouse_get_event(mouse_event_t *event) {
    while (!mouse_buffer_pop(event)) {
        __asm__ volatile("hlt");
    }
    return 1;
}

int mouse_try_get_event(mouse_event_t *event) {
    return mouse_buffer_pop(event);
}

