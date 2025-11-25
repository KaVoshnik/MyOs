#include <mouse.h>
#include <io.h>
#include <terminal.h>
#include <string.h>
#include <stddef.h>

#define MOUSE_DATA_PORT    0x60
#define MOUSE_STATUS_PORT  0x64
#define MOUSE_COMMAND_PORT 0x64

#define MOUSE_BUFFER_SIZE  32
#define PS2_TIMEOUT        100000U
#define PS2_RETRY_COUNT    3

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
#define MOUSE_CMD_SET_DEFAULTS    0xF6

/* Mouse responses */
#define MOUSE_ACK                 0xFA
#define MOUSE_NACK                0xFE
#define MOUSE_ERROR               0xFC

static volatile mouse_event_t mouse_buffer[MOUSE_BUFFER_SIZE];
static volatile size_t mouse_buffer_head = 0;
static volatile size_t mouse_buffer_tail = 0;
static int mouse_available = 0;
static int mouse_wheel_supported = 0;

static int ps2_wait_can_read(void) {
    for (uint32_t i = 0; i < PS2_TIMEOUT; ++i) {
        if (inb(MOUSE_STATUS_PORT) & 0x01) {
            return 1;
        }
    }
    return 0;
}

static int ps2_wait_can_write(void) {
    for (uint32_t i = 0; i < PS2_TIMEOUT; ++i) {
        if ((inb(MOUSE_STATUS_PORT) & 0x02) == 0) {
            return 1;
        }
    }
    return 0;
}

static void ps2_flush_output(void) {
    for (uint32_t i = 0; i < PS2_TIMEOUT; ++i) {
        if ((inb(MOUSE_STATUS_PORT) & 0x01) == 0) {
            break;
        }
        (void)inb(MOUSE_DATA_PORT);
    }
}

static int ps2_write_command(uint8_t command) {
    if (!ps2_wait_can_write()) {
        return 0;
    }
    outb(MOUSE_COMMAND_PORT, command);
    return 1;
}

static int ps2_write_data(uint8_t data) {
    if (!ps2_wait_can_write()) {
        return 0;
    }
    outb(MOUSE_DATA_PORT, data);
    return 1;
}

static int ps2_read_data(uint8_t *value) {
    if (!ps2_wait_can_read()) {
        return 0;
    }
    *value = inb(MOUSE_DATA_PORT);
    return 1;
}

static int mouse_write_device(uint8_t value) {
    if (!ps2_write_command(0xD4)) {
        return 0;
    }
    return ps2_write_data(value);
}

static int mouse_read_response(uint8_t *value) {
    return ps2_read_data(value);
}

static int mouse_send(uint8_t value) {
    for (int attempt = 0; attempt < PS2_RETRY_COUNT; ++attempt) {
        if (!mouse_write_device(value)) {
            return 0;
        }
        uint8_t response = 0;
        if (!mouse_read_response(&response)) {
            return 0;
        }
        if (response == MOUSE_ACK) {
            return 1;
        }
        if (response != MOUSE_NACK) {
            break;
        }
    }
    return 0;
}

static int mouse_send_with_argument(uint8_t command, uint8_t argument) {
    if (!mouse_send(command)) {
        return 0;
    }
    return mouse_send(argument);
}

static int mouse_get_id(uint8_t *out_id) {
    if (!mouse_send(MOUSE_CMD_GET_ID)) {
        return 0;
    }
    return mouse_read_response(out_id);
}

static void mouse_detect_capabilities(void) {
    mouse_wheel_supported = 0;
    uint8_t id = 0;
    if (mouse_get_id(&id)) {
        if (id == 3 || id == 4) {
            mouse_wheel_supported = 1;
            return;
        }
    }

    if (mouse_send_with_argument(MOUSE_CMD_SET_SAMPLE_RATE, 200) &&
        mouse_send_with_argument(MOUSE_CMD_SET_SAMPLE_RATE, 100) &&
        mouse_send_with_argument(MOUSE_CMD_SET_SAMPLE_RATE, 80) &&
        mouse_get_id(&id) &&
        (id == 3 || id == 4)) {
        mouse_wheel_supported = 1;
    }
}

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

#if ENABLE_MOUSE_DRIVER
void mouse_init(void) {
    mouse_available = 0;
    mouse_wheel_supported = 0;
    mouse_buffer_head = mouse_buffer_tail = 0;

    ps2_flush_output();

    if (!ps2_write_command(0xA8)) {
        terminal_write_line("[mouse] Failed to enable PS/2 mouse port");
        return;
    }

    uint8_t config = 0;
    if (!ps2_write_command(0x20) || !ps2_read_data(&config)) {
        terminal_write_line("[mouse] Failed to read PS/2 controller config");
        return;
    }
    config |= 0x02; /* Enable mouse IRQ */
    config |= 0x01; /* Ensure keyboard IRQ stays enabled */
    config &= ~(1 << 4); /* Ensure keyboard clock enabled */
    config &= ~(1 << 5); /* Ensure mouse clock enabled */
    if (!ps2_write_command(0x60) || !ps2_write_data(config)) {
        terminal_write_line("[mouse] Failed to write PS/2 controller config");
        return;
    }

    if (!ps2_write_command(0xAE)) {
        terminal_write_line("[mouse] Failed to enable PS/2 keyboard port");
        return;
    }

    if (!mouse_send(MOUSE_CMD_SET_DEFAULTS)) {
        terminal_write_line("[mouse] Failed to apply mouse defaults");
        return;
    }

    if (!mouse_send(MOUSE_CMD_DISABLE)) {
        terminal_write_line("[mouse] Failed to disable mouse before detection");
        return;
    }

    mouse_detect_capabilities();

    if (!mouse_send(MOUSE_CMD_SET_STREAM_MODE) || !mouse_send(MOUSE_CMD_ENABLE)) {
        terminal_write_line("[mouse] Failed to enable mouse stream mode");
        return;
    }

    mouse_available = 1;
    if (mouse_wheel_supported) {
        terminal_write_line("[mouse] Mouse driver ready (wheel detected)");
    } else {
        terminal_write_line("[mouse] Mouse driver ready");
    }
}
#else
void mouse_init(void) {
    mouse_available = 0;
    mouse_wheel_supported = 0;
    mouse_buffer_head = mouse_buffer_tail = 0;
    terminal_write_line("[mouse] Driver disabled by configuration");
}
#endif

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

size_t mouse_packet_length(void) {
#if ENABLE_MOUSE_DRIVER
    return mouse_wheel_supported ? 4 : 3;
#else
    (void)mouse_wheel_supported;
    return 3;
#endif
}


