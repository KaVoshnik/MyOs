#include <user.h>
#include <filesystem.h>
#include <string.h>
#include <memory.h>

#define MAX_USERS 16
static user_t users[MAX_USERS];
static size_t user_count = 0;
static user_t *current_user = NULL;
static uint64_t next_uid = 1000;

/* Simple hash function for passwords (djb2 algorithm) */
static uint32_t simple_hash(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

void password_hash(const char *password, char *hash_out, size_t hash_size) {
    if (!password || !hash_out || hash_size < 9) {
        return;
    }
    uint32_t hash = simple_hash(password);
    /* Convert to hex string */
    static const char hex_digits[] = "0123456789ABCDEF";
    for (int i = 0; i < 8; ++i) {
        hash_out[i] = hex_digits[(hash >> (28 - i * 4)) & 0xF];
    }
    hash_out[8] = '\0';
}

int password_verify(const char *password, const char *hash) {
    if (!password || !hash) {
        return 0;
    }
    char computed_hash[64];
    password_hash(password, computed_hash, sizeof(computed_hash));
    return strcmp(computed_hash, hash) == 0;
}

static void user_system_load_users(void) {
    if (!fs_exists(USERS_FILE_PATH)) {
        return;
    }
    
    size_t size = 0;
    const uint8_t *data = fs_get_file_data(USERS_FILE_PATH, &size);
    if (!data || size == 0) {
        return;
    }
    
    /* Simple format: username:hash:uid:admin\n */
    const char *text = (const char *)data;
    size_t pos = 0;
    user_count = 0;
    
    while (pos < size && user_count < MAX_USERS) {
        user_t *user = &users[user_count];
        memset(user, 0, sizeof(user_t));
        
        /* Read username */
        size_t i = 0;
        while (pos < size && text[pos] != ':' && i < USERNAME_MAX_LEN - 1) {
            user->username[i++] = text[pos++];
        }
        user->username[i] = '\0';
        if (pos >= size || text[pos] != ':') break;
        pos++; /* Skip ':' */
        
        /* Read password hash */
        i = 0;
        while (pos < size && text[pos] != ':' && i < 63) {
            user->password_hash[i++] = text[pos++];
        }
        user->password_hash[i] = '\0';
        if (pos >= size || text[pos] != ':') break;
        pos++; /* Skip ':' */
        
        /* Read UID */
        user->uid = 0;
        while (pos < size && text[pos] >= '0' && text[pos] <= '9') {
            user->uid = user->uid * 10 + (text[pos] - '0');
            pos++;
        }
        if (pos >= size || text[pos] != ':') break;
        pos++; /* Skip ':' */
        
        /* Read admin flag */
        if (pos < size && text[pos] == '1') {
            user->is_admin = 1;
        } else {
            user->is_admin = 0;
        }
        pos++; /* Skip admin flag */
        
        /* Skip newline */
        if (pos < size && text[pos] == '\n') {
            pos++;
        }
        
        if (user->uid >= next_uid) {
            next_uid = user->uid + 1;
        }
        
        user_count++;
    }
}

void user_system_save_users(void) {
    if (!fs_exists(USERS_FILE_PATH)) {
        fs_create_file(USERS_FILE_PATH);
    }
    
    /* Build users file content */
    char buffer[4096];
    size_t pos = 0;
    
    for (size_t i = 0; i < user_count && pos < sizeof(buffer) - 100; ++i) {
        user_t *user = &users[i];
        int written = 0;
        
        /* Format: username:hash:uid:admin\n */
        const char *fmt = "%s:%s:%llu:%d\n";
        /* Simple sprintf-like function */
        size_t len = strlen(user->username);
        if (pos + len < sizeof(buffer) - 1) {
            memcpy(buffer + pos, user->username, len);
            pos += len;
        }
        buffer[pos++] = ':';
        
        len = strlen(user->password_hash);
        if (pos + len < sizeof(buffer) - 1) {
            memcpy(buffer + pos, user->password_hash, len);
            pos += len;
        }
        buffer[pos++] = ':';
        
        /* Write UID */
        uint64_t uid = user->uid;
        char uid_str[21];
        int uid_len = 0;
        if (uid == 0) {
            uid_str[uid_len++] = '0';
        } else {
            char temp[21];
            int temp_len = 0;
            while (uid > 0) {
                temp[temp_len++] = '0' + (uid % 10);
                uid /= 10;
            }
            for (int j = temp_len - 1; j >= 0; --j) {
                uid_str[uid_len++] = temp[j];
            }
        }
        uid_str[uid_len] = '\0';
        if (pos + uid_len < sizeof(buffer) - 1) {
            memcpy(buffer + pos, uid_str, uid_len);
            pos += uid_len;
        }
        buffer[pos++] = ':';
        
        buffer[pos++] = user->is_admin ? '1' : '0';
        buffer[pos++] = '\n';
    }
    
    buffer[pos] = '\0';
    fs_write_file(USERS_FILE_PATH, buffer, pos);
}

void user_system_init(void) {
    memset(users, 0, sizeof(users));
    user_count = 0;
    current_user = NULL;
    next_uid = 1000;
    
    user_system_load_users();
}

int user_create(const char *username, const char *password, int is_admin) {
    if (!username || !password || user_count >= MAX_USERS) {
        return -1;
    }
    
    /* Check if user already exists */
    for (size_t i = 0; i < user_count; ++i) {
        if (strcmp(users[i].username, username) == 0) {
            return -2; /* User already exists */
        }
    }
    
    user_t *user = &users[user_count];
    memset(user, 0, sizeof(user_t));
    
    strncpy(user->username, username, USERNAME_MAX_LEN - 1);
    user->username[USERNAME_MAX_LEN - 1] = '\0';
    
    password_hash(password, user->password_hash, sizeof(user->password_hash));
    user->uid = next_uid++;
    user->is_admin = is_admin ? 1 : 0;
    
    user_count++;
    user_system_save_users();
    
    return 0;
}

int user_authenticate(const char *username, const char *password) {
    if (!username || !password) {
        return 0;
    }
    
    for (size_t i = 0; i < user_count; ++i) {
        if (strcmp(users[i].username, username) == 0) {
            return password_verify(password, users[i].password_hash);
        }
    }
    
    return 0;
}

user_t *user_find_by_name(const char *username) {
    if (!username) {
        return NULL;
    }
    
    for (size_t i = 0; i < user_count; ++i) {
        if (strcmp(users[i].username, username) == 0) {
            return &users[i];
        }
    }
    
    return NULL;
}

int user_set_current(const char *username) {
    user_t *user = user_find_by_name(username);
    if (!user) {
        return -1;
    }
    current_user = user;
    return 0;
}

void user_logout(void) {
    current_user = NULL;
}

user_t *user_get_current(void) {
    return current_user;
}

const char *user_get_current_username(void) {
    if (!current_user) {
        return NULL;
    }
    return current_user->username;
}

uint64_t user_get_current_uid(void) {
    if (!current_user) {
        return 0;
    }
    return current_user->uid;
}

int user_is_admin(void) {
    if (!current_user) {
        return 0;
    }
    return current_user->is_admin;
}

int config_load(system_config_t *config) {
    if (!config) {
        return -1;
    }
    
    memset(config, 0, sizeof(system_config_t));
    config->first_boot = 1; /* Default to first boot */
    
    if (!fs_exists(CONFIG_FILE_PATH)) {
        return 0; /* Config doesn't exist, use defaults */
    }
    
    size_t size = 0;
    const uint8_t *data = fs_get_file_data(CONFIG_FILE_PATH, &size);
    if (!data || size == 0) {
        return 0;
    }
    
    /* Simple format: first_boot=1\ndefault_user=username\nauto_login=0\n */
    const char *text = (const char *)data;
    
    /* Parse first_boot */
    if (strstr(text, "first_boot=0") != NULL) {
        config->first_boot = 0;
    } else if (strstr(text, "first_boot=1") != NULL) {
        config->first_boot = 1;
    }
    
    /* Parse default_user */
    const char *user_start = strstr(text, "default_user=");
    if (user_start) {
        user_start += 12; /* Skip "default_user=" */
        size_t i = 0;
        while (*user_start != '\n' && *user_start != '\0' && i < USERNAME_MAX_LEN - 1) {
            config->default_user[i++] = *user_start++;
        }
        config->default_user[i] = '\0';
    }
    
    /* Parse auto_login */
    if (strstr(text, "auto_login=1") != NULL) {
        config->auto_login = 1;
    } else if (strstr(text, "auto_login=0") != NULL) {
        config->auto_login = 0;
    }
    
    return 0;
}

int config_save(const system_config_t *config) {
    if (!config) {
        return -1;
    }
    
    if (!fs_exists(CONFIG_FILE_PATH)) {
        fs_create_file(CONFIG_FILE_PATH);
    }
    
    char buffer[256];
    size_t pos = 0;
    
    /* Write first_boot */
    const char *fb = config->first_boot ? "first_boot=1\n" : "first_boot=0\n";
    size_t len = strlen(fb);
    if (pos + len < sizeof(buffer)) {
        memcpy(buffer + pos, fb, len);
        pos += len;
    }
    
    /* Write default_user */
    if (config->default_user[0] != '\0') {
        const char *du = "default_user=";
        len = strlen(du);
        if (pos + len < sizeof(buffer)) {
            memcpy(buffer + pos, du, len);
            pos += len;
        }
        len = strlen(config->default_user);
        if (pos + len < sizeof(buffer) - 1) {
            memcpy(buffer + pos, config->default_user, len);
            pos += len;
        }
        buffer[pos++] = '\n';
    }
    
    /* Write auto_login */
    const char *al = config->auto_login ? "auto_login=1\n" : "auto_login=0\n";
    len = strlen(al);
    if (pos + len < sizeof(buffer)) {
        memcpy(buffer + pos, al, len);
        pos += len;
    }
    
    fs_write_file(CONFIG_FILE_PATH, buffer, pos);
    return 0;
}

int config_is_first_boot(void) {
    system_config_t config;
    config_load(&config);
    return config.first_boot;
}

int config_set_first_boot(int value) {
    system_config_t config;
    config_load(&config);
    config.first_boot = value ? 1 : 0;
    return config_save(&config);
}

