#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

// PS/2 mouse commands
#define MOUSE_CMD_RESET     0xFF
#define MOUSE_CMD_ENABLE    0xF4
#define MOUSE_CMD_DISABLE   0xF5
#define MOUSE_CMD_SET_SAMPLE_RATE 0xF3
#define MOUSE_CMD_GET_DEVICE_ID   0xF2
#define MOUSE_CMD_SET_REMOTE_MODE 0xF0
#define MOUSE_CMD_SET_STREAM_MODE 0xEA
#define MOUSE_CMD_SET_WRAP_MODE   0xEE
#define MOUSE_CMD_RESET_WRAP_MODE 0xEC
#define MOUSE_CMD_READ_DATA       0xEB

/* Next byte to port 0x60 goes to mouse */
#define PS2_CMD_WRITE_TO_MOUSE    0xD4

/* Output buffer holds mouse data */
#define STATUS_OUTPUT_FROM_MOUSE  0x20

// PS/2 ports
#define PS2_DATA_PORT       0x60
#define PS2_STATUS_PORT     0x64
#define PS2_COMMAND_PORT    0x64

// Status bits
#define STATUS_OUTPUT_FULL  0x01
#define STATUS_INPUT_FULL   0x02
#define STATUS_SYSTEM       0x04
#define STATUS_COMMAND_DATA 0x08
#define STATUS_TIMEOUT      0x40
#define STATUS_PARITY_ERROR 0x80

// Mouse buttons
#define MOUSE_LEFT_BUTTON   0x01
#define MOUSE_RIGHT_BUTTON  0x02
#define MOUSE_MIDDLE_BUTTON 0x04

typedef struct {
    int32_t x;
    int32_t y;
    int32_t scroll;      // Accumulated scroll (up is positive)
    uint8_t buttons;
    uint8_t initialized;
    uint8_t packet_size; // 3 or 4 bytes
} mouse_state_t;

void mouse_init(void);
void mouse_handler(void);
mouse_state_t get_mouse_state(void);
void mouse_wait(uint8_t type);

#endif