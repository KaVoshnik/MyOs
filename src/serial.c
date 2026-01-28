#include <serial.h>
#include <io.h>

/* COM1 base port */
#define COM1 0x3F8

static int serial_ready = 0;

static int serial_can_transmit(void) {
    /* Line Status Register (LSR) bit 5 = THR empty */
    return (inb(COM1 + 5) & 0x20) != 0;
}

void serial_init(void) {
    /* Disable interrupts */
    outb(COM1 + 1, 0x00);
    /* Enable DLAB */
    outb(COM1 + 3, 0x80);
    /* Set baud divisor to 3 (38400 baud if base is 115200) */
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    /* 8 bits, no parity, one stop bit */
    outb(COM1 + 3, 0x03);
    /* Enable FIFO, clear them, 14-byte threshold */
    outb(COM1 + 2, 0xC7);
    /* IRQs enabled, RTS/DSR set */
    outb(COM1 + 4, 0x0B);

    /* Self-test (loopback) to avoid hard hangs if COM1 отсутствует/не отвечает */
    outb(COM1 + 4, 0x1E);          /* loopback mode */
    outb(COM1 + 0, 0xAE);          /* test byte */
    if (inb(COM1 + 0) != 0xAE) {
        serial_ready = 0;
        outb(COM1 + 4, 0x00);
        return;
    }
    outb(COM1 + 4, 0x0B);          /* normal operation */
    serial_ready = 1;
}

void serial_write_char(char c) {
    if (!serial_ready) {
        return;
    }
    /* Translate \n to \r\n for nicer host terminals */
    if (c == '\n') {
        serial_write_char('\r');
    }
    /* Bounded spin to prevent permanent lockup */
    for (uint32_t i = 0; i < 1000000u; ++i) {
        if (serial_can_transmit()) {
            outb(COM1 + 0, (uint8_t)c);
            return;
        }
    }
    /* Disable serial if it seems stuck */
    serial_ready = 0;
    return;
}

void serial_write(const char *s) {
    if (!s) {
        return;
    }
    while (*s) {
        serial_write_char(*s++);
    }
}

