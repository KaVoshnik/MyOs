#include <pit.h>
#include <io.h>
#include <terminal.h>

#define PIT_BASE_FREQUENCY 1193182
#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL0_PORT 0x40

static uint32_t pit_frequency = 0;
static volatile uint64_t pit_tick_count = 0;

static void print_decimal(uint32_t value) {
    char digits[10];
    int len = 0;
    if (value == 0) {
        digits[len++] = '0';
    } else {
        while (value > 0 && len < (int)sizeof(digits)) {
            digits[len++] = (char)('0' + (value % 10));
            value /= 10;
        }
    }
    for (int i = len - 1; i >= 0; --i) {
        terminal_putc(digits[i]);
    }
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
    print_decimal(frequency_hz);
    terminal_write_line(" Hz");
}

void pit_handle_tick(void) {
    ++pit_tick_count;
    if (pit_frequency != 0 && (pit_tick_count % pit_frequency) == 0) {
        terminal_write_line("[pit] 1 second elapsed");
    }
}

uint64_t pit_ticks(void) {
    return pit_tick_count;
}

