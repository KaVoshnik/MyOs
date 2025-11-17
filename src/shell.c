#include <shell.h>
#include <terminal.h>
#include <keyboard.h>
#include <pit.h>
#include <string.h>
#include <memory.h>

#define SHELL_BUFFER_SIZE 256

static void print_uint64(uint64_t value) {
    char buffer[21];
    int i = 20;
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

static void shell_print_prompt(void) {
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREEN, TERMINAL_COLOR_BLACK);
    terminal_write("myos> ");
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);
}

static void shell_cmd_help(void) {
    terminal_write_line("Commands:");
    terminal_write_line("  help       - show this list");
    terminal_write_line("  clear      - clear the screen");
    terminal_write_line("  uptime     - show time since boot");
    terminal_write_line("  mem        - show heap usage");
    terminal_write_line("  testmem    - test memory allocator");
    terminal_write_line("  echo TEXT  - print TEXT");
}

static void shell_cmd_clear(void) {
    terminal_clear();
}

static void shell_cmd_uptime(void) {
    uint64_t seconds = pit_seconds();
    terminal_write("Uptime: ");
    print_uint64(seconds);
    terminal_write_line(" s");
}

static void shell_cmd_mem(void) {
    size_t used = memory_bytes_used();
    size_t total = memory_heap_size();
    size_t free = (total > used) ? (total - used) : 0;
    
    terminal_write("Heap total: ");
    print_uint64(total);
    terminal_write_line(" bytes");
    terminal_write("Heap used:  ");
    print_uint64(used);
    terminal_write_line(" bytes");
    terminal_write("Heap free:  ");
    print_uint64(free);
    terminal_write_line(" bytes");
}

static void shell_cmd_echo(const char *args) {
    if (args == NULL || *args == '\0') {
        terminal_write_line("");
        return;
    }
    terminal_write_line(args);
}

static void shell_cmd_testmem(void) {
    terminal_write_line("Testing memory allocator...");
    
    size_t initial_used = memory_bytes_used();
    terminal_write("Initial memory used: ");
    print_uint64(initial_used);
    terminal_write_line(" bytes");
    
    /* Test 1: Simple allocation */
    void *ptr1 = kmalloc(100);
    if (ptr1 == NULL) {
        terminal_write_line("ERROR: kmalloc(100) failed!");
        return;
    }
    terminal_write_line("Test 1: Allocated 100 bytes - OK");
    
    size_t after_alloc = memory_bytes_used();
    terminal_write("Memory used after alloc: ");
    print_uint64(after_alloc);
    terminal_write_line(" bytes");
    
    /* Test 2: Multiple allocations */
    void *ptr2 = kmalloc(200);
    void *ptr3 = kmalloc(50);
    if (ptr2 == NULL || ptr3 == NULL) {
        terminal_write_line("ERROR: Multiple allocations failed!");
        kfree(ptr1);
        if (ptr2) kfree(ptr2);
        return;
    }
    terminal_write_line("Test 2: Multiple allocations - OK");
    
    /* Test 3: Free memory */
    kfree(ptr2);
    terminal_write_line("Test 3: Free memory - OK");
    
    size_t after_free = memory_bytes_used();
    terminal_write("Memory used after free: ");
    print_uint64(after_free);
    terminal_write_line(" bytes");
    
    /* Test 4: Aligned allocation */
    void *ptr4 = kmalloc_aligned(64, 16);
    if (ptr4 == NULL) {
        terminal_write_line("ERROR: Aligned allocation failed!");
        kfree(ptr1);
        kfree(ptr3);
        return;
    }
    if (((uintptr_t)ptr4 & 0xF) != 0) {
        terminal_write_line("ERROR: Alignment incorrect!");
        kfree(ptr1);
        kfree(ptr3);
        kfree(ptr4);
        return;
    }
    terminal_write_line("Test 4: Aligned allocation (16 bytes) - OK");
    
    /* Cleanup */
    kfree(ptr1);
    kfree(ptr3);
    kfree(ptr4);
    
    size_t final_used = memory_bytes_used();
    terminal_write("Final memory used: ");
    print_uint64(final_used);
    terminal_write_line(" bytes");
    
    if (final_used == initial_used) {
        terminal_write_line("All tests passed! Memory properly freed.");
    } else {
        terminal_write("WARNING: Memory leak detected! Expected ");
        print_uint64(initial_used);
        terminal_write(", got ");
        print_uint64(final_used);
        terminal_write_line(" bytes");
    }
}

static void shell_execute(const char *line) {
    if (line[0] == '\0') {
        return;
    }

    if (strcmp(line, "help") == 0) {
        shell_cmd_help();
        return;
    }

    if (strcmp(line, "clear") == 0) {
        shell_cmd_clear();
        return;
    }

    if (strcmp(line, "uptime") == 0) {
        shell_cmd_uptime();
        return;
    }

    if (strcmp(line, "mem") == 0) {
        shell_cmd_mem();
        return;
    }

    if (strncmp(line, "echo ", 5) == 0) {
        shell_cmd_echo(line + 5);
        return;
    }

    if (strcmp(line, "testmem") == 0) {
        shell_cmd_testmem();
        return;
    }

    terminal_write("Unknown command: ");
    terminal_write_line(line);
    terminal_write_line("Type 'help' for the list of commands.");
}

void shell_run(void) {
    static char buffer[SHELL_BUFFER_SIZE];

    terminal_write_line("");
    terminal_write_line("Simple shell ready. Type 'help' to begin.");

    while (1) {
        shell_print_prompt();
        keyboard_read_line(buffer, SHELL_BUFFER_SIZE);
        shell_execute(buffer);
    }
}


