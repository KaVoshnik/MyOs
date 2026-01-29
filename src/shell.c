#include <shell.h>
#include <terminal.h>
#include <keyboard.h>
#include <pit.h>
#include <string.h>
#include <memory.h>
#include <filesystem.h>
#include <system.h>
#include <ata.h>
#include <thread.h>
#include <process.h>
#include <user.h>
#include <mouse.h>
#include <graphics.h>
#include <rtl8139.h>
#include <net.h>

#define SHELL_BUFFER_SIZE 256
#define SHELL_HISTORY_SIZE 50
#define SHELL_AUTOCOMPLETE_MAX_MATCHES 32
#define SHELL_AUTOSAVE_INTERVAL_SECONDS 60
#define SHELL_THREAD_SNAPSHOT_MAX 32

static char *shell_history_data[SHELL_HISTORY_SIZE];
static size_t shell_history_count = 0;
static size_t shell_history_index = 0;
static uint64_t shell_last_autosave_seconds = 0;
static void shell_cmd_threads(void);
static void shell_cmd_spawn(const char *args);
static void shell_spawn_worker(void *arg);

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
    terminal_write("\x1B[1;31m");  /* Bold red */
    terminal_write("[ERROR] ");
    terminal_write("\x1B[0m");
    terminal_write("\x1B[31m");  /* Red color */
    switch (status) {
        case FS_ERR_NOENT:
            terminal_write_line("Path not found.");
            break;
        case FS_ERR_EXIST:
            terminal_write_line("Already exists.");
            break;
        case FS_ERR_NOTDIR:
            terminal_write_line("Not a directory.");
            break;
        case FS_ERR_ISDIR:
            terminal_write_line("Path is a directory.");
            break;
        case FS_ERR_NOMEM:
            terminal_write_line("Out of memory.");
            break;
        case FS_ERR_INVALID:
            terminal_write_line("Invalid path.");
            break;
        case FS_ERR_NOTEMPTY:
            terminal_write_line("Directory not empty.");
            break;
        default:
            terminal_write_line("Unknown error.");
            break;
    }
    terminal_write("\x1B[0m");  /* Reset color */
}

static void shell_print_prompt(void) {
    char prompt_path[FS_MAX_PATH_LEN];
    shell_build_prompt_path(prompt_path, sizeof(prompt_path));
    
    /* Show username if logged in */
    const char *username = user_get_current_username();
    if (username) {
        terminal_set_color(TERMINAL_COLOR_LIGHT_MAGENTA, TERMINAL_COLOR_BLACK);
        terminal_write(username);
        terminal_set_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);
        terminal_write("@");
    }
    
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREEN, TERMINAL_COLOR_BLACK);
    terminal_write("myos");
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);
    terminal_write(":");
    terminal_set_color(TERMINAL_COLOR_LIGHT_CYAN, TERMINAL_COLOR_BLACK);
    terminal_write(prompt_path);
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREEN, TERMINAL_COLOR_BLACK);
    terminal_write(" $ ");
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);
}

static void shell_cmd_help_1(void) {
    terminal_write("\x1B[1;36m");  /* Bold cyan */
    terminal_write_line("=== MyOs Shell Commands Page 1 ===");
    terminal_write("\x1B[0m");
    terminal_write_line("");
    
    terminal_write("\x1B[1;33m");  /* Bold yellow */
    terminal_write_line("System:");
    terminal_write("\x1B[0m");
    terminal_write("  ");
    terminal_write("\x1B[32mhelp 1-4\x1B[0m       - show this list");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mclear\x1B[0m      - clear the screen");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32muptime\x1B[0m     - show time since boot");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mmem\x1B[0m        - show heap usage");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mtestmem\x1B[0m    - test memory allocator");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mhistory\x1B[0m    - list recent commands");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mpoweroff\x1B[0m   - shut down the system");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mreboot\x1B[0m     - restart the system");
    terminal_write_line("");
    terminal_write_line("");
    terminal_write("\x1B[1;33m");
    terminal_write_line("System Info:");
    terminal_write("\x1B[0m");
    terminal_write("  ");
    terminal_write("\x1B[32mdiskinfo\x1B[0m   - show ATA disk information");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mmyfetch\x1B[0m    - display system information with logo");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mansi\x1B[0m       - test ANSI escape sequences");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mgui\x1B[0m        - test graphics subsystem (demo)");
    terminal_write_line("");
    terminal_write_line("");
}

static void shell_cmd_help_2(void) {
    terminal_write("\x1B[1;36m");  /* Bold cyan */
    terminal_write_line("=== MyOs Shell Commands Page 2 ===");
    terminal_write("\x1B[0m");
    terminal_write_line("");

    terminal_write("\x1B[1;33m");
    terminal_write_line("Filesystem:");
    terminal_write("\x1B[0m");
    terminal_write("  ");
    terminal_write("\x1B[32mpwd\x1B[0m        - show current directory");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mls [PATH]\x1B[0m  - list directory contents");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mcd PATH\x1B[0m    - change directory");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mtouch PATH\x1B[0m - create/truncate a file");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mcat PATH\x1B[0m   - print file contents");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mwrite PATH DATA\x1B[0m  - overwrite file with DATA");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mappend PATH DATA\x1B[0m - append DATA to file");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mmkdir PATH\x1B[0m - create directory");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mrm [-r] PATH\x1B[0m - remove file or directory");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mcp SRC DEST\x1B[0m - copy file");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mmv SRC DEST\x1B[0m - move/rename file");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32msavefs\x1B[0m     - persist filesystem to disk");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mloadfs\x1B[0m     - reload filesystem from disk");
    terminal_write_line("");
    terminal_write_line("");
}

static void shell_cmd_help_3(void) {
    terminal_write("\x1B[1;36m");  /* Bold cyan */
    terminal_write_line("=== MyOs Shell Commands Page 3 ===");
    terminal_write("\x1B[0m");
    terminal_write_line("");

    terminal_write("\x1B[1;33m");
    terminal_write_line("Text Processing:");
    terminal_write("\x1B[0m");
    terminal_write("  ");
    terminal_write("\x1B[32mecho TEXT\x1B[0m  - print TEXT");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mfind [PATH] PATTERN\x1B[0m - find files by name pattern");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mgrep PATTERN FILE\x1B[0m - search for pattern in file");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mhead [FILE] [LINES]\x1B[0m - show first lines of file");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mtail [FILE] [LINES]\x1B[0m - show last lines of file");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mwc FILE\x1B[0m - count lines, words, characters");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mhexdump FILE\x1B[0m - show file in hexadecimal");
    terminal_write_line("");
    terminal_write_line("");
    
    terminal_write("\x1B[1;33m");
    terminal_write_line("Processes & Threads:");
    terminal_write("\x1B[0m");
    terminal_write("  ");
    terminal_write("\x1B[32mthreads\x1B[0m    - list all kernel threads");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mps\x1B[0m         - show detailed process information");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mkill PID\x1B[0m   - kill a process by PID");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mspawn TEXT\x1B[0m - start background process printing TEXT");
    terminal_write_line("");
    terminal_write_line("");
}

static void shell_cmd_help_4(void) {
        terminal_write("\x1B[1;33m");
    terminal_write_line("Users & Security:");
    terminal_write("\x1B[0m");
    terminal_write("  ");
    terminal_write("\x1B[32mwhoami\x1B[0m     - show current username");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mlogout\x1B[0m     - logout from current session");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32museradd USERNAME\x1B[0m - create new user (admin only)");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[32mpasswd\x1B[0m     - change password");
    terminal_write_line("");
    terminal_write_line("");
    
    terminal_write("\x1B[1;33m");
    terminal_write_line("Shell Features:");
    terminal_write("\x1B[0m");
    terminal_write("  ");
    terminal_write("\x1B[36mUp/Down\x1B[0m    - navigate command history");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[36mLeft/Right\x1B[0m - move cursor in line");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[36mTab\x1B[0m        - autocomplete commands");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[36mCtrl+R\x1B[0m     - search history");
    terminal_write_line("");
    terminal_write("  ");
    terminal_write("\x1B[36mAutosave\x1B[0m   - snapshot every minute when disk is attached");
    terminal_write_line("");
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
        const char *icon;
    } units[] = {
        { 24ULL * 60ULL * 60ULL, "day", "days", "d" },
        { 60ULL * 60ULL, "hour", "hours", "h" },
        { 60ULL, "min", "mins", "m" },
        { 1ULL, "sec", "secs", "s" }
    };

    terminal_write("\x1B[1;36m");  /* Bold cyan */
    terminal_write("Uptime: ");
    terminal_write("\x1B[0m");
    int printed = 0;
    for (size_t i = 0; i < sizeof(units) / sizeof(units[0]); ++i) {
        if (seconds >= units[i].unit_seconds) {
            uint64_t value = seconds / units[i].unit_seconds;
            seconds %= units[i].unit_seconds;
            if (printed) {
                terminal_write("\x1B[90m");  /* Dark grey */
                terminal_write(", ");
                terminal_write("\x1B[0m");
            }
            terminal_write("\x1B[33m");  /* Yellow */
            print_uint64(value);
            terminal_write("\x1B[0m");
            terminal_write("\x1B[90m");
            terminal_write(units[i].icon);
            terminal_write("\x1B[0m");
            printed = 1;
        }
    }
    if (!printed) {
        terminal_write("\x1B[33m");
        terminal_write("0");
        terminal_write("\x1B[0m");
        terminal_write("\x1B[90m");
        terminal_write("s");
        terminal_write("\x1B[0m");
    }
    terminal_write_line("");
}

static void shell_cmd_mem(void) {
    size_t used = memory_bytes_used();
    size_t total = memory_heap_size();
    size_t free = (total > used) ? (total - used) : 0;
    size_t blocks = memory_blocks_count();
    size_t free_blocks = memory_free_blocks_count();
    size_t largest_free = memory_largest_free_block();
    
    terminal_write("\x1B[1;36m");  /* Bold cyan */
    terminal_write("Memory Statistics");
    terminal_write("\x1B[0m");
    terminal_write_line("");
    terminal_write_line("");
    
    terminal_write("\x1B[36mTotal:\x1B[0m      ");
    terminal_write("\x1B[33m");
    print_uint64(total);
    terminal_write("\x1B[0m");
    terminal_write(" bytes");
    terminal_write_line("");
    
    terminal_write("\x1B[32mUsed:\x1B[0m       ");
    terminal_write("\x1B[33m");
    print_uint64(used);
    terminal_write("\x1B[0m");
    terminal_write(" bytes");
    if (total > 0) {
        uint64_t percent = (used * 100) / total;
        terminal_write(" (");
        terminal_write("\x1B[31m");
        print_uint64(percent);
        terminal_write("\x1B[0m");
        terminal_write("%)");
    }
    terminal_write_line("");
    
    terminal_write("\x1B[32mFree:\x1B[0m       ");
    terminal_write("\x1B[33m");
    print_uint64(free);
    terminal_write("\x1B[0m");
    terminal_write(" bytes");
    if (total > 0) {
        uint64_t percent = (free * 100) / total;
        terminal_write(" (");
        terminal_write("\x1B[32m");
        print_uint64(percent);
        terminal_write("\x1B[0m");
        terminal_write("%)");
    }
    terminal_write_line("");
    
    terminal_write("\x1B[36mBlocks:\x1B[0m     ");
    terminal_write("\x1B[33m");
    print_uint64(blocks);
    terminal_write("\x1B[0m");
    terminal_write(" total, ");
    terminal_write("\x1B[32m");
    print_uint64(free_blocks);
    terminal_write("\x1B[0m");
    terminal_write(" free, ");
    terminal_write("\x1B[31m");
    print_uint64(blocks - free_blocks);
    terminal_write("\x1B[0m");
    terminal_write_line(" used");
    
    terminal_write("\x1B[36mLargest free:\x1B[0m ");
    terminal_write("\x1B[33m");
    print_uint64(largest_free);
    terminal_write("\x1B[0m");
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
        terminal_write("\x1B[1;34m");  /* Bold blue */
        terminal_write("d ");
        terminal_write("\x1B[0m");
        terminal_write("\x1B[1;36m");  /* Bold cyan */
        terminal_write(entry->name);
        terminal_write("\x1B[0m");
        terminal_write("\x1B[90m");  /* Dark grey */
        terminal_write("/");
        terminal_write("\x1B[0m");
    } else {
        terminal_write("\x1B[32m");  /* Green */
        terminal_write("- ");
        terminal_write("\x1B[0m");
        terminal_write("\x1B[32m");  /* Green for files */
        terminal_write(entry->name);
        terminal_write("\x1B[0m");
        terminal_write("\x1B[90m");  /* Dark grey */
        terminal_write("  (");
        terminal_write("\x1B[0m");
        print_uint64(entry->size);
        terminal_write("\x1B[90m");
        terminal_write(" bytes)");
        terminal_write("\x1B[0m");
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
    } else {
        terminal_write("\x1B[1;32m");  /* Bold green */
        terminal_write("[OK] ");
        terminal_write("\x1B[0m");
        terminal_write("\x1B[32m");
        terminal_write("File created/updated.");
        terminal_write("\x1B[0m");
        terminal_write_line("");
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
    } else {
        terminal_write("\x1B[1;32m");  /* Bold green */
        terminal_write("[OK] ");
        terminal_write("\x1B[0m");
        terminal_write("\x1B[32m");
        terminal_write("Directory created.");
        terminal_write("\x1B[0m");
        terminal_write_line("");
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
    } else {
        terminal_write("\x1B[1;32m");  /* Bold green */
        terminal_write("[OK] ");
        terminal_write("\x1B[0m");
        terminal_write("\x1B[32m");
        terminal_write("Removed successfully.");
        terminal_write("\x1B[0m");
        terminal_write_line("");
    }
}

static void shell_cmd_savefs(void) {
    if (!fs_persistence_available()) {
        terminal_write("\x1B[1;33m");  /* Bold yellow warning */
        terminal_write("[WARNING] ");
        terminal_write("\x1B[0m");
        terminal_write("\x1B[33m");
        terminal_write_line("Persistence unavailable: attach an ATA disk.");
        terminal_write("\x1B[0m");
        return;
    }
    fs_status_t status = fs_save();
    if (status == FS_OK) {
        terminal_write("\x1B[1;32m");  /* Bold green success */
        terminal_write("[OK] ");
        terminal_write("\x1B[0m");
        terminal_write("\x1B[32m");
        terminal_write("Filesystem snapshot saved to disk.");
        terminal_write("\x1B[0m");
        terminal_write_line("");
    } else {
        shell_print_fs_error(status);
    }
}

static void shell_cmd_loadfs(void) {
    if (!fs_persistence_available()) {
        terminal_write("\x1B[1;33m");  /* Bold yellow warning */
        terminal_write("[WARNING] ");
        terminal_write("\x1B[0m");
        terminal_write("\x1B[33m");
        terminal_write_line("Persistence unavailable: attach an ATA disk.");
        terminal_write("\x1B[0m");
        return;
    }
    fs_status_t status = fs_load();
    if (status == FS_OK) {
        terminal_write("\x1B[1;32m");  /* Bold green success */
        terminal_write("[OK] ");
        terminal_write("\x1B[0m");
        terminal_write("\x1B[32m");
        terminal_write("Filesystem reloaded from disk.");
        terminal_write("\x1B[0m");
        terminal_write_line("");
    } else {
        shell_print_fs_error(status);
    }
}

static void shell_cmd_diskinfo(void) {
    if (!ata_is_available()) {
        terminal_write_line("ATA disk not available.");
        return;
    }
    
    const char *model = ata_get_model();
    const char *serial = ata_get_serial();
    const char *firmware = ata_get_firmware();
    uint64_t total_sectors = ata_get_total_sectors();
    uint64_t total_bytes = total_sectors * 512;
    uint64_t total_mb = total_bytes / (1024 * 1024);
    uint64_t total_gb = total_bytes / (1024 * 1024 * 1024);
    
    terminal_write("\x1B[1;36m");  /* Bold cyan */
    terminal_write_line("ATA Disk Information:");
    terminal_write("\x1B[0m");
    terminal_write("  Model:    \x1B[33m");  /* Yellow */
    terminal_write_line(model && model[0] ? model : "(unknown)");
    terminal_write("\x1B[0m  Serial:   \x1B[33m");
    terminal_write_line(serial && serial[0] ? serial : "(unknown)");
    terminal_write("\x1B[0m  Firmware: \x1B[33m");
    terminal_write_line(firmware && firmware[0] ? firmware : "(unknown)");
    terminal_write("\x1B[0m  Capacity: \x1B[32m");  /* Green */
    print_uint64(total_sectors);
    terminal_write("\x1B[0m sectors (");
    if (total_gb > 0) {
        print_uint64(total_gb);
        terminal_write(" GB / ");
    }
    print_uint64(total_mb);
    terminal_write_line(" MB)");
}

static void shell_cmd_cp(const char *args) {
    char src[FS_MAX_PATH_LEN];
    char dest[FS_MAX_PATH_LEN];
    const char *rest = shell_extract_token(args, src, sizeof(src));
    rest = shell_extract_token(rest, dest, sizeof(dest));
    
    if (src[0] == '\0' || dest[0] == '\0') {
        terminal_write_line("Usage: cp SRC DEST");
        return;
    }
    
    if (!fs_exists(src)) {
        terminal_write_line("cp: source file not found.");
        return;
    }
    
    if (fs_is_dir(src)) {
        terminal_write_line("cp: cannot copy directory (use -r for recursive copy).");
        return;
    }
    
    size_t size = 0;
    const uint8_t *data = fs_get_file_data(src, &size);
    if (data == NULL && size > 0) {
        terminal_write_line("cp: unable to read source file.");
        return;
    }
    
    fs_status_t status = fs_write_file(dest, data, size);
    if (status != FS_OK) {
        if (status == FS_ERR_NOENT) {
            fs_status_t create_status = fs_create_file(dest);
            if (create_status == FS_OK) {
                status = fs_write_file(dest, data, size);
            } else {
                status = create_status;
            }
        }
        if (status != FS_OK) {
            shell_print_fs_error(status);
        } else {
            terminal_write("\x1B[1;32m");  /* Bold green */
            terminal_write("[OK] ");
            terminal_write("\x1B[0m");
            terminal_write("\x1B[32m");
            terminal_write("File copied successfully.");
            terminal_write("\x1B[0m");
            terminal_write_line("");
        }
    } else {
        terminal_write("\x1B[1;32m");  /* Bold green */
        terminal_write("[OK] ");
        terminal_write("\x1B[0m");
        terminal_write("\x1B[32m");
        terminal_write("File copied successfully.");
        terminal_write("\x1B[0m");
        terminal_write_line("");
    }
}

static void shell_cmd_mv(const char *args) {
    char src[FS_MAX_PATH_LEN];
    char dest[FS_MAX_PATH_LEN];
    const char *rest = shell_extract_token(args, src, sizeof(src));
    rest = shell_extract_token(rest, dest, sizeof(dest));
    
    if (src[0] == '\0' || dest[0] == '\0') {
        terminal_write_line("Usage: mv SRC DEST");
        return;
    }
    
    if (!fs_exists(src)) {
        terminal_write_line("mv: source not found.");
        return;
    }
    
    if (strcmp(src, dest) == 0) {
        return; /* Same file, nothing to do */
    }
    
    /* Copy first */
    size_t size = 0;
    const uint8_t *data = fs_get_file_data(src, &size);
    if (data == NULL && size > 0) {
        terminal_write_line("mv: unable to read source.");
        return;
    }
    
    fs_status_t status = fs_write_file(dest, data, size);
    if (status != FS_OK) {
        if (status == FS_ERR_NOENT) {
            fs_status_t create_status = fs_create_file(dest);
            if (create_status == FS_OK) {
                status = fs_write_file(dest, data, size);
            } else {
                status = create_status;
            }
        }
        if (status != FS_OK) {
            shell_print_fs_error(status);
            return;
        }
    }
    
    /* Then remove source */
    status = fs_remove(src, 0);
    if (status != FS_OK) {
        shell_print_fs_error(status);
    } else {
        terminal_write("\x1B[1;32m");  /* Bold green */
        terminal_write("[OK] ");
        terminal_write("\x1B[0m");
        terminal_write("\x1B[32m");
        terminal_write("File moved successfully.");
        terminal_write("\x1B[0m");
        terminal_write_line("");
    }
}

typedef struct {
    const char *pattern;
    const char *base_path;
    int *found_count;
} shell_find_data_t;

static void shell_find_callback(const fs_dir_entry_t *entry, void *user_data) {
    shell_find_data_t *data = (shell_find_data_t *)user_data;
    char full_path[FS_MAX_PATH_LEN];
    size_t base_len = strlen(data->base_path);
    size_t name_len = strlen(entry->name);
    
    if (base_len >= sizeof(full_path) || name_len >= sizeof(full_path)) {
        terminal_write_line("find: path too long, skipping entry.");
        return;
    }
    
    size_t pos = 0;
    if (base_len == 0) {
        full_path[pos++] = '/';
    } else {
        memcpy(full_path, data->base_path, base_len);
        pos = base_len;
    }
    
    if (!(pos == 1 && full_path[0] == '/')) {
        if (pos == 0 || full_path[pos - 1] != '/') {
            if (pos + 1 >= sizeof(full_path)) {
                terminal_write_line("find: path too long, skipping entry.");
                return;
            }
            full_path[pos++] = '/';
        }
    }
    
    if (pos + name_len >= sizeof(full_path)) {
        terminal_write_line("find: path too long, skipping entry.");
        return;
    }
    
    memcpy(full_path + pos, entry->name, name_len);
    pos += name_len;
    full_path[pos] = '\0';
    
    if (strstr(entry->name, data->pattern) != NULL) {
        if (entry->is_directory) {
            terminal_write("\x1B[1;36m");  /* Bold cyan */
            terminal_write(full_path);
            terminal_write("\x1B[90m");
            terminal_write("/");
            terminal_write("\x1B[0m");
        } else {
            terminal_write("\x1B[32m");  /* Green */
            terminal_write(full_path);
            terminal_write("\x1B[0m");
        }
        terminal_write_line("");
        (*data->found_count)++;
    }
    
    if (entry->is_directory && strcmp(entry->name, ".") != 0 && strcmp(entry->name, "..") != 0) {
        shell_find_data_t sub_data = {
            .pattern = data->pattern,
            .base_path = full_path,
            .found_count = data->found_count
        };
        fs_list_dir(full_path, shell_find_callback, &sub_data);
    }
}

static void shell_cmd_find(const char *args) {
    char first[FS_MAX_PATH_LEN];
    char second[FS_MAX_NAME_LEN];
    const char *rest = shell_extract_token(args, first, sizeof(first));
    rest = shell_extract_token(rest, second, sizeof(second));
    
    if (first[0] == '\0') {
        terminal_write_line("Usage: find [PATH] PATTERN");
        return;
    }
    
    const char *path = NULL;
    const char *pattern = NULL;
    if (second[0] == '\0') {
        path = ".";
        pattern = first;
    } else {
        path = first;
        pattern = second;
    }
    
    if (pattern[0] == '\0') {
        terminal_write_line("Usage: find [PATH] PATTERN");
        return;
    }
    
    int found_count = 0;
    char search_path[FS_MAX_PATH_LEN];
    
    if (strcmp(path, ".") == 0) {
        fs_get_cwd(search_path, sizeof(search_path));
    } else {
        size_t path_len = strlen(path);
        if (path_len >= sizeof(search_path)) {
            terminal_write_line("find: path too long.");
            return;
        }
        memcpy(search_path, path, path_len + 1);
    }
    
    shell_find_data_t find_data = {
        .pattern = pattern,
        .base_path = search_path,
        .found_count = &found_count
    };
    
    fs_status_t status = fs_list_dir(search_path, shell_find_callback, &find_data);
    if (status != FS_OK) {
        shell_print_fs_error(status);
        return;
    }
    
    if (found_count == 0) {
        terminal_write("\x1B[33m");  /* Yellow */
        terminal_write_line("No matches found.");
        terminal_write("\x1B[0m");
    } else {
        terminal_write("\x1B[90m");  /* Dark grey */
        terminal_write("Found ");
        terminal_write("\x1B[33m");
        print_uint64(found_count);
        terminal_write("\x1B[90m");
        terminal_write(" match");
        if (found_count != 1) {
            terminal_write("es");
        }
        terminal_write_line(".");
        terminal_write("\x1B[0m");
    }
}

static void shell_cmd_grep(const char *args) {
    char pattern[FS_MAX_PATH_LEN];
    char file_path[FS_MAX_PATH_LEN];
    const char *rest = shell_extract_token(args, pattern, sizeof(pattern));
    rest = shell_extract_token(rest, file_path, sizeof(file_path));
    
    if (pattern[0] == '\0' || file_path[0] == '\0') {
        terminal_write_line("Usage: grep PATTERN FILE");
        return;
    }
    
    if (!fs_exists(file_path)) {
        terminal_write_line("grep: file not found.");
        return;
    }
    
    if (fs_is_dir(file_path)) {
        terminal_write_line("grep: cannot search in directory.");
        return;
    }
    
    size_t size = 0;
    const uint8_t *data = fs_get_file_data(file_path, &size);
    if (data == NULL && size > 0) {
        terminal_write_line("grep: unable to read file.");
        return;
    }
    
    /* Simple line-by-line search */
    const char *text = (const char *)data;
    size_t line_start = 0;
    int found_any = 0;
    
    for (size_t i = 0; i < size; ++i) {
        if (text[i] == '\n' || i == size - 1) {
            size_t line_len = i - line_start;
            if (i == size - 1 && text[i] != '\n') {
                line_len++;
            }
            
            if (line_len > 0) {
                char line[256];
                size_t copy_len = (line_len < sizeof(line) - 1) ? line_len : sizeof(line) - 1;
                memcpy(line, text + line_start, copy_len);
                line[copy_len] = '\0';
                
                if (strstr(line, pattern) != NULL) {
                    terminal_write("\x1B[36m");  /* Cyan for filename */
                    terminal_write(file_path);
                    terminal_write("\x1B[0m: ");
                    /* Highlight the pattern in the line */
                    const char *match = strstr(line, pattern);
                    if (match) {
                        size_t before_len = match - line;
                        if (before_len < sizeof(line)) {
                            char before[256];
                            size_t copy_len = (before_len < sizeof(before) - 1) ? before_len : sizeof(before) - 1;
                            memcpy(before, line, copy_len);
                            before[copy_len] = '\0';
                            terminal_write(before);
                            terminal_write("\x1B[1;31m");  /* Bold red for match */
                            terminal_write(pattern);
                            terminal_write("\x1B[0m");
                            terminal_write(match + strlen(pattern));
                        } else {
                            terminal_write(line);
                        }
                    } else {
                        terminal_write(line);
                    }
                    terminal_write_line("");
                    found_any = 1;
                }
            }
            
            line_start = i + 1;
        }
    }
    
    if (!found_any) {
        /* No matches */
    }
}

static void shell_cmd_head(const char *args) {
    char file_path[FS_MAX_PATH_LEN];
    const char *rest = shell_extract_token(args, file_path, sizeof(file_path));
    int lines = 10;
    
    /* Try to parse number of lines */
    if (rest && *rest != '\0') {
        char num_str[32];
        shell_extract_token(rest, num_str, sizeof(num_str));
        /* Simple atoi */
        lines = 0;
        for (size_t i = 0; num_str[i] && i < sizeof(num_str) - 1; ++i) {
            if (num_str[i] >= '0' && num_str[i] <= '9') {
                lines = lines * 10 + (num_str[i] - '0');
            } else {
                break;
            }
        }
        if (lines == 0) {
            lines = 10;
        }
    }
    
    if (file_path[0] == '\0') {
        terminal_write_line("Usage: head [FILE] [LINES]");
        return;
    }
    
    if (!fs_exists(file_path)) {
        terminal_write_line("head: file not found.");
        return;
    }
    
    if (fs_is_dir(file_path)) {
        terminal_write_line("head: cannot read directory.");
        return;
    }
    
    size_t size = 0;
    const uint8_t *data = fs_get_file_data(file_path, &size);
    if (data == NULL && size > 0) {
        terminal_write_line("head: unable to read file.");
        return;
    }
    
    const char *text = (const char *)data;
    int line_count = 0;
    for (size_t i = 0; i < size && line_count < lines; ++i) {
        terminal_putc(text[i]);
        if (text[i] == '\n') {
            line_count++;
        }
    }
}

static void shell_cmd_tail(const char *args) {
    char file_path[FS_MAX_PATH_LEN];
    const char *rest = shell_extract_token(args, file_path, sizeof(file_path));
    int lines = 10;
    
    if (rest && *rest != '\0') {
        char num_str[32];
        shell_extract_token(rest, num_str, sizeof(num_str));
        lines = 0;
        for (size_t i = 0; num_str[i] && i < sizeof(num_str) - 1; ++i) {
            if (num_str[i] >= '0' && num_str[i] <= '9') {
                lines = lines * 10 + (num_str[i] - '0');
            } else {
                break;
            }
        }
        if (lines == 0) {
            lines = 10;
        }
    }
    
    if (file_path[0] == '\0') {
        terminal_write_line("Usage: tail [FILE] [LINES]");
        return;
    }
    
    if (!fs_exists(file_path)) {
        terminal_write_line("tail: file not found.");
        return;
    }
    
    if (fs_is_dir(file_path)) {
        terminal_write_line("tail: cannot read directory.");
        return;
    }
    
    size_t size = 0;
    const uint8_t *data = fs_get_file_data(file_path, &size);
    if (data == NULL && size > 0) {
        terminal_write_line("tail: unable to read file.");
        return;
    }
    
    const char *text = (const char *)data;
    int line_count = 0;
    size_t start_pos = size;
    
    /* Count lines from end */
    for (size_t i = size; i > 0; --i) {
        if (text[i - 1] == '\n' || i == 1) {
            line_count++;
            if (line_count > lines) {
                start_pos = i;
                break;
            }
            if (i == 1 && text[0] != '\n') {
                start_pos = 0;
                break;
            }
        }
    }
    
    for (size_t i = start_pos; i < size; ++i) {
        terminal_putc(text[i]);
    }
}

static void shell_cmd_wc(const char *args) {
    const char *path = shell_skip_spaces(args);
    if (!path || *path == '\0') {
        terminal_write_line("Usage: wc FILE");
        return;
    }
    
    if (!fs_exists(path)) {
        terminal_write_line("wc: file not found.");
        return;
    }
    
    if (fs_is_dir(path)) {
        terminal_write_line("wc: cannot count directory.");
        return;
    }
    
    size_t size = 0;
    const uint8_t *data = fs_get_file_data(path, &size);
    if (data == NULL && size > 0) {
        terminal_write_line("wc: unable to read file.");
        return;
    }
    
    const char *text = (const char *)data;
    size_t lines = 0;
    size_t words = 0;
    size_t chars = size;
    int in_word = 0;
    
    for (size_t i = 0; i < size; ++i) {
        if (text[i] == '\n') {
            lines++;
        }
        
        int is_space = (text[i] == ' ' || text[i] == '\t' || text[i] == '\n' || text[i] == '\r');
        if (is_space) {
            if (in_word) {
                words++;
                in_word = 0;
            }
        } else {
            in_word = 1;
        }
    }
    
    if (in_word) {
        words++;
    }
    
    terminal_write("\x1B[33m");  /* Yellow */
    print_uint64(lines);
    terminal_write("\x1B[0m");
    terminal_write("  ");
    terminal_write("\x1B[33m");
    print_uint64(words);
    terminal_write("\x1B[0m");
    terminal_write("  ");
    terminal_write("\x1B[33m");
    print_uint64(chars);
    terminal_write("\x1B[0m");
    terminal_write("  ");
    terminal_write("\x1B[36m");  /* Cyan */
    terminal_write_line(path);
    terminal_write("\x1B[0m");
}

static void shell_cmd_ansi_test(void) {
    terminal_write_line("\x1B[1mANSI Escape Sequences Test\x1B[0m");
    terminal_write_line("");
    
    terminal_write_line("\x1B[31mRed text\x1B[0m");
    terminal_write_line("\x1B[32mGreen text\x1B[0m");
    terminal_write_line("\x1B[33mYellow text\x1B[0m");
    terminal_write_line("\x1B[34mBlue text\x1B[0m");
    terminal_write_line("\x1B[35mMagenta text\x1B[0m");
    terminal_write_line("\x1B[36mCyan text\x1B[0m");
    terminal_write_line("\x1B[37mWhite text\x1B[0m");
    terminal_write_line("");
    
    terminal_write_line("\x1B[1;31mBold red\x1B[0m");
    terminal_write_line("\x1B[1;32mBold green\x1B[0m");
    terminal_write_line("\x1B[1;33mBold yellow\x1B[0m");
    terminal_write_line("");
    
    terminal_write_line("\x1B[90mBright black\x1B[0m");
    terminal_write_line("\x1B[91mBright red\x1B[0m");
    terminal_write_line("\x1B[92mBright green\x1B[0m");
    terminal_write_line("\x1B[93mBright yellow\x1B[0m");
    terminal_write_line("\x1B[94mBright blue\x1B[0m");
    terminal_write_line("\x1B[95mBright magenta\x1B[0m");
    terminal_write_line("\x1B[96mBright cyan\x1B[0m");
    terminal_write_line("\x1B[97mBright white\x1B[0m");
    terminal_write_line("");
    
    terminal_write_line("\x1B[41mRed background\x1B[0m");
    terminal_write_line("\x1B[42mGreen background\x1B[0m");
    terminal_write_line("\x1B[43mYellow background\x1B[0m");
    terminal_write_line("\x1B[44mBlue background\x1B[0m");
    terminal_write_line("");
    
    terminal_write_line("\x1B[7mInverted colors\x1B[0m");
    terminal_write_line("");
    
    terminal_write("Cursor movement test: ");
    terminal_write("\x1B[5C");  /* Move right 5 positions */
    terminal_write_line("<- moved here");
    
    terminal_write_line("\x1B[2K");  /* Clear line */
    terminal_write_line("Line cleared above");
}

/* Forward declaration of multiboot info structure */
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

extern struct multiboot_info *mb_info;

static void shell_cmd_gui(void) {
    terminal_write_line("[GUI] Initializing graphics subsystem...");
    
    /* Show memory status */
    size_t used = memory_bytes_used();
    size_t total = memory_heap_size();
    size_t free = (total > used) ? (total - used) : 0;
    terminal_write("[GUI] Memory: ");
    print_uint64(free / 1024);
    terminal_write(" KiB free / ");
    print_uint64(total / 1024);
    terminal_write_line(" KiB total");
    
    /* Try to initialize graphics with 800x600x32 */
    /* It will automatically fallback to smaller resolution if needed */
    int result = graphics_init(800, 600, 32);
    if (result != 0) {
        terminal_write("\x1B[1;31m[ERROR]\x1B[0m ");
        if (result == -1) {
            terminal_write_line("Failed to allocate framebuffer (out of memory).");
        } else if (result == -2) {
            terminal_write_line("Not enough memory for framebuffer.");
        } else {
            terminal_write_line("Failed to initialize graphics.");
        }
        terminal_write("[GUI] Required: ~");
        print_uint64((800 * 600 * 4) / 1024);
        terminal_write(" KiB for 800x600x32");
        terminal_write_line("");
        terminal_write("[GUI] Available: ");
        print_uint64(free / 1024);
        terminal_write_line(" KiB");
        terminal_write_line("[GUI] Note: Increase heap size in kernel.c to support larger resolutions.");
        terminal_write_line("      Current heap: 4 MiB");
        return;
    }
    
    terminal_write_line("[GUI] Graphics initialized successfully!");
    terminal_write_line("[GUI] Running demo...");
    
    /* Run demo */
    graphics_demo();
    
    terminal_write_line("[GUI] Demo complete!");
    
    /* Check Multiboot framebuffer status */
    if (mb_info) {
        terminal_write("[GUI] Multiboot info available, flags: 0x");
        char hex[] = "0123456789ABCDEF";
        uint32_t flags = mb_info->flags;
        for (int i = 28; i >= 0; i -= 4) {
            terminal_putc(hex[(flags >> i) & 0xF]);
        }
        terminal_write_line("");
        
        if (mb_info->flags & (1 << 12)) {
            terminal_write_line("[GUI] Multiboot framebuffer info available!");
            terminal_write("[GUI] Framebuffer address: 0x");
            uint64_t fb_addr = mb_info->framebuffer_addr;
            for (int i = 60; i >= 0; i -= 4) {
                terminal_putc(hex[(fb_addr >> i) & 0xF]);
            }
            terminal_write_line("");
            terminal_write("[GUI] Resolution: ");
            print_uint64(mb_info->framebuffer_width);
            terminal_write("x");
            print_uint64(mb_info->framebuffer_height);
            terminal_write(" @ ");
            print_uint64(mb_info->framebuffer_bpp);
            terminal_write_line(" bpp");
        } else {
            terminal_write_line("[GUI] Multiboot framebuffer info NOT available.");
            terminal_write_line("[GUI] GRUB may not be configured for framebuffer.");
        }
    } else {
        terminal_write_line("[GUI] Multiboot info not available.");
    }
    
    terminal_write_line("[GUI] Attempting to switch to graphics mode...");
    
    /* Try to switch to graphics mode and show framebuffer */
    /* This is safe - won't cause page faults if framebuffer is not available */
    graphics_show();
    
    if (graphics_is_mode_active()) {
        terminal_write_line("[GUI] Graphics mode activated!");
        terminal_write_line("[GUI] Framebuffer copied to video memory.");
        terminal_write_line("[GUI] Note: If graphics not visible, screen may still be in text mode.");
        terminal_write_line("[GUI]       Try: QEMU with -vga std or -vga vmware");
    } else {
        terminal_write_line("[GUI] Note: Framebuffer is in memory only.");
        terminal_write_line("[GUI]       Multiboot framebuffer not available.");
        terminal_write_line("[GUI]       To enable graphics display:");
        terminal_write_line("[GUI]       1. Ensure GRUB is configured with framebuffer");
        terminal_write_line("[GUI]       2. Or use QEMU with -vga std option");
        terminal_write_line("[GUI]       Graphics context is ready for use.");
    }
    
    graphics_context_t *ctx = graphics_get_context();
    if (ctx) {
        terminal_write("[GUI] Framebuffer: ");
        terminal_write("0x");
        /* Print framebuffer address */
        uintptr_t addr = (uintptr_t)ctx->framebuffer;
        char hex[] = "0123456789ABCDEF";
        for (int i = 60; i >= 0; i -= 4) {
            terminal_putc(hex[(addr >> i) & 0xF]);
        }
        terminal_write_line("");
        terminal_write("[GUI] Resolution: ");
        print_uint64(ctx->width);
        terminal_write("x");
        print_uint64(ctx->height);
        terminal_write(" @ ");
        print_uint64(ctx->bpp);
        terminal_write_line(" bpp");
    }
}

static void shell_cmd_myfetch(void) {
    terminal_write("\x1B[1;36m");  /* Bold cyan color */
    terminal_write_line("                   ____      ");
    terminal_write_line("                  / __ \\     ");
    terminal_write_line("  _ __ ___  _   _| |  | |___ ");
    terminal_write_line(" | '_ ` _ \\| | | | |  | / __|");
    terminal_write_line(" | | | | | | |_| | |__| \\__ \\");
    terminal_write_line(" |_| |_| |_|\\__, |\\____/|___/");
    terminal_write_line("             __/ |           ");
    terminal_write_line("            |___/            ");
    terminal_write("\x1B[0m");  /* Reset color */
    terminal_write_line("");

        terminal_write("\x1B[1;36m");  /* Bold cyan */
    terminal_write("OS:        ");
    terminal_write("\x1B[0m");
    terminal_write("\x1B[32m");  /* Green */
    terminal_write_line("MyOs");
    
    terminal_write("\x1B[1;36m");
    terminal_write("Version:   ");
    terminal_write("\x1B[0m");
    terminal_write("\x1B[33m");  /* Yellow */
    terminal_write_line("1.1.0");
    
    /* Uptime */
    terminal_write("\x1B[1;36m");
    terminal_write("Uptime:    ");
    terminal_write("\x1B[0m");
    terminal_write("\x1B[35m");  /* Magenta */
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
    
    /* Memory */
    terminal_write("\x1B[1;36m");
    terminal_write("Memory:    ");
    terminal_write("\x1B[0m");
    terminal_write("\x1B[34m");  /* Blue */
    size_t used = memory_bytes_used();
    size_t total = memory_heap_size();
    print_uint64(used / 1024);
    terminal_write(" KiB / ");
    print_uint64(total / 1024);
    terminal_write(" KiB (");
    if (total > 0) {
        uint64_t percent = (used * 100) / total;
        print_uint64(percent);
    } else {
        terminal_write("0");
    }
    terminal_write_line("%)");
    
    /* Threads */
    terminal_write("\x1B[1;36m");
    terminal_write("Threads:   ");
    terminal_write("\x1B[0m");
    terminal_write("\x1B[31m");  /* Red */
    thread_snapshot_t snapshots[SHELL_THREAD_SNAPSHOT_MAX];
    size_t thread_count = thread_snapshot_list(snapshots, SHELL_THREAD_SNAPSHOT_MAX);
    print_uint64(thread_count);
    terminal_write_line(" active");
    
    /* Disk */
    terminal_write("\x1B[1;36m");
    terminal_write("Disk:      ");
    terminal_write("\x1B[0m");
    if (ata_is_available()) {
        terminal_write("\x1B[32m");  /* Green */
        uint64_t total_sectors = ata_get_total_sectors();
        uint64_t total_mb = (total_sectors * 512) / (1024 * 1024);
        print_uint64(total_mb);
        terminal_write_line(" MB available");
    } else {
        terminal_write("\x1B[33m");  /* Yellow */
        terminal_write_line("Not available");
    }
    
    /* Filesystem */
    terminal_write("\x1B[1;36m");
    terminal_write("Filesystem:");
    terminal_write("\x1B[0m");
    terminal_write("\x1B[36m");  /* Cyan */
    if (fs_persistence_available()) {
        terminal_write_line("Persistent (ATA disk attached)");
    } else {
        terminal_write_line("In-memory only");
    }
    
    /* Current directory */
    terminal_write("\x1B[1;36m");
    terminal_write("Directory: ");
    terminal_write("\x1B[0m");
    terminal_write("\x1B[37m");  /* White */
    char path[FS_MAX_PATH_LEN];
    fs_get_cwd(path, sizeof(path));
    terminal_write_line(path);
    
    terminal_write("\x1B[0m");  /* Reset color */
}

static void shell_cmd_nicinfo(void) {
    const rtl8139_info_t *info = rtl8139_get_info();
    if (!info || !info->present) {
        terminal_write_line("nicinfo: RTL8139 not detected.");
        return;
    }

    terminal_write_line("RTL8139 network interface:");

    char buf[4];

    terminal_write("  PCI bus=");
    buf[0] = '0' + (info->bus / 10);
    buf[1] = '0' + (info->bus % 10);
    buf[2] = '\0';
    terminal_write(buf);

    terminal_write(" dev=");
    buf[0] = '0' + (info->device / 10);
    buf[1] = '0' + (info->device % 10);
    buf[2] = '\0';
    terminal_write(buf);

    terminal_write(" fn=");
    buf[0] = '0' + info->function;
    buf[1] = '\0';
    terminal_write_line(buf);

    static const char hex[] = "0123456789ABCDEF";
    char hbuf[5];

    terminal_write("  IO base=0x");
    hbuf[0] = hex[(info->io_base >> 12) & 0xF];
    hbuf[1] = hex[(info->io_base >> 8) & 0xF];
    hbuf[2] = hex[(info->io_base >> 4) & 0xF];
    hbuf[3] = hex[info->io_base & 0xF];
    hbuf[4] = '\0';
    terminal_write(hbuf);

    terminal_write(" irq=");
    buf[0] = '0' + (info->irq_line / 10);
    buf[1] = '0' + (info->irq_line % 10);
    buf[2] = '\0';
    terminal_write_line(buf);
}

static void shell_cmd_nicregs(void) {
    rtl8139_regs_t r;
    if (!rtl8139_get_regs(&r)) {
        terminal_write_line("nicregs: RTL8139 not detected.");
        return;
    }
    static const char hex[] = "0123456789ABCDEF";
    char h16[5];
    char h32[9];

    terminal_write_line("RTL8139 regs:");

    terminal_write("  CR=0x");
    h16[0] = hex[(r.cr >> 4) & 0xF];
    h16[1] = hex[r.cr & 0xF];
    h16[2] = '\0';
    terminal_write_line(h16);

    terminal_write("  CAPR=0x");
    h16[0] = hex[(r.capr >> 12) & 0xF];
    h16[1] = hex[(r.capr >> 8) & 0xF];
    h16[2] = hex[(r.capr >> 4) & 0xF];
    h16[3] = hex[r.capr & 0xF];
    h16[4] = '\0';
    terminal_write_line(h16);

    terminal_write("  CBR=0x");
    h16[0] = hex[(r.cbr >> 12) & 0xF];
    h16[1] = hex[(r.cbr >> 8) & 0xF];
    h16[2] = hex[(r.cbr >> 4) & 0xF];
    h16[3] = hex[r.cbr & 0xF];
    h16[4] = '\0';
    terminal_write_line(h16);

    terminal_write("  ISR=0x");
    h16[0] = hex[(r.isr >> 12) & 0xF];
    h16[1] = hex[(r.isr >> 8) & 0xF];
    h16[2] = hex[(r.isr >> 4) & 0xF];
    h16[3] = hex[r.isr & 0xF];
    h16[4] = '\0';
    terminal_write_line(h16);

    terminal_write("  IMR=0x");
    h16[0] = hex[(r.imr >> 12) & 0xF];
    h16[1] = hex[(r.imr >> 8) & 0xF];
    h16[2] = hex[(r.imr >> 4) & 0xF];
    h16[3] = hex[r.imr & 0xF];
    h16[4] = '\0';
    terminal_write_line(h16);

    terminal_write("  RCR=0x");
    uint32_t v = r.rcr;
    for (int i = 7; i >= 0; --i) {
        h32[i] = hex[v & 0xF];
        v >>= 4;
    }
    h32[8] = '\0';
    terminal_write_line(h32);
}

static void shell_cmd_netdump(void) {
    int count = rtl8139_poll_rx(16);
    terminal_write("netdump: processed ");
    print_uint64((uint64_t)count);
    terminal_write_line(" frame(s).");
}

static void shell_cmd_ping(const char *args) {
    const char *ip_str = shell_skip_spaces(args);
    if (!ip_str || *ip_str == '\0') {
        terminal_write_line("Usage: ping <ip>");
        terminal_write_line("Example: ping 10.0.2.2");
        return;
    }
    uint32_t ip_host = 0;
    if (!net_parse_ipv4(ip_str, &ip_host)) {
        terminal_write_line("ping: invalid IP (use a.b.c.d).");
        return;
    }
    
    /* Print IP in readable format */
    terminal_write("PING ");
    char ip_buf[16];
    uint32_t a = ip_host & 0xFF;
    uint32_t b = (ip_host >> 8) & 0xFF;
    uint32_t c = (ip_host >> 16) & 0xFF;
    uint32_t d = (ip_host >> 24) & 0xFF;
    int pos = 0;
    if (a >= 100) ip_buf[pos++] = (char)('0' + (a / 100));
    if (a >= 10) ip_buf[pos++] = (char)('0' + ((a / 10) % 10));
    ip_buf[pos++] = (char)('0' + (a % 10));
    ip_buf[pos++] = '.';
    if (b >= 100) ip_buf[pos++] = (char)('0' + (b / 100));
    if (b >= 10) ip_buf[pos++] = (char)('0' + ((b / 10) % 10));
    ip_buf[pos++] = (char)('0' + (b % 10));
    ip_buf[pos++] = '.';
    if (c >= 100) ip_buf[pos++] = (char)('0' + (c / 100));
    if (c >= 10) ip_buf[pos++] = (char)('0' + ((c / 10) % 10));
    ip_buf[pos++] = (char)('0' + (c % 10));
    ip_buf[pos++] = '.';
    if (d >= 100) ip_buf[pos++] = (char)('0' + (d / 100));
    if (d >= 10) ip_buf[pos++] = (char)('0' + ((d / 10) % 10));
    ip_buf[pos++] = (char)('0' + (d % 10));
    ip_buf[pos] = '\0';
    terminal_write(ip_buf);
    terminal_write_line("...");
    
    uint32_t rtt_ms = 0;
    int r = net_ping(ip_host, 2000, &rtt_ms);
    if (r == 0) {
        terminal_write("Reply from ");
        terminal_write(ip_buf);
        terminal_write(": time=");
        print_uint64(rtt_ms);
        terminal_write_line("ms");
    } else if (r == -1) {
        terminal_write_line("ping: ARP resolve failed.");
    } else {
        terminal_write_line("ping: timeout (no reply).");
    }
}

static void shell_cmd_hexdump(const char *args) {
    char file_path[FS_MAX_PATH_LEN];
    shell_extract_token(args, file_path, sizeof(file_path));
    
    if (file_path[0] == '\0') {
        terminal_write_line("Usage: hexdump FILE");
        return;
    }
    
    if (!fs_exists(file_path)) {
        terminal_write_line("hexdump: file not found.");
        return;
    }
    
    if (fs_is_dir(file_path)) {
        terminal_write_line("hexdump: cannot dump directory.");
        return;
    }
    
    size_t size = 0;
    const uint8_t *data = fs_get_file_data(file_path, &size);
    if (data == NULL && size > 0) {
        terminal_write_line("hexdump: unable to read file.");
        return;
    }
    
    static const char hex_digits[] = "0123456789ABCDEF";
    size_t offset = 0;
    size_t bytes_per_line = 16;
    
    while (offset < size) {
        /* Print offset in cyan */
        terminal_write("\x1B[36m0000");
        uint64_t off = offset;
        char offset_str[9];
        for (int i = 7; i >= 0; --i) {
            offset_str[i] = hex_digits[off & 0xF];
            off >>= 4;
        }
        offset_str[8] = '\0';
        terminal_write(offset_str);
        terminal_write("\x1B[0m  ");
        
        /* Print hex bytes */
        for (size_t i = 0; i < bytes_per_line; ++i) {
            if (offset + i < size) {
                uint8_t byte = data[offset + i];
                /* Color code: printable = green, non-printable = red */
                if (byte >= 32 && byte < 127) {
                    terminal_write("\x1B[32m");
                } else {
                    terminal_write("\x1B[31m");
                }
                terminal_putc(hex_digits[(byte >> 4) & 0xF]);
                terminal_putc(hex_digits[byte & 0xF]);
                terminal_write("\x1B[0m");
            } else {
                terminal_write("  ");
            }
            if (i == 7) {
                terminal_write(" ");
            } else {
                terminal_write(" ");
            }
        }
        
        terminal_write(" \x1B[33m|\x1B[0m");  /* Yellow separator */
        
        /* Print ASCII representation */
        for (size_t i = 0; i < bytes_per_line && offset + i < size; ++i) {
            uint8_t byte = data[offset + i];
            if (byte >= 32 && byte < 127) {
                terminal_write("\x1B[32m");  /* Green for printable */
                terminal_putc((char)byte);
                terminal_write("\x1B[0m");
            } else {
                terminal_write("\x1B[31m");  /* Red for non-printable */
                terminal_putc('.');
                terminal_write("\x1B[0m");
            }
        }
        
        terminal_write("\x1B[33m|\x1B[0m");  /* Yellow separator */
        terminal_write_line("");
        offset += bytes_per_line;
    }
}

static void shell_cmd_threads(void) {
    thread_snapshot_t snapshots[SHELL_THREAD_SNAPSHOT_MAX];
    size_t count = thread_snapshot_list(snapshots, SHELL_THREAD_SNAPSHOT_MAX);
    if (count == 0) {
        terminal_write("\x1B[33m");  /* Yellow */
        terminal_write_line("No active threads.");
        terminal_write("\x1B[0m");
        return;
    }
    terminal_write("\x1B[1;36m");  /* Bold cyan */
    terminal_write("ID");
    terminal_write("\x1B[0m");
    terminal_write("     ");
    terminal_write("\x1B[1;36m");
    terminal_write("STATE");
    terminal_write("\x1B[0m");
    terminal_write("      ");
    terminal_write("\x1B[1;36m");
    terminal_write_line("NAME");
    terminal_write("\x1B[0m");
    for (size_t i = 0; i < count; ++i) {
        terminal_write("  ");
        terminal_write("\x1B[33m");  /* Yellow */
        print_uint64(snapshots[i].id);
        terminal_write("\x1B[0m");
        terminal_write("    ");
        terminal_write("\x1B[32m");  /* Green */
        terminal_write(thread_state_name(snapshots[i].state));
        terminal_write("\x1B[0m");
        terminal_write("    ");
        terminal_write("\x1B[36m");  /* Cyan */
        terminal_write_line(snapshots[i].name ? snapshots[i].name : "(null)");
        terminal_write("\x1B[0m");
    }
}

static void shell_cmd_ps(void) {
    process_snapshot_t snapshots[64];
    size_t count = process_snapshot_list(snapshots, 64);
    if (count == 0) {
        terminal_write_line("No active processes.");
        return;
    }
    
    uint64_t current_pid = process_current_pid();
    
    /* Header with colors */
    terminal_write("\x1B[1;36m");  /* Bold cyan */
    terminal_write("  PID   PPID  STATE      NAME");
    terminal_write_line("");
    terminal_write("\x1B[0m");  /* Reset */
    
    for (size_t i = 0; i < count; ++i) {
        /* Highlight current process */
        if (snapshots[i].pid == current_pid) {
            terminal_write("\x1B[1;32m");  /* Bold green */
            terminal_write("* ");
        } else {
            terminal_write("  ");
        }
        
        /* PID */
        terminal_write("\x1B[33m");  /* Yellow */
        print_uint64(snapshots[i].pid);
        terminal_write("   ");
        
        /* PPID */
        terminal_write("\x1B[35m");  /* Magenta */
        print_uint64(snapshots[i].ppid);
        terminal_write("   ");
        
        /* State with color coding */
        const char *state_name = process_state_name(snapshots[i].state);
        switch (snapshots[i].state) {
            case PROCESS_RUNNING:
                terminal_write("\x1B[32m");  /* Green */
                break;
            case PROCESS_SLEEPING:
                terminal_write("\x1B[33m");  /* Yellow */
                break;
            case PROCESS_ZOMBIE:
                terminal_write("\x1B[31m");  /* Red */
                break;
            case PROCESS_STOPPED:
                terminal_write("\x1B[37m");  /* White */
                break;
            default:
                terminal_write("\x1B[37m");  /* White */
                break;
        }
        terminal_write(state_name);
        /* Pad state name to align */
        size_t state_len = strlen(state_name);
        for (size_t j = state_len; j < 11; ++j) {
            terminal_write(" ");
        }
        
        /* Name */
        terminal_write("\x1B[0m");  /* Reset */
        terminal_write("  ");
        terminal_write_line(snapshots[i].name ? snapshots[i].name : "(null)");
    }
    
    terminal_write("\x1B[0m");  /* Reset */
    terminal_write_line("");
    terminal_write("* = current process");
    terminal_write_line("");
}

static void shell_cmd_kill(const char *args) {
    const char *id_str = shell_skip_spaces(args);
    if (!id_str || *id_str == '\0') {
        terminal_write_line("Usage: kill <process_id>");
        terminal_write_line("Example: kill 5");
        return;
    }
    
    /* Parse process ID */
    uint64_t pid = 0;
    while (*id_str >= '0' && *id_str <= '9') {
        pid = pid * 10 + (*id_str - '0');
        ++id_str;
    }
    
    if (pid == 0) {
        terminal_write_line("kill: invalid process ID (must be > 0)");
        return;
    }
    
    if (pid == 1) {
        terminal_write_line("kill: cannot kill init process (PID 1)");
        return;
    }
    
    uint64_t current_pid = process_current_pid();
    if (pid == current_pid) {
        terminal_write_line("kill: cannot kill current process (use 'exit' or let it finish)");
        return;
    }
    
    int result = process_kill(pid);
    if (result == 0) {
        terminal_write("\x1B[32m");  /* Green */
        terminal_write("Process ");
        print_uint64(pid);
        terminal_write_line(" killed.");
        terminal_write("\x1B[0m");
    } else if (result == -1) {
        terminal_write("\x1B[31m");  /* Red */
        terminal_write("kill: process ");
        print_uint64(pid);
        terminal_write_line(" not found.");
        terminal_write("\x1B[0m");
    } else if (result == -2) {
        terminal_write_line("kill: cannot kill self.");
    } else if (result == -3) {
        terminal_write("\x1B[33m");  /* Yellow */
        terminal_write("kill: process ");
        print_uint64(pid);
        terminal_write_line(" is already dead.");
        terminal_write("\x1B[0m");
    } else {
        terminal_write("\x1B[31m");  /* Red */
        terminal_write("kill: failed to kill process ");
        print_uint64(pid);
        terminal_write_line(".");
        terminal_write("\x1B[0m");
    }
}

static void shell_spawn_worker(void *arg) {
    char *message = (char *)arg;
    uint64_t pid = process_current_pid();
    for (int i = 0; i < 5; ++i) {
        terminal_write("[process ");
        print_uint64(pid);
        terminal_write("] ");
        terminal_write_line(message ? message : "(null)");
        thread_yield();
    }
    if (message) {
        kfree(message);
    }
    process_exit(0);
}

static void shell_cmd_whoami(void) {
    const char *username = user_get_current_username();
    if (username) {
        terminal_write_line(username);
    } else {
        terminal_write_line("Not logged in.");
    }
}

static void shell_cmd_logout(void) {
    user_logout();
    terminal_write_line("Logged out. Please login again.");
    terminal_write_line("System will halt. Restart to login.");
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

static void read_password_silent(char *buffer, size_t buffer_size) {
    size_t pos = 0;
    buffer[0] = '\0';
    
    while (1) {
        uint16_t code;
        while (!keyboard_try_read_char_extended(&code)) {
            __asm__ volatile("hlt");
        }
        
        if (code < 256) {
            char c = (char)code;
            
            if (c == '\r' || c == '\n') {
                terminal_write_line("");
                buffer[pos] = '\0';
                return;
            }
            
            if (c == '\b') {
                if (pos > 0) {
                    pos--;
                    buffer[pos] = '\0';
                    terminal_write("\b \b");
                }
                continue;
            }
            
            if (pos < buffer_size - 1 && c >= 32 && c < 127) {
                buffer[pos++] = c;
                buffer[pos] = '\0';
                terminal_write("*");
            }
        }
    }
}

static void shell_cmd_useradd(const char *args) {
    if (!user_is_admin()) {
        terminal_write_line("useradd: permission denied (admin only).");
        return;
    }
    
    const char *username = shell_skip_spaces(args);
    if (!username || *username == '\0') {
        terminal_write_line("Usage: useradd <username>");
        return;
    }
    
    if (strlen(username) < 3) {
        terminal_write_line("useradd: username must be at least 3 characters.");
        return;
    }
    
    if (user_find_by_name(username) != NULL) {
        terminal_write_line("useradd: user already exists.");
        return;
    }
    
    terminal_write("Enter password for ");
    terminal_write(username);
    terminal_write(": ");
    
    char password[64];
    read_password_silent(password, sizeof(password));
    
    if (strlen(password) < 4) {
        terminal_write_line("useradd: password must be at least 4 characters.");
        return;
    }
    
    terminal_write("Confirm password: ");
    char password_confirm[64];
    read_password_silent(password_confirm, sizeof(password_confirm));
    
    if (strcmp(password, password_confirm) != 0) {
        terminal_write_line("useradd: passwords do not match.");
        return;
    }
    
    int is_admin = 0; /* Regular user by default */
    int result = user_create(username, password, is_admin);
    
    if (result == 0) {
        terminal_write("\x1B[32m");  /* Green */
        terminal_write("User '");
        terminal_write(username);
        terminal_write_line("' created successfully.");
        terminal_write("\x1B[0m");
    } else if (result == -2) {
        terminal_write_line("useradd: user already exists.");
    } else {
        terminal_write_line("useradd: failed to create user.");
    }
}

static void shell_cmd_passwd(const char *args) {
    const char *target_username = shell_skip_spaces(args);
    const char *current_username = user_get_current_username();
    
    if (!current_username) {
        terminal_write_line("passwd: not logged in.");
        return;
    }
    
    /* If no username specified, change own password */
    if (!target_username || *target_username == '\0') {
        target_username = current_username;
    }
    
    /* Check permissions */
    if (strcmp(target_username, current_username) != 0 && !user_is_admin()) {
        terminal_write_line("passwd: permission denied (can only change own password).");
        return;
    }
    
    user_t *target_user = user_find_by_name(target_username);
    if (!target_user) {
        terminal_write_line("passwd: user not found.");
        return;
    }
    
    terminal_write("Enter current password: ");
    char current_password[64];
    read_password_silent(current_password, sizeof(current_password));
    
    if (!password_verify(current_password, target_user->password_hash)) {
        terminal_write_line("passwd: incorrect password.");
        return;
    }
    
    terminal_write("Enter new password: ");
    char new_password[64];
    read_password_silent(new_password, sizeof(new_password));
    
    if (strlen(new_password) < 4) {
        terminal_write_line("passwd: password must be at least 4 characters.");
        return;
    }
    
    terminal_write("Confirm new password: ");
    char new_password_confirm[64];
    read_password_silent(new_password_confirm, sizeof(new_password_confirm));
    
    if (strcmp(new_password, new_password_confirm) != 0) {
        terminal_write_line("passwd: passwords do not match.");
        return;
    }
    
    /* Update password */
    password_hash(new_password, target_user->password_hash, sizeof(target_user->password_hash));
    
    /* Save users */
    user_system_save_users();
    
    terminal_write("\x1B[32m");  /* Green */
    terminal_write("Password changed for ");
    terminal_write(target_username);
    terminal_write_line(".");
    terminal_write("\x1B[0m");
}

static void shell_cmd_spawn(const char *args) {
    const char *text = shell_skip_spaces(args);
    if (!text || *text == '\0') {
        text = "background task";
    }
    size_t len = strlen(text) + 1;
    char *data = (char *)kmalloc(len);
    if (!data) {
        terminal_write_line("spawn: out of memory.");
        return;
    }
    memcpy(data, text, len);
    uint64_t pid = process_create("shell-worker", shell_spawn_worker, data, 0);
    if (pid == 0) {
        terminal_write_line("spawn: failed to create process.");
        kfree(data);
    } else {
        terminal_write("Created process ");
        print_uint64(pid);
        terminal_write_line(".");
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
        terminal_write("\x1B[1;31mERROR\x1B[0m: kmalloc(100) failed!");
        terminal_write_line("");
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
        terminal_write("\x1B[1;31mERROR\x1B[0m: Multiple allocations failed!");
        terminal_write_line("");
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
        terminal_write("\x1B[1;31mERROR\x1B[0m: Aligned allocation failed!");
        terminal_write_line("");
        kfree(ptr1);
        kfree(ptr3);
        return;
    }
    if (((uintptr_t)ptr4 & 0xF) != 0) {
        terminal_write("\x1B[1;31mERROR\x1B[0m: Alignment incorrect!");
        terminal_write_line("");
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

static void shell_cmd_history(void) {
    if (shell_history_count == 0) {
        terminal_write("\x1B[33m");  /* Yellow */
        terminal_write_line("History is empty.");
        terminal_write("\x1B[0m");
        return;
    }
    terminal_write("\x1B[1;36m");  /* Bold cyan */
    terminal_write_line("Command History:");
    terminal_write("\x1B[0m");
    terminal_write_line("");
    for (size_t i = 0; i < shell_history_count; ++i) {
        terminal_write("\x1B[90m");  /* Dark grey */
        terminal_write("  ");
        print_uint64(i + 1);
        terminal_write(": ");
        terminal_write("\x1B[0m");
        if (shell_history_data[i]) {
            terminal_write("\x1B[36m");  /* Cyan */
            terminal_write_line(shell_history_data[i]);
            terminal_write("\x1B[0m");
        } else {
            terminal_write_line("");
        }
    }
}

static void shell_execute(const char *line) {
    if (line[0] == '\0') {
        return;
    }

    if (strcmp(line, "help 1") == 0) {
        shell_cmd_help_1();
        return;
    }
    
    if (strcmp(line, "help 2") == 0) {
        shell_cmd_help_2();
        return;
    }

    if (strcmp(line, "help 3") == 0) {
        shell_cmd_help_3();
        return;
    }
    
    if (strcmp(line, "help 4") == 0) {
        shell_cmd_help_4();
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

    if (strcmp(line, "history") == 0) {
        shell_cmd_history();
        return;
    }

    if (strcmp(line, "threads") == 0) {
        shell_cmd_threads();
        return;
    }

    if (strcmp(line, "ps") == 0) {
        shell_cmd_ps();
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

    if ((args = shell_match_command(line, "diskinfo")) != NULL) {
        (void)args;
        shell_cmd_diskinfo();
        return;
    }

    if ((args = shell_match_command(line, "nicinfo")) != NULL) {
        (void)args;
        shell_cmd_nicinfo();
        return;
    }

    if ((args = shell_match_command(line, "nicregs")) != NULL) {
        (void)args;
        shell_cmd_nicregs();
        return;
    }

    if ((args = shell_match_command(line, "netdump")) != NULL) {
        (void)args;
        shell_cmd_netdump();
        return;
    }

    if ((args = shell_match_command(line, "ping")) != NULL) {
        shell_cmd_ping(args);
        return;
    }

    if ((args = shell_match_command(line, "cp")) != NULL) {
        shell_cmd_cp(args);
        return;
    }

    if ((args = shell_match_command(line, "mv")) != NULL) {
        shell_cmd_mv(args);
        return;
    }

    if ((args = shell_match_command(line, "find")) != NULL) {
        shell_cmd_find(args);
        return;
    }

    if ((args = shell_match_command(line, "grep")) != NULL) {
        shell_cmd_grep(args);
        return;
    }

    if ((args = shell_match_command(line, "head")) != NULL) {
        shell_cmd_head(args);
        return;
    }

    if ((args = shell_match_command(line, "tail")) != NULL) {
        shell_cmd_tail(args);
        return;
    }

    if ((args = shell_match_command(line, "wc")) != NULL) {
        shell_cmd_wc(args);
        return;
    }

    if ((args = shell_match_command(line, "hexdump")) != NULL) {
        shell_cmd_hexdump(args);
        return;
    }

    if ((args = shell_match_command(line, "kill")) != NULL) {
        shell_cmd_kill(args);
        return;
    }

    if ((args = shell_match_command(line, "spawn")) != NULL) {
        shell_cmd_spawn(args);
        return;
    }

    if ((args = shell_match_command(line, "ansi")) != NULL) {
        (void)args;
        shell_cmd_ansi_test();
        return;
    }

    if ((args = shell_match_command(line, "gui")) != NULL) {
        (void)args;
        shell_cmd_gui();
        return;
    }

    if ((args = shell_match_command(line, "myfetch")) != NULL) {
        (void)args;
        shell_cmd_myfetch();
        return;
    }

    if ((args = shell_match_command(line, "whoami")) != NULL) {
        (void)args;
        shell_cmd_whoami();
        return;
    }

    if ((args = shell_match_command(line, "logout")) != NULL) {
        (void)args;
        shell_cmd_logout();
        return;
    }

    if ((args = shell_match_command(line, "useradd")) != NULL) {
        shell_cmd_useradd(args);
        return;
    }

    if ((args = shell_match_command(line, "passwd")) != NULL) {
        shell_cmd_passwd(args);
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

    terminal_write("\x1B[1;31m");  /* Bold red */
    terminal_write("[ERROR] ");
    terminal_write("\x1B[0m");
    terminal_write("\x1B[31m");
    terminal_write("Unknown command: ");
    terminal_write("\x1B[33m");
    terminal_write(line);
    terminal_write("\x1B[0m");
    terminal_write_line("");
    terminal_write("\x1B[90m");
    terminal_write("Type ");
    terminal_write("\x1B[36m");
    terminal_write("'help 1-4'");
    terminal_write("\x1B[90m");
    terminal_write_line(" for the list of commands.");
    terminal_write("\x1B[0m");
}

static const char *shell_commands[] = {
    "help", "clear", "uptime", "mem", "testmem", "history", "echo", "pwd", "ls", "cd",
    "touch", "cat", "write", "append", "mkdir", "rm", "savefs", "loadfs", "diskinfo",
    "cp", "mv", "find", "grep", "head", "tail", "wc", "hexdump", "threads", "ps", "kill",
    "spawn", "ansi", "gui", "myfetch", "nicinfo", "nicregs", "netdump", "ping", "whoami", "logout", "useradd", "passwd", "poweroff", "reboot", NULL
};

static size_t shell_collect_command_matches(const char *prefix, const char **matches, size_t max_matches) {
    size_t prefix_len = prefix ? strlen(prefix) : 0;
    size_t count = 0;
    for (size_t i = 0; shell_commands[i] != NULL; ++i) {
        if (prefix_len == 0 || strncmp(shell_commands[i], prefix, prefix_len) == 0) {
            if (count < max_matches) {
                matches[count++] = shell_commands[i];
            }
        }
    }
    return count;
}

static size_t shell_common_prefix_length(const char **matches, size_t match_count) {
    if (match_count == 0) {
        return 0;
    }
    size_t min_len = strlen(matches[0]);
    for (size_t i = 1; i < match_count; ++i) {
        size_t len = strlen(matches[i]);
        if (len < min_len) {
            min_len = len;
        }
    }
    for (size_t pos = 0; pos < min_len; ++pos) {
        char ch = matches[0][pos];
        for (size_t i = 1; i < match_count; ++i) {
            if (matches[i][pos] != ch) {
                return pos;
            }
        }
    }
    return min_len;
}

static void shell_refresh_input(const char *buffer, size_t length, size_t cursor_pos,
                                size_t prompt_row, size_t prompt_col,
                                size_t *rendered_length) {
    terminal_set_cursor(prompt_row, prompt_col);
    if (length > 0) {
        terminal_write(buffer);
    }
    size_t pad = 0;
    if (*rendered_length > length) {
        pad = *rendered_length - length;
        for (size_t i = 0; i < pad; ++i) {
            terminal_putc(' ');
        }
    }
    size_t total_visible = length + pad;
    if (cursor_pos > total_visible) {
        cursor_pos = total_visible;
    }
    if (total_visible > cursor_pos) {
        size_t move_back = total_visible - cursor_pos;
        for (size_t i = 0; i < move_back; ++i) {
            terminal_putc('\b');
        }
    }
    *rendered_length = length;
}

static void shell_history_append(char **history, size_t *history_count, size_t *history_index,
                                 const char *line, size_t length) {
    if (*history_count == SHELL_HISTORY_SIZE) {
        if (history[0]) {
            kfree(history[0]);
        }
        for (size_t i = 1; i < SHELL_HISTORY_SIZE; ++i) {
            history[i - 1] = history[i];
        }
        history[SHELL_HISTORY_SIZE - 1] = NULL;
        if (*history_index > 0) {
            (*history_index)--;
        }
        *history_count = SHELL_HISTORY_SIZE - 1;
    }

    history[*history_count] = (char *)kmalloc(length + 1);
    if (history[*history_count]) {
        memcpy(history[*history_count], line, length);
        history[*history_count][length] = '\0';
        (*history_count)++;
    }
}

static int shell_maybe_autosave(void) {
    uint64_t now = pit_seconds();
    if (shell_last_autosave_seconds == 0 || now < shell_last_autosave_seconds) {
        shell_last_autosave_seconds = now;
        return 0;
    }

    if (!fs_persistence_available()) {
        shell_last_autosave_seconds = now;
        return 0;
    }

    if ((now - shell_last_autosave_seconds) < SHELL_AUTOSAVE_INTERVAL_SECONDS) {
        return 0;
    }

    shell_last_autosave_seconds = now;
    fs_status_t status = fs_save();
    if (status == FS_OK) {
        terminal_write("\x1B[36m[autosave]\x1B[0m ");  /* Cyan for autosave */
        terminal_write("\x1B[1;32m");  /* Bold green */
        terminal_write("Filesystem snapshot saved.");
        terminal_write("\x1B[0m");
        terminal_write_line("");
    } else {
        terminal_write("\x1B[36m[autosave]\x1B[0m ");
        shell_print_fs_error(status);
    }
    return 1;
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
    size_t prompt_row = 0;
    size_t prompt_col = 0;
    terminal_get_cursor(&prompt_row, &prompt_col);
    size_t rendered_length = 0;
    
    while (1) {
        uint16_t code;
        while (!keyboard_try_read_char_extended(&code)) {
            if (shell_maybe_autosave()) {
                shell_print_prompt();
                terminal_get_cursor(&prompt_row, &prompt_col);
                rendered_length = 0;
                shell_refresh_input(buffer, length, cursor_pos, prompt_row, prompt_col, &rendered_length);
            }
            __asm__ volatile("hlt");
        }

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
                    shell_print_prompt();
                    terminal_get_cursor(&prompt_row, &prompt_col);
                    rendered_length = 0;
                    if (search_len > 0) {
                        for (size_t i = *history_count; i > 0; --i) {
                            if (history[i - 1] && strstr(history[i - 1], search_buffer) != NULL) {
                                current_history = i - 1;
                                size_t hist_len = strlen(history[i - 1]);
                                if (hist_len >= buffer_size) {
                                    hist_len = buffer_size - 1;
                                }
                                memcpy(buffer, history[i - 1], hist_len);
                                buffer[hist_len] = '\0';
                                length = hist_len;
                                cursor_pos = length;
                                break;
                            }
                        }
                    }
                    shell_refresh_input(buffer, length, cursor_pos, prompt_row, prompt_col, &rendered_length);
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
                    shell_refresh_input(buffer, length, cursor_pos, prompt_row, prompt_col, &rendered_length);
                }
                continue;
            }
            
            if (c == '\n') {
                terminal_putc('\n');
                if (length > 0 && (*history_count == 0 || strcmp(buffer, history[*history_count - 1]) != 0)) {
                    shell_history_append(history, history_count, history_index, buffer, length);
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

                    const char *matches[SHELL_AUTOCOMPLETE_MAX_MATCHES];
                    size_t match_count = shell_collect_command_matches(prefix, matches, SHELL_AUTOCOMPLETE_MAX_MATCHES);

                    if (match_count == 0) {
                        terminal_putc('\a');
                        continue;
                    }

                    size_t common_len = shell_common_prefix_length(matches, match_count);
                    if (common_len > word_len) {
                        size_t to_add = common_len - word_len;
                        if (length + to_add + 1 < buffer_size) {
                            for (size_t i = length; i > cursor_pos; --i) {
                                buffer[i + to_add] = buffer[i];
                            }
                            memcpy(buffer + word_start, matches[0], common_len);
                            length += to_add;
                            cursor_pos = word_start + common_len;
                            buffer[length] = '\0';
                            shell_refresh_input(buffer, length, cursor_pos, prompt_row, prompt_col, &rendered_length);
                        }
                        continue;
                    }

                    if (match_count == 1) {
                        size_t match_len = strlen(matches[0]);
                        if (match_len == word_len && length + 1 < buffer_size) {
                            for (size_t i = length; i > cursor_pos; --i) {
                                buffer[i + 1] = buffer[i];
                            }
                            buffer[cursor_pos] = ' ';
                            cursor_pos++;
                            length++;
                            buffer[length] = '\0';
                            shell_refresh_input(buffer, length, cursor_pos, prompt_row, prompt_col, &rendered_length);
                        }
                        continue;
                    }

                    terminal_write_line("");
                    for (size_t i = 0; i < match_count; ++i) {
                        terminal_write("  ");
                        terminal_write_line(matches[i]);
                    }
                    shell_print_prompt();
                    terminal_get_cursor(&prompt_row, &prompt_col);
                    rendered_length = 0;
                    shell_refresh_input(buffer, length, cursor_pos, prompt_row, prompt_col, &rendered_length);
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
                shell_refresh_input(buffer, length, cursor_pos, prompt_row, prompt_col, &rendered_length);
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
                        shell_refresh_input(buffer, length, cursor_pos, prompt_row, prompt_col, &rendered_length);
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
                shell_refresh_input(buffer, length, cursor_pos, prompt_row, prompt_col, &rendered_length);
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

    terminal_write_line("");
    terminal_write("\x1B[1;32m");  /* Bold green */
    terminal_write("MyOs Shell ");
    terminal_write("\x1B[0m");
    terminal_write("\x1B[32m");
    terminal_write_line("ready.");
    terminal_write("\x1B[0m");
    terminal_write("\x1B[90m");
    terminal_write("Tip: Type ");
    terminal_write("\x1B[36m");
    terminal_write("'help 1-4'");
    terminal_write("\x1B[90m");
    terminal_write(" to see available commands.");
    terminal_write_line("");
    terminal_write("\x1B[0m");
    terminal_write_line("");

        while (1) {
        shell_maybe_autosave();
        
        shell_print_prompt();
        shell_read_line_with_history(buffer, SHELL_BUFFER_SIZE, shell_history_data, &shell_history_count, &shell_history_index);
        if (buffer[0] != '\0') {
            shell_execute(buffer);
            terminal_scroll_to_bottom();
        }
        
        mouse_state_t state = get_mouse_state();
        if (state.scroll != 0) {
            // Отображаем скролл в терминале
            terminal_write("\x1B[36m[MOUSE]\x1B[0m Scroll: ");
            if (state.scroll > 0) {
                terminal_write("\x1B[32m+");  // Зеленый для вверх
                print_uint64(state.scroll);
                terminal_write("\x1B[0m");
                terminal_write_line(" (up)");
            } else {
                terminal_write("\x1B[31m");   // Красный для вниз
                print_uint64(-state.scroll);
                terminal_write("\x1B[0m");
                terminal_write_line(" (down)");
            }
            state.scroll = 0;
        }
    }
}


