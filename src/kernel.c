#include <terminal.h>
#include <interrupts.h>
#include <pit.h>
#include <keyboard.h>
#include <mouse.h>
#include <memory.h>
#include <shell.h>
#include <filesystem.h>
#include <ata.h>
#include <io.h>
#include <thread.h>
#include <process.h>
#include <user.h>
#include <login.h>

extern uint8_t _kernel_end;

void kernel_main(void) {
    terminal_initialize();
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREEN, TERMINAL_COLOR_BLACK);
    terminal_write_line("Welcome to MyOs!");
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);
    terminal_write_line("[kernel] Setting up interrupts...");

    uintptr_t heap_start = ((uintptr_t)&_kernel_end + 0xFFF) & ~((uintptr_t)0xFFF);
    memory_init(heap_start, 0x400000); /* 4 MiB heap (increased for graphics framebuffer) */
    terminal_write_line("[kernel] Heap initialized.");

    interrupts_disable();
    interrupts_init();
    pit_init(100);
    keyboard_init();
    mouse_init();
    thread_system_init();
    process_system_init();
    
    interrupts_enable();

    ata_init();
    if (ata_is_available()) {
        terminal_write_line("[kernel] ATA initialized.");
    } else {
        terminal_write_line("[kernel] ATA device not found.");
    }

    fs_init();
    terminal_write_line("[kernel] Filesystem ready.");

    user_system_init();
    terminal_write_line("[kernel] User system ready.");

    terminal_write_line("[kernel] Initialization complete.");
    
    /* Check if first boot */
    if (config_is_first_boot()) {
        if (first_boot_setup() != 0) {
            terminal_write_line("[kernel] Setup failed, halting.");
            for (;;) {
                __asm__ volatile("cli; hlt");
            }
        }
    } else {
        /* Normal boot - show login */
        system_config_t config;
        config_load(&config);
        
        if (config.auto_login && config.default_user[0] != '\0') {
            /* Auto login */
            if (user_set_current(config.default_user) == 0) {
                terminal_set_color(TERMINAL_COLOR_LIGHT_GREEN, TERMINAL_COLOR_BLACK);
                terminal_write("Auto-login as ");
                terminal_write(config.default_user);
                terminal_write_line(".");
                terminal_set_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);
            } else {
                if (login_screen() != 0) {
                    terminal_write_line("[kernel] Login failed, halting.");
                    for (;;) {
                        __asm__ volatile("cli; hlt");
                    }
                }
            }
        } else {
            /* Require login */
            if (login_screen() != 0) {
                terminal_write_line("[kernel] Login failed, halting.");
                for (;;) {
                    __asm__ volatile("cli; hlt");
                }
            }
        }
    }
    
    shell_run();
}

