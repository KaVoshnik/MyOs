#include <filesystem.h>
#include <memory.h>
#include <string.h>

typedef struct fs_node {
    char name[FS_MAX_NAME_LEN];
    fs_node_type_t type;
    struct fs_node *parent;
    struct fs_node *children;
    struct fs_node *next_sibling;
    uint8_t *data;
    size_t size;
    size_t capacity;
} fs_node_t;

static fs_node_t *fs_root = NULL;
static fs_node_t *fs_cwd = NULL;

static const char *fs_skip_separators(const char *path) {
    while (path && *path == '/') {
        ++path;
    }
    return path;
}

static void fs_copy_name(char dest[FS_MAX_NAME_LEN], const char *src) {
    size_t i = 0;
    while (src[i] != '\0' && i < FS_MAX_NAME_LEN - 1) {
        dest[i] = src[i];
        ++i;
    }
    dest[i] = '\0';
}

static const char *fs_read_component(const char *path, char buffer[FS_MAX_NAME_LEN], int *too_long) {
    size_t i = 0;
    int overflow = 0;
    while (path && *path != '\0' && *path != '/') {
        if (i < FS_MAX_NAME_LEN - 1) {
            buffer[i++] = *path;
        } else {
            overflow = 1;
        }
        ++path;
    }
    buffer[i] = '\0';
    if (too_long) {
        *too_long = overflow;
    }
    return path;
}

static fs_node_t *fs_find_child(fs_node_t *parent, const char *name) {
    if (!parent || parent->type != FS_NODE_DIRECTORY) {
        return NULL;
    }

    fs_node_t *child = parent->children;
    while (child) {
        if (strcmp(child->name, name) == 0) {
            return child;
        }
        child = child->next_sibling;
    }
    return NULL;
}

static void fs_attach_child(fs_node_t *parent, fs_node_t *child) {
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
}

static fs_node_t *fs_alloc_node(const char *name, fs_node_type_t type) {
    fs_node_t *node = (fs_node_t *)kmalloc(sizeof(fs_node_t));
    if (!node) {
        return NULL;
    }
    memset(node, 0, sizeof(fs_node_t));
    fs_copy_name(node->name, name);
    node->type = type;
    return node;
}

static fs_node_t *fs_start_for_path(const char *path) {
    if (!fs_root) {
        return NULL;
    }

    if (!path || *path == '\0') {
        return fs_cwd ? fs_cwd : fs_root;
    }

    if (*path == '/') {
        return fs_root;
    }

    return fs_cwd ? fs_cwd : fs_root;
}

static fs_node_t *fs_walk(const char *path) {
    fs_node_t *current = fs_start_for_path(path);
    if (!current) {
        return NULL;
    }

    if (!path || *path == '\0') {
        return current;
    }

    const char *cursor = path;
    if (*cursor == '/') {
        cursor = fs_skip_separators(cursor);
        if (*cursor == '\0') {
            return current;
        }
    }

    char component[FS_MAX_NAME_LEN];
    int overflow = 0;
    cursor = fs_skip_separators(cursor);
    while (*cursor != '\0') {
        cursor = fs_read_component(cursor, component, &overflow);
        if (overflow) {
            return NULL;
        }
        cursor = fs_skip_separators(cursor);

        if (component[0] == '\0' || component[0] == '.') {
            if (component[1] == '\0') {
                continue;
            }
            if (component[1] == '.' && component[2] == '\0') {
                if (current->parent) {
                    current = current->parent;
                }
                continue;
            }
        }

        if (current->type != FS_NODE_DIRECTORY) {
            return NULL;
        }

        fs_node_t *next = fs_find_child(current, component);
        if (!next) {
            return NULL;
        }
        current = next;
    }

    return current;
}

static fs_status_t fs_prepare_parent(const char *path, fs_node_t **parent_out, char leaf[FS_MAX_NAME_LEN]) {
    if (!path || *path == '\0') {
        return FS_ERR_INVALID;
    }

    fs_node_t *current = fs_start_for_path(path);
    if (!current) {
        return FS_ERR_INVALID;
    }

    const char *cursor = path;
    if (*cursor == '/') {
        cursor = fs_skip_separators(cursor);
        if (*cursor == '\0') {
            return FS_ERR_INVALID;
        }
        current = fs_root;
    }

    char component[FS_MAX_NAME_LEN];
    int overflow = 0;
    cursor = fs_skip_separators(cursor);
    while (*cursor != '\0') {
        cursor = fs_read_component(cursor, component, &overflow);
        if (overflow) {
            return FS_ERR_INVALID;
        }
        cursor = fs_skip_separators(cursor);
        int more = (*cursor != '\0');

        if (!more) {
            if (component[0] == '\0' ||
                (component[0] == '.' && component[1] == '\0') ||
                (component[0] == '.' && component[1] == '.' && component[2] == '\0')) {
                return FS_ERR_INVALID;
            }
            if (parent_out) {
                *parent_out = current;
            }
            if (leaf) {
                fs_copy_name(leaf, component);
            }
            return FS_OK;
        }

        if (component[0] == '\0' || (component[0] == '.' && component[1] == '\0')) {
            continue;
        }

        if (component[0] == '.' && component[1] == '.' && component[2] == '\0') {
            if (current->parent) {
                current = current->parent;
            }
            continue;
        }

        fs_node_t *next = fs_find_child(current, component);
        if (!next || next->type != FS_NODE_DIRECTORY) {
            return FS_ERR_NOENT;
        }
        current = next;
    }

    return FS_ERR_INVALID;
}

static fs_status_t fs_reserve(fs_node_t *node, size_t new_size) {
    if (!node) {
        return FS_ERR_INVALID;
    }

    if (new_size <= node->capacity) {
        return FS_OK;
    }

    size_t capacity = node->capacity ? node->capacity : 64;
    while (capacity < new_size) {
        capacity *= 2;
    }

    uint8_t *buffer = (uint8_t *)kmalloc(capacity);
    if (!buffer) {
        return FS_ERR_NOMEM;
    }
    if (node->data && node->size > 0) {
        memcpy(buffer, node->data, node->size);
        kfree(node->data);
    }
    node->data = buffer;
    node->capacity = capacity;
    return FS_OK;
}

static void fs_seed(void) {
    fs_mkdir("/etc");
    fs_create_file("/etc/motd");
    const char *motd = "Welcome to MyOs!\nUse 'help' to discover shell commands.\n";
    fs_write_file("/etc/motd", motd, strlen(motd));

    fs_mkdir("/docs");
    fs_create_file("/docs/readme.txt");
    const char *readme = "MyOs RAM filesystem demo.\nAvailable commands: ls, cd, pwd, cat, touch, write, append.\n";
    fs_write_file("/docs/readme.txt", readme, strlen(readme));
}

void fs_init(void) {
    fs_root = fs_alloc_node("/", FS_NODE_DIRECTORY);
    if (!fs_root) {
        return;
    }
    fs_root->parent = fs_root;
    fs_cwd = fs_root;
    fs_seed();
}

fs_status_t fs_mkdir(const char *path) {
    if (!fs_root) {
        return FS_ERR_INVALID;
    }

    if (fs_walk(path)) {
        return FS_ERR_EXIST;
    }

    fs_node_t *parent = NULL;
    char leaf[FS_MAX_NAME_LEN];
    fs_status_t status = fs_prepare_parent(path, &parent, leaf);
    if (status != FS_OK) {
        return status;
    }

    if (!parent || parent->type != FS_NODE_DIRECTORY) {
        return FS_ERR_NOTDIR;
    }

    fs_node_t *node = fs_alloc_node(leaf, FS_NODE_DIRECTORY);
    if (!node) {
        return FS_ERR_NOMEM;
    }
    fs_attach_child(parent, node);
    return FS_OK;
}

fs_status_t fs_create_file(const char *path) {
    if (!fs_root) {
        return FS_ERR_INVALID;
    }

    if (fs_walk(path)) {
        return FS_ERR_EXIST;
    }

    fs_node_t *parent = NULL;
    char leaf[FS_MAX_NAME_LEN];
    fs_status_t status = fs_prepare_parent(path, &parent, leaf);
    if (status != FS_OK) {
        return status;
    }

    if (!parent || parent->type != FS_NODE_DIRECTORY) {
        return FS_ERR_NOTDIR;
    }

    fs_node_t *node = fs_alloc_node(leaf, FS_NODE_FILE);
    if (!node) {
        return FS_ERR_NOMEM;
    }
    fs_attach_child(parent, node);
    return FS_OK;
}

fs_status_t fs_write_file(const char *path, const void *data, size_t size) {
    fs_node_t *node = fs_walk(path);
    if (!node) {
        return FS_ERR_NOENT;
    }
    if (node->type != FS_NODE_FILE) {
        return FS_ERR_ISDIR;
    }

    fs_status_t status = fs_reserve(node, size);
    if (status != FS_OK) {
        return status;
    }

    if (size > 0 && data) {
        memcpy(node->data, data, size);
    }
    node->size = size;
    return FS_OK;
}

fs_status_t fs_append_file(const char *path, const void *data, size_t size) {
    fs_node_t *node = fs_walk(path);
    if (!node) {
        return FS_ERR_NOENT;
    }
    if (node->type != FS_NODE_FILE) {
        return FS_ERR_ISDIR;
    }

    fs_status_t status = fs_reserve(node, node->size + size);
    if (status != FS_OK) {
        return status;
    }

    if (size > 0 && data) {
        memcpy(node->data + node->size, data, size);
    }
    node->size += size;
    return FS_OK;
}

fs_status_t fs_read_file(const char *path, void *buffer, size_t buffer_size, size_t *out_size) {
    fs_node_t *node = fs_walk(path);
    if (!node) {
        return FS_ERR_NOENT;
    }
    if (node->type != FS_NODE_FILE) {
        return FS_ERR_ISDIR;
    }

    size_t to_copy = (buffer_size < node->size) ? buffer_size : node->size;
    if (buffer && to_copy > 0) {
        memcpy(buffer, node->data, to_copy);
    }
    if (out_size) {
        *out_size = node->size;
    }
    return FS_OK;
}

const uint8_t *fs_get_file_data(const char *path, size_t *out_size) {
    fs_node_t *node = fs_walk(path);
    if (!node || node->type != FS_NODE_FILE) {
        return NULL;
    }
    if (out_size) {
        *out_size = node->size;
    }
    return node->data;
}

fs_status_t fs_list_dir(const char *path, fs_list_callback_t callback, void *user_data) {
    fs_node_t *node = fs_walk(path);
    if (!node) {
        return FS_ERR_NOENT;
    }
    if (node->type != FS_NODE_DIRECTORY) {
        return FS_ERR_NOTDIR;
    }

    fs_dir_entry_t entry;
    fs_node_t *child = node->children;
    while (child) {
        entry.name = child->name;
        entry.size = child->size;
        entry.is_directory = (child->type == FS_NODE_DIRECTORY);
        if (callback) {
            callback(&entry, user_data);
        }
        child = child->next_sibling;
    }
    return FS_OK;
}

fs_status_t fs_change_dir(const char *path) {
    fs_node_t *node = fs_walk(path);
    if (!node) {
        return FS_ERR_NOENT;
    }
    if (node->type != FS_NODE_DIRECTORY) {
        return FS_ERR_NOTDIR;
    }
    fs_cwd = node;
    return FS_OK;
}

void fs_get_cwd(char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size < 2 || !fs_cwd) {
        if (buffer && buffer_size > 0) {
            buffer[0] = '\0';
        }
        return;
    }

    const size_t max_components = FS_MAX_PATH_LEN / 2;
    fs_node_t *components[max_components];
    size_t depth = 0;
    fs_node_t *node = fs_cwd;
    while (node && node != fs_root && depth < max_components) {
        components[depth++] = node;
        node = node->parent;
    }

    size_t pos = 0;
    buffer[pos++] = '/';

    for (size_t i = 0; i < depth && pos < buffer_size - 1; ++i) {
        fs_node_t *component_node = components[depth - i - 1];
        size_t len = strlen(component_node->name);
        if (pos + len >= buffer_size) {
            break;
        }
        memcpy(&buffer[pos], component_node->name, len);
        pos += len;
        if (i != depth - 1 && pos < buffer_size - 1) {
            buffer[pos++] = '/';
        }
    }

    buffer[pos] = '\0';
}

int fs_exists(const char *path) {
    return fs_walk(path) != NULL;
}

int fs_is_dir(const char *path) {
    fs_node_t *node = fs_walk(path);
    if (!node) {
        return 0;
    }
    return node->type == FS_NODE_DIRECTORY;
}


