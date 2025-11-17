#include <terminal.h>
#include <interrupts.h>
#include <pit.h>
#include <keyboard.h>

void kernel_main(void) {
    terminal_initialize();
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREEN, TERMINAL_COLOR_BLACK);
    terminal_write_line("Welcome to MyOs!");
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);
    terminal_write_line("[kernel] Setting up interrupts...");

    interrupts_disable();
    interrupts_init();
    pit_init(100);
    keyboard_init();
    interrupts_enable();

    terminal_write_line("[kernel] Initialization complete.");

    for (;;) {
        __asm__ volatile("hlt");
    }
}

