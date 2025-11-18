#include <shell.h>
#include <terminal.h>
#include <keyboard.h>
#include <pit.h>
#include <string.h>
#include <memory.h>
#include <filesystem.h>
#include <system.h>

#define SHELL_BUFFER_SIZE 256
#define SHELL_HISTORY_SIZE 50

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

static void shell_build_prompt_path(char *buffer, size_t buffer_size) {
    if (buffer_size == 0) {
        return;
    }
    char path[FS_MAX_PATH_LEN];
    fs_get_cwd(path, sizeof(path));
    if (path[0] == '/' && path[1] == '\0') {
        buffer[0] = '~';
        buffer[1] = '\0';
        return;
    }
    size_t pos = 0;
    buffer[pos++] = '~';
    const char *src = (path[0] == '/') ? path + 1 : path;
    while (*src && pos < buffer_size - 1) {
        buffer[pos++] = *src++;
    }
    buffer[pos] = '\0';
}

static const char *shell_skip_spaces(const char *str) {
    while (str && *str == ' ') {
        ++str;
    }
    return str;
}

static const char *shell_match_command(const char *line, const char *command) {
    size_t len = strlen(command);
    if (strncmp(line, command, len) != 0) {
        return NULL;
    }
    if (line[len] == '\0') {
        return line + len;
    }
    if (line[len] == ' ') {
        return shell_skip_spaces(line + len + 1);
    }
    return NULL;
}

static const char *shell_extract_token(const char *input, char *buffer, size_t buffer_size) {
    input = shell_skip_spaces(input);
    if (!input || *input == '\0') {
        buffer[0] = '\0';
        return input;
    }

    size_t i = 0;
    while (*input != '\0' && *input != ' ') {
        if (i < buffer_size - 1) {
            buffer[i++] = *input;
        }
        ++input;
    }
    buffer[i] = '\0';
    return shell_skip_spaces(input);
}

static void shell_print_fs_error(fs_status_t status) {
    switch (status) {
        case FS_ERR_NOENT:
            terminal_write_line("Filesystem error: path not found.");
            break;
        case FS_ERR_EXIST:
            terminal_write_line("Filesystem error: already exists.");
            break;
        case FS_ERR_NOTDIR:
            terminal_write_line("Filesystem error: not a directory.");
            break;
        case FS_ERR_ISDIR:
            terminal_write_line("Filesystem error: path is a directory.");
            break;
        case FS_ERR_NOMEM:
            terminal_write_line("Filesystem error: out of memory.");
            break;
        case FS_ERR_INVALID:
            terminal_write_line("Filesystem error: invalid path.");
            break;
        case FS_ERR_NOTEMPTY:
            terminal_write_line("Filesystem error: directory not empty.");
            break;
        default:
            terminal_write_line("Filesystem error: unknown.");
            break;
    }
}

static void shell_print_prompt(void) {
    char prompt_path[FS_MAX_PATH_LEN];
    shell_build_prompt_path(prompt_path, sizeof(prompt_path));
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREEN, TERMINAL_COLOR_BLACK);
    terminal_write("myos ");
    terminal_set_color(TERMINAL_COLOR_LIGHT_CYAN, TERMINAL_COLOR_BLACK);
    terminal_write(prompt_path);
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREEN, TERMINAL_COLOR_BLACK);
    terminal_write("> ");
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
    terminal_write_line("  pwd        - show current directory");
    terminal_write_line("  ls [PATH]  - list directory contents");
    terminal_write_line("  cd PATH    - change directory");
    terminal_write_line("  touch PATH - create/truncate a file");
    terminal_write_line("  cat PATH   - print file contents");
    terminal_write_line("  write PATH DATA  - overwrite file with DATA");
    terminal_write_line("  append PATH DATA - append DATA to file");
    terminal_write_line("  mkdir PATH - create directory");
    terminal_write_line("  rm [-r] PATH - remove file or directory");
    terminal_write_line("  savefs     - persist filesystem to disk");
    terminal_write_line("  loadfs     - reload filesystem from disk");
    terminal_write_line("  poweroff   - shut down the system");
    terminal_write_line("  reboot     - restart the system");
    terminal_write_line("");
    terminal_write_line("Shell features:");
    terminal_write_line("  Up/Down    - navigate command history");
    terminal_write_line("  Left/Right - move cursor in line");
    terminal_write_line("  Tab        - autocomplete commands");
    terminal_write_line("  Ctrl+R     - search history");
}

static void shell_cmd_clear(void) {
    terminal_clear();
}

static void shell_cmd_uptime(void) {
    uint64_t seconds = pit_seconds();
    struct {
        uint64_t unit_seconds;
        const char *singular;
        const char *plural;
    } units[] = {
        { 24ULL * 60ULL * 60ULL, "day", "days" },
        { 60ULL * 60ULL, "hour", "hours" },
        { 60ULL, "min", "mins" },
        { 1ULL, "sec", "secs" }
    };

    terminal_write("Uptime: ");
    int printed = 0;
    for (size_t i = 0; i < sizeof(units) / sizeof(units[0]); ++i) {
        if (seconds >= units[i].unit_seconds) {
            uint64_t value = seconds / units[i].unit_seconds;
            seconds %= units[i].unit_seconds;
            if (printed) {
                terminal_write(", ");
            }
            print_uint64(value);
            terminal_write(" ");
            terminal_write(value == 1 ? units[i].singular : units[i].plural);
            printed = 1;
        }
    }
    if (!printed) {
        terminal_write("0 secs");
    }
    terminal_write_line("");
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

static void shell_cmd_pwd(void) {
    char path[FS_MAX_PATH_LEN];
    fs_get_cwd(path, sizeof(path));
    terminal_write_line(path);
}

static void shell_ls_callback(const fs_dir_entry_t *entry, void *user_data) {
    (void)user_data;
    if (entry->is_directory) {
        terminal_write("[DIR] ");
    } else {
        terminal_write("      ");
    }
    terminal_write(entry->name);
    if (!entry->is_directory) {
        terminal_write("  ");
        print_uint64(entry->size);
        terminal_write(" bytes");
    }
    terminal_write_line("");
}

static void shell_cmd_ls(const char *args) {
    const char *path = shell_skip_spaces(args);
    if (path && *path == '\0') {
        path = NULL;
    }
    fs_status_t status = fs_list_dir(path, shell_ls_callback, NULL);
    if (status == FS_OK) {
        return;
    }
    if (status == FS_ERR_NOENT) {
        terminal_write_line("ls: path not found.");
    } else if (status == FS_ERR_NOTDIR) {
        terminal_write_line("ls: not a directory.");
    } else {
        shell_print_fs_error(status);
    }
}

static void shell_cmd_cd(const char *args) {
    const char *path = shell_skip_spaces(args);
    if (!path || *path == '\0') {
        path = "/";
    }
    fs_status_t status = fs_change_dir(path);
    if (status != FS_OK) {
        shell_print_fs_error(status);
    }
}

static void shell_cmd_touch(const char *args) {
    const char *path = shell_skip_spaces(args);
    if (!path || *path == '\0') {
        terminal_write_line("Usage: touch PATH");
        return;
    }

    if (fs_is_dir(path)) {
        terminal_write_line("touch: cannot operate on a directory.");
        return;
    }

    fs_status_t status = fs_create_file(path);
    if (status == FS_ERR_EXIST) {
        status = fs_write_file(path, NULL, 0);
    }
    if (status != FS_OK) {
        shell_print_fs_error(status);
    }
}

static void shell_cmd_mkdir(const char *args) {
    const char *path = shell_skip_spaces(args);
    if (!path || *path == '\0') {
        terminal_write_line("Usage: mkdir PATH");
        return;
    }

    fs_status_t status = fs_mkdir(path);
    if (status != FS_OK) {
        shell_print_fs_error(status);
    }
}

static void shell_cmd_rm(const char *args) {
    char token[FS_MAX_PATH_LEN];
    const char *rest = shell_extract_token(args, token, sizeof(token));
    int recursive = 0;

    if (strcmp(token, "-r") == 0 || strcmp(token, "--recursive") == 0) {
        recursive = 1;
        rest = shell_extract_token(rest, token, sizeof(token));
    }

    if (token[0] == '\0') {
        terminal_write_line("Usage: rm [-r] PATH");
        return;
    }

    fs_status_t status = fs_remove(token, recursive);
    if (status != FS_OK) {
        shell_print_fs_error(status);
    }
}

static void shell_cmd_savefs(void) {
    if (!fs_persistence_available()) {
        terminal_write_line("Persistence unavailable: attach an ATA disk.");
        return;
    }
    fs_status_t status = fs_save();
    if (status == FS_OK) {
        terminal_write_line("Filesystem snapshot saved to disk.");
    } else {
        shell_print_fs_error(status);
    }
}

static void shell_cmd_loadfs(void) {
    if (!fs_persistence_available()) {
        terminal_write_line("Persistence unavailable: attach an ATA disk.");
        return;
    }
    fs_status_t status = fs_load();
    if (status == FS_OK) {
        terminal_write_line("Filesystem reloaded from disk.");
    } else {
        shell_print_fs_error(status);
    }
}

static void shell_cmd_poweroff(void) {
    if (fs_persistence_available()) {
        terminal_write_line("Tip: run 'savefs' to persist changes before shutdown.");
    }
    terminal_write_line("Powering off...");
    system_poweroff();
}

static void shell_cmd_reboot(void) {
    terminal_write_line("Rebooting...");
    system_reboot();
}

static void shell_cmd_cat(const char *args) {
    const char *path = shell_skip_spaces(args);
    if (!path || *path == '\0') {
        terminal_write_line("Usage: cat PATH");
        return;
    }

    if (!fs_exists(path)) {
        terminal_write_line("cat: file not found.");
        return;
    }

    if (fs_is_dir(path)) {
        terminal_write_line("cat: path is a directory.");
        return;
    }

    size_t size = 0;
    const uint8_t *data = fs_get_file_data(path, &size);
    if (!data && size > 0) {
        terminal_write_line("cat: unable to read file.");
        return;
    }

    for (size_t i = 0; i < size; ++i) {
        terminal_putc((char)data[i]);
    }
    terminal_write_line("");
}

static void shell_cmd_writefile(const char *args, int append) {
    const char *cmd_name = append ? "append" : "write";
    char path[FS_MAX_PATH_LEN];
    const char *data = shell_extract_token(args, path, sizeof(path));
    if (path[0] == '\0') {
        terminal_write("Usage: ");
        terminal_write(cmd_name);
        terminal_write_line(" PATH DATA");
        return;
    }

    if (fs_is_dir(path)) {
        terminal_write(cmd_name);
        terminal_write_line(": path is a directory.");
        return;
    }

    if (!data) {
        data = "";
    }
    size_t len = strlen(data);

    fs_status_t status;
    if (append) {
        status = fs_append_file(path, data, len);
        if (status == FS_ERR_NOENT) {
            fs_status_t create_status = fs_create_file(path);
            if (create_status == FS_OK) {
                status = fs_append_file(path, data, len);
            } else {
                status = create_status;
            }
        }
    } else {
        if (!fs_exists(path)) {
            fs_status_t create_status = fs_create_file(path);
            if (create_status != FS_OK && create_status != FS_ERR_EXIST) {
                shell_print_fs_error(create_status);
                return;
            }
        }
        status = fs_write_file(path, data, len);
    }

    if (status != FS_OK) {
        shell_print_fs_error(status);
    }
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

    const char *args;

    if ((args = shell_match_command(line, "pwd")) != NULL) {
        shell_cmd_pwd();
        return;
    }

    if ((args = shell_match_command(line, "ls")) != NULL) {
        shell_cmd_ls(args);
        return;
    }

    if ((args = shell_match_command(line, "cd")) != NULL) {
        shell_cmd_cd(args);
        return;
    }

    if ((args = shell_match_command(line, "touch")) != NULL) {
        shell_cmd_touch(args);
        return;
    }

    if ((args = shell_match_command(line, "cat")) != NULL) {
        shell_cmd_cat(args);
        return;
    }

    if ((args = shell_match_command(line, "write")) != NULL) {
        shell_cmd_writefile(args, 0);
        return;
    }

    if ((args = shell_match_command(line, "append")) != NULL) {
        shell_cmd_writefile(args, 1);
        return;
    }

    if ((args = shell_match_command(line, "mkdir")) != NULL) {
        shell_cmd_mkdir(args);
        return;
    }

    if ((args = shell_match_command(line, "rm")) != NULL) {
        shell_cmd_rm(args);
        return;
    }

    if ((args = shell_match_command(line, "savefs")) != NULL) {
        (void)args;
        shell_cmd_savefs();
        return;
    }

    if ((args = shell_match_command(line, "loadfs")) != NULL) {
        (void)args;
        shell_cmd_loadfs();
        return;
    }

    if ((args = shell_match_command(line, "poweroff")) != NULL) {
        (void)args;
        shell_cmd_poweroff();
        return;
    }

    if ((args = shell_match_command(line, "reboot")) != NULL) {
        (void)args;
        shell_cmd_reboot();
        return;
    }

    terminal_write("Unknown command: ");
    terminal_write_line(line);
    terminal_write_line("Type 'help' for the list of commands.");
}

static const char *shell_commands[] = {
    "help", "clear", "uptime", "mem", "testmem", "echo", "pwd", "ls", "cd",
    "touch", "cat", "write", "append", "mkdir", "rm", "savefs", "loadfs",
    "poweroff", "reboot", NULL
};

static void shell_clear_line(size_t length) {
    for (size_t i = 0; i < length; ++i) {
        terminal_putc('\b');
        terminal_putc(' ');
        terminal_putc('\b');
    }
}

static void shell_redraw_line(const char *line, size_t length) {
    (void)length;
    terminal_write(line);
}

static int shell_autocomplete(const char *prefix, char *result, size_t result_size) {
    if (!prefix || *prefix == '\0') {
        return 0;
    }
    
    size_t prefix_len = strlen(prefix);
    const char *match = NULL;
    int match_count = 0;
    
    for (size_t i = 0; shell_commands[i] != NULL; ++i) {
        if (strncmp(shell_commands[i], prefix, prefix_len) == 0) {
            if (match_count == 0) {
                match = shell_commands[i];
            }
            match_count++;
        }
    }
    
    if (match_count == 1 && match) {
        size_t match_len = strlen(match);
        if (match_len < result_size) {
            memcpy(result, match, match_len);
            result[match_len] = '\0';
            return (int)(match_len - prefix_len);
        }
    }
    
    if (match_count > 1) {
        terminal_write_line("");
        for (size_t i = 0; shell_commands[i] != NULL; ++i) {
            if (strncmp(shell_commands[i], prefix, prefix_len) == 0) {
                terminal_write("  ");
                terminal_write_line(shell_commands[i]);
            }
        }
        return -1;
    }
    
    return 0;
}

static size_t shell_read_line_with_history(char *buffer, size_t buffer_size,
                                           char **history, size_t *history_count,
                                           size_t *history_index) {
    if (buffer_size == 0) {
        return 0;
    }
    
    size_t length = 0;
    size_t cursor_pos = 0;
    size_t current_history = *history_index;
    int in_search = 0;
    char search_buffer[SHELL_BUFFER_SIZE] = {0};
    size_t search_len = 0;
    
    buffer[0] = '\0';
    
    while (1) {
        uint16_t code;
        keyboard_read_char_extended(&code);
        
        if (code < 256) {
            char c = (char)code;
            
            if (in_search) {
                if (c == '\b') {
                    if (search_len > 0) {
                        search_len--;
                        search_buffer[search_len] = '\0';
                        terminal_putc('\b');
                        terminal_putc(' ');
                        terminal_putc('\b');
                    }
                    continue;
                }
                if (c == '\n' || c == '\r') {
                    in_search = 0;
                    terminal_write_line("");
                    if (search_len > 0) {
                        for (size_t i = *history_count; i > 0; --i) {
                            if (strstr(history[i - 1], search_buffer) != NULL) {
                                current_history = i - 1;
                                memcpy(buffer, history[i - 1], buffer_size);
                                length = strlen(buffer);
                                cursor_pos = length;
                                shell_print_prompt();
                                terminal_write(buffer);
                                break;
                            }
                        }
                    }
                    continue;
                }
                if (search_len + 1 < sizeof(search_buffer)) {
                    search_buffer[search_len++] = c;
                    terminal_putc(c);
                }
                continue;
            }
            
            if (c == '\r') {
                c = '\n';
            }
            
            if (c == '\b') {
                if (cursor_pos > 0) {
                    cursor_pos--;
                    length--;
                    for (size_t i = cursor_pos; i < length; ++i) {
                        buffer[i] = buffer[i + 1];
                    }
                    buffer[length] = '\0';
                    shell_clear_line(length + 1);
                    shell_redraw_line(buffer, length);
                    for (size_t i = cursor_pos; i < length; ++i) {
                        terminal_putc('\b');
                    }
                }
                continue;
            }
            
            if (c == '\n') {
                terminal_putc('\n');
                if (length > 0 && (*history_count == 0 || strcmp(buffer, history[*history_count - 1]) != 0)) {
                    if (*history_count < SHELL_HISTORY_SIZE) {
                        history[*history_count] = (char *)kmalloc(length + 1);
                        if (history[*history_count]) {
                            memcpy(history[*history_count], buffer, length + 1);
                            (*history_count)++;
                        }
                    }
                }
                *history_index = *history_count;
                buffer[length] = '\0';
                return length;
            }
            
            if (c == '\t') {
                char prefix[SHELL_BUFFER_SIZE] = {0};
                size_t word_start = cursor_pos;
                while (word_start > 0 && buffer[word_start - 1] != ' ') {
                    word_start--;
                }
                size_t word_len = cursor_pos - word_start;
                if (word_len > 0 && word_len < sizeof(prefix)) {
                    memcpy(prefix, buffer + word_start, word_len);
                    prefix[word_len] = '\0';
                    
                    char result[SHELL_BUFFER_SIZE];
                    int added = shell_autocomplete(prefix, result, sizeof(result));
                    if (added > 0) {
                        size_t result_len = strlen(result);
                        size_t to_add = result_len - word_len;
                        if (length + to_add + 1 < buffer_size) {
                            for (size_t i = length; i > cursor_pos; --i) {
                                buffer[i + to_add] = buffer[i];
                            }
                            memcpy(buffer + word_start, result, result_len);
                            length += to_add;
                            cursor_pos = word_start + result_len;
                            shell_clear_line(length - to_add);
                            shell_redraw_line(buffer, length);
                        }
                    } else if (added < 0) {
                        shell_print_prompt();
                        shell_redraw_line(buffer, length);
                    }
                }
                continue;
            }
            
            if (length + 1 < buffer_size) {
                for (size_t i = length; i > cursor_pos; --i) {
                    buffer[i] = buffer[i - 1];
                }
                buffer[cursor_pos] = c;
                cursor_pos++;
                length++;
                buffer[length] = '\0';
                shell_clear_line(length - cursor_pos);
                terminal_write(buffer + cursor_pos - 1);
                for (size_t i = cursor_pos; i < length; ++i) {
                    terminal_putc('\b');
                }
            }
        } else if (code == KEY_SPECIAL_UP) {
            if (current_history > 0) {
                current_history--;
                if (history[current_history]) {
                    size_t hist_len = strlen(history[current_history]);
                    if (hist_len < buffer_size) {
                        memcpy(buffer, history[current_history], hist_len + 1);
                        length = hist_len;
                        cursor_pos = length;
                        shell_clear_line(length);
                        shell_redraw_line(buffer, length);
                    }
                }
            }
        } else if (code == KEY_SPECIAL_DOWN) {
            if (current_history < *history_count) {
                current_history++;
                if (current_history < *history_count && history[current_history]) {
                    size_t hist_len = strlen(history[current_history]);
                    if (hist_len < buffer_size) {
                        memcpy(buffer, history[current_history], hist_len + 1);
                        length = hist_len;
                        cursor_pos = length;
                    } else {
                        length = 0;
                        cursor_pos = 0;
                        buffer[0] = '\0';
                    }
                } else {
                    length = 0;
                    cursor_pos = 0;
                    buffer[0] = '\0';
                }
                shell_clear_line(length);
                shell_redraw_line(buffer, length);
            }
        } else if (code == KEY_SPECIAL_LEFT) {
            if (cursor_pos > 0) {
                cursor_pos--;
                terminal_putc('\b');
            }
        } else if (code == KEY_SPECIAL_RIGHT) {
            if (cursor_pos < length) {
                terminal_putc(buffer[cursor_pos]);
                cursor_pos++;
            }
        } else if (code == KEY_SPECIAL_CTRL_R) {
            in_search = 1;
            search_len = 0;
            search_buffer[0] = '\0';
            terminal_write_line("");
            terminal_write("(reverse-i-search)`': ");
        }
    }
}

void shell_run(void) {
    static char buffer[SHELL_BUFFER_SIZE];
    static char *history[SHELL_HISTORY_SIZE];
    static size_t history_count = 0;
    static size_t history_index = 0;

    terminal_write_line("");
    terminal_write_line("Simple shell ready. Type 'help' to begin.");
    terminal_write_line("Tip: Use arrow keys for history, Tab for completion, Ctrl+R for search.");

    while (1) {
        shell_print_prompt();
        shell_read_line_with_history(buffer, SHELL_BUFFER_SIZE, history, &history_count, &history_index);
        if (buffer[0] != '\0') {
            shell_execute(buffer);
        }
    }
}


