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
    terminal_write("Heap used: ");
    print_uint64(memory_bytes_used());
    terminal_write_line(" bytes");
}

static void shell_cmd_echo(const char *args) {
    if (args == NULL || *args == '\0') {
        terminal_write_line("");
        return;
    }
    terminal_write_line(args);
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


