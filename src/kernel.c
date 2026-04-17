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
#include <pci.h>
#include <serial.h>
#include <rtl8139.h>
#include <net.h>

extern uint8_t _kernel_end;

/* Multiboot information structure */
struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint8_t color_info[6];
} __attribute__((packed));

struct multiboot_info *mb_info = NULL;  /* Made global for graphics.c */

void kernel_main(uint64_t multiboot_info_ptr) {
    /* Store multiboot info pointer */
    if (multiboot_info_ptr != 0) {
        mb_info = (struct multiboot_info *)multiboot_info_ptr;
    }
    serial_init();

    /* Memory MUST be initialized before terminal_initialize() because
     * terminal_initialize() calls kmalloc() to allocate the scrollback buffer.
     * Previously the order was reversed, causing kmalloc to run with a NULL
     * heap_start and silently return NULL — scrollback never worked. */
    uintptr_t heap_start = ((uintptr_t)&_kernel_end + 0xFFF) & ~((uintptr_t)0xFFF);
    memory_init(heap_start, 0x400000); /* 4 MiB heap */

    terminal_initialize();
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREEN, TERMINAL_COLOR_BLACK);
    terminal_write_line("Welcome to MyOs!");
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);
    terminal_write_line("[kernel] Heap initialized.");
    terminal_write_line("[kernel] Setting up interrupts...");

    interrupts_disable();
    interrupts_init();
    pit_init(100);
    keyboard_init();
    mouse_init();
    thread_system_init();
    process_system_init();
    
    interrupts_enable();

    pci_scan_and_print();
    rtl8139_init();
    net_init();

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

