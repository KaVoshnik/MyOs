#include <terminal.h>
#include <interrupts.h>
#include <pit.h>
#include <keyboard.h>
#include <memory.h>
#include <shell.h>
#include <filesystem.h>
#include <ata.h>
#include <mouse.h>
#include <io.h>

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

#if ENABLE_MOUSE_DRIVER
    /* Clear any pending PS/2 data before initializing mouse */
    while ((inb(0x64) & 0x01) != 0) {
        inb(0x60); /* Discard any pending data */
    }
    
    mouse_init();
    
    /* Clear any data that mouse init might have generated */
    while ((inb(0x64) & 0x01) != 0) {
        uint8_t status = inb(0x64);
        if (status & 0x20) {
            /* Mouse data - discard */
            inb(0x60);
        } else {
            /* Keyboard data - might be important, but clear to be safe */
            inb(0x60);
        }
    }
#else
    terminal_write_line("[kernel] Mouse driver disabled.");
#endif
    
    interrupts_enable();

    ata_init();
    if (ata_is_available()) {
        terminal_write_line("[kernel] ATA initialized.");
    } else {
        terminal_write_line("[kernel] ATA device not found.");
    }

    fs_init();
    terminal_write_line("[kernel] Filesystem ready.");

    terminal_write_line("[kernel] Initialization complete.");
    shell_run();
}

