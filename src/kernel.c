#include <terminal.h>

void kernel_main(void) {
    terminal_initialize();
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREEN, TERMINAL_COLOR_BLACK);
    terminal_write_line("Welcome to MyOs!");
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);
    terminal_write_line("");
    terminal_write_line("[kernel] Initialization complete.");

    for (;;) {
        __asm__ volatile("hlt");
    }
}

