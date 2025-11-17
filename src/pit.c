#include <pit.h>
#include <io.h>
#include <terminal.h>

#define PIT_BASE_FREQUENCY 1193182
#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL0_PORT 0x40

static uint32_t pit_frequency = 0;
static volatile uint64_t pit_tick_count = 0;

static void print_uint(uint32_t value) {
    char buffer[11];
    int i = 10;
    buffer[i] = '\0';
    if (value == 0) {
        buffer[--i] = '0';
    } else {
        while (value > 0 && i > 0) {
            buffer[--i] = (char)('0' + (value % 10));
            value /= 10;
        }
    }
    terminal_write(&buffer[i]);
}

void pit_init(uint32_t frequency_hz) {
    if (frequency_hz == 0) {
        frequency_hz = 100;
    }
    pit_frequency = frequency_hz;

    uint16_t divisor = (uint16_t)(PIT_BASE_FREQUENCY / frequency_hz);

    outb(PIT_COMMAND_PORT, 0x36); /* channel 0, lobyte/hibyte, mode 3 */
    outb(PIT_CHANNEL0_PORT, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0_PORT, (uint8_t)((divisor >> 8) & 0xFF));

    terminal_write("[pit] Configured to ");
    print_uint(frequency_hz);
    terminal_write_line(" Hz");
}

void pit_handle_tick(void) {
    ++pit_tick_count;
}

uint64_t pit_ticks(void) {
    return pit_tick_count;
}

uint32_t pit_current_frequency(void) {
    return pit_frequency;
}

uint64_t pit_seconds(void) {
    if (pit_frequency == 0) {
        return 0;
    }
    return pit_tick_count / pit_frequency;
}

