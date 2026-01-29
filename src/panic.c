#include <panic.h>
#include <terminal.h>
#include <system.h>

__attribute__((noreturn)) void panic(const char *message, const char *file, int line) {
    terminal_write_line("");
    terminal_set_color(TERMINAL_COLOR_LIGHT_RED, TERMINAL_COLOR_BLACK);
    terminal_write("[panic] ");
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);

    if (message) {
        terminal_write(message);
    } else {
        terminal_write("(null)");
    }
    terminal_write_line("");

    if (file) {
        terminal_set_color(TERMINAL_COLOR_DARK_GREY, TERMINAL_COLOR_BLACK);
        terminal_write("at ");
        terminal_set_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);
        terminal_write(file);
        terminal_write(":");

        /* print line number (simple) */
        char buf[16];
        int n = line;
        int i = 0;
        if (n == 0) {
            buf[i++] = '0';
        } else {
            if (n < 0) {
                buf[i++] = '-';
                n = -n;
            }
            char tmp[16];
            int t = 0;
            while (n > 0 && t < (int)sizeof(tmp)) {
                tmp[t++] = (char)('0' + (n % 10));
                n /= 10;
            }
            while (t > 0 && i < (int)sizeof(buf) - 1) {
                buf[i++] = tmp[--t];
            }
        }
        buf[i] = '\0';
        terminal_write(buf);
        terminal_write_line("");
    }

    terminal_set_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);
    system_halt();
}

