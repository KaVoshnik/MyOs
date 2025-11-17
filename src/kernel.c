#include <terminal.h>
#include <interrupts.h>
#include <pit.h>
#include <keyboard.h>
#include <memory.h>
#include <shell.h>

extern uint8_t _kernel_end;

void kernel_main(void) {
    terminal_initialize();
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREEN, TERMINAL_COLOR_BLACK);
    terminal_write_line("Welcome to MyOs!");
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);
    terminal_write_line("[kernel] Setting up interrupts...");

    uintptr_t heap_start = ((uintptr_t)&_kernel_end + 0xFFF) & ~((uintptr_t)0xFFF);
    memory_init(heap_start, 0x100000); /* 1 MiB heap */
    terminal_write_line("[kernel] Heap initialized.");

    interrupts_disable();
    interrupts_init();
    pit_init(100);
    keyboard_init();
    interrupts_enable();

    terminal_write_line("[kernel] Initialization complete.");
    shell_run();
}

