#ifndef _MYOS_USER_H
#define _MYOS_USER_H

#include <stdint.h>
#include <stddef.h>

#define USERNAME_MAX_LEN 32
#define PASSWORD_MAX_LEN 64
#define CONFIG_FILE_PATH "/etc/config"
#define USERS_FILE_PATH "/etc/users"

typedef struct user {
    char username[USERNAME_MAX_LEN];
    char password_hash[64];  /* Simple hash storage */
    uint64_t uid;
    int is_admin;
} user_t;

typedef struct system_config {
    int first_boot;           /* 1 = first boot, 0 = normal boot */
    char default_user[USERNAME_MAX_LEN];
    int auto_login;           /* 1 = auto login, 0 = require login */
} system_config_t;

/* User management */
void user_system_init(void);
void user_system_save_users(void);
int user_create(const char *username, const char *password, int is_admin);
int user_authenticate(const char *username, const char *password);
user_t *user_get_current(void);
user_t *user_find_by_name(const char *username);
int user_set_current(const char *username);
void user_logout(void);
const char *user_get_current_username(void);
uint64_t user_get_current_uid(void);
int user_is_admin(void);

/* Configuration management */
int config_load(system_config_t *config);
int config_save(const system_config_t *config);
int config_is_first_boot(void);
int config_set_first_boot(int value);

/* Password hashing (simple implementation) */
void password_hash(const char *password, char *hash_out, size_t hash_size);
int password_verify(const char *password, const char *hash);

#endif /* _MYOS_USER_H */

