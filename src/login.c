#include <user.h>
#include <login.h>
#include <terminal.h>
#include <keyboard.h>
#include <string.h>
#include <memory.h>

#define LOGIN_BUFFER_SIZE 64

static void read_password(char *buffer, size_t buffer_size) {
    size_t pos = 0;
    buffer[0] = '\0';
    
    terminal_write("Password: ");
    
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
                terminal_write("*"); /* Show asterisk instead of actual password */
            }
        }
    }
}

static void read_line(char *buffer, size_t buffer_size) {
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
                terminal_putc(c);
            }
        }
    }
}

int login_screen(void) {
    terminal_clear();
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREEN, TERMINAL_COLOR_BLACK);
    terminal_write_line("╔════════════════════════════════════╗");
    terminal_write_line("║         Welcome to MyOs           ║");
    terminal_write_line("╚════════════════════════════════════╝");
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);
    terminal_write_line("");
    
    char username[LOGIN_BUFFER_SIZE];
    char password[LOGIN_BUFFER_SIZE];
    int attempts = 0;
    const int max_attempts = 3;
    
    while (attempts < max_attempts) {
        terminal_write("Username: ");
        read_line(username, sizeof(username));
        
        if (username[0] == '\0') {
            continue;
        }
        
        read_password(password, sizeof(password));
        
        if (user_authenticate(username, password)) {
            if (user_set_current(username) == 0) {
                terminal_set_color(TERMINAL_COLOR_LIGHT_GREEN, TERMINAL_COLOR_BLACK);
                terminal_write("Welcome, ");
                terminal_write(username);
                terminal_write_line("!");
                terminal_set_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);
                terminal_write_line("");
                return 0;
            }
        }
        
        attempts++;
        terminal_set_color(TERMINAL_COLOR_LIGHT_RED, TERMINAL_COLOR_BLACK);
        terminal_write("Login failed. ");
        if (attempts < max_attempts) {
            terminal_write("Try again.");
        } else {
            terminal_write("Too many attempts.");
        }
        terminal_write_line("");
        terminal_set_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);
        terminal_write_line("");
    }
    
    return -1;
}

int first_boot_setup(void) {
    terminal_clear();
    terminal_set_color(TERMINAL_COLOR_LIGHT_CYAN, TERMINAL_COLOR_BLACK);
    terminal_write_line("╔════════════════════════════════════════════╗");
    terminal_write_line("║      MyOs First Boot Setup                ║");
    terminal_write_line("╚════════════════════════════════════════════╝");
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);
    terminal_write_line("");
    terminal_write_line("Welcome! Let's set up your system.");
    terminal_write_line("");
    
    char username[LOGIN_BUFFER_SIZE];
    char password[LOGIN_BUFFER_SIZE];
    char password_confirm[LOGIN_BUFFER_SIZE];
    int is_admin = 1; /* First user is always admin */
    
    /* Get username */
    while (1) {
        terminal_write("Enter username: ");
        read_line(username, sizeof(username));
        
        if (username[0] == '\0') {
            terminal_write_line("Username cannot be empty.");
            continue;
        }
        
        if (strlen(username) < 3) {
            terminal_write_line("Username must be at least 3 characters.");
            continue;
        }
        
        if (user_find_by_name(username) != NULL) {
            terminal_write_line("Username already exists.");
            continue;
        }
        
        break;
    }
    
    /* Get password */
    while (1) {
        terminal_write("Enter password: ");
        read_password(password, sizeof(password));
        
        if (strlen(password) < 4) {
            terminal_write_line("Password must be at least 4 characters.");
            continue;
        }
        
        terminal_write("Confirm password: ");
        read_password(password_confirm, sizeof(password_confirm));
        
        if (strcmp(password, password_confirm) != 0) {
            terminal_write_line("Passwords do not match. Try again.");
            continue;
        }
        
        break;
    }
    
    /* Create user */
    if (user_create(username, password, is_admin) != 0) {
        terminal_set_color(TERMINAL_COLOR_LIGHT_RED, TERMINAL_COLOR_BLACK);
        terminal_write_line("Failed to create user.");
        terminal_set_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);
        return -1;
    }
    
    /* Set as current user */
    user_set_current(username);
    
    /* Save configuration */
    system_config_t config;
    config_load(&config);
    config.first_boot = 0;
    strncpy(config.default_user, username, USERNAME_MAX_LEN - 1);
    config.default_user[USERNAME_MAX_LEN - 1] = '\0';
    config.auto_login = 0;
    config_save(&config);
    
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREEN, TERMINAL_COLOR_BLACK);
    terminal_write_line("");
    terminal_write("User '");
    terminal_write(username);
    terminal_write_line("' created successfully!");
    terminal_set_color(TERMINAL_COLOR_LIGHT_GREY, TERMINAL_COLOR_BLACK);
    terminal_write_line("");
    terminal_write_line("Setup complete! Press Enter to continue...");
    
    /* Wait for Enter */
    while (1) {
        uint16_t code;
        while (!keyboard_try_read_char_extended(&code)) {
            __asm__ volatile("hlt");
        }
        if (code < 256 && ((char)code == '\r' || (char)code == '\n')) {
            break;
        }
    }
    
    terminal_clear();
    return 0;
}

