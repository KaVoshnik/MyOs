#ifndef _MYOS_FILESYSTEM_H
#define _MYOS_FILESYSTEM_H

#include <stddef.h>
#include <stdint.h>

#define FS_MAX_NAME_LEN 32
#define FS_MAX_PATH_LEN 256

typedef enum fs_node_type {
    FS_NODE_DIRECTORY = 0,
    FS_NODE_FILE = 1
} fs_node_type_t;

typedef struct fs_dir_entry {
    const char *name;
    size_t size;
    int is_directory;
} fs_dir_entry_t;

typedef enum fs_status {
    FS_OK = 0,
    FS_ERR_NOENT = -1,
    FS_ERR_EXIST = -2,
    FS_ERR_NOTDIR = -3,
    FS_ERR_ISDIR = -4,
    FS_ERR_NOMEM = -5,
    FS_ERR_INVALID = -6
} fs_status_t;

typedef void (*fs_list_callback_t)(const fs_dir_entry_t *entry, void *user_data);

void fs_init(void);
fs_status_t fs_mkdir(const char *path);
fs_status_t fs_create_file(const char *path);
fs_status_t fs_write_file(const char *path, const void *data, size_t size);
fs_status_t fs_append_file(const char *path, const void *data, size_t size);
fs_status_t fs_read_file(const char *path, void *buffer, size_t buffer_size, size_t *out_size);
const uint8_t *fs_get_file_data(const char *path, size_t *out_size);
fs_status_t fs_list_dir(const char *path, fs_list_callback_t callback, void *user_data);
fs_status_t fs_change_dir(const char *path);
void fs_get_cwd(char *buffer, size_t buffer_size);
int fs_exists(const char *path);
int fs_is_dir(const char *path);

#endif /* _MYOS_FILESYSTEM_H */


