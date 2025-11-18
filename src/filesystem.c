#include <filesystem.h>
#include <memory.h>
#include <string.h>
#include <ata.h>

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

#define FS_IMAGE_MAGIC        0x4D594653u
#define FS_IMAGE_VERSION      1u
#define FS_IMAGE_LBA_START    2048u
#define FS_IMAGE_LBA_COUNT    256u
#define FS_IMAGE_SECTOR_SIZE  512u
#define FS_IMAGE_BUFFER_SIZE  (FS_IMAGE_LBA_COUNT * FS_IMAGE_SECTOR_SIZE)

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t total_size;
    uint32_t entry_count;
} fs_image_header_t;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t reserved;
    uint16_t path_len;
    uint32_t data_len;
} fs_image_entry_t;

static uint8_t *fs_image_buffer = NULL;

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

static void fs_detach_child(fs_node_t *node) {
    if (!node || !node->parent) {
        return;
    }

    fs_node_t **cursor = &node->parent->children;
    while (*cursor && *cursor != node) {
        cursor = &(*cursor)->next_sibling;
    }
    if (*cursor == node) {
        *cursor = node->next_sibling;
    }
    node->parent = NULL;
    node->next_sibling = NULL;
}

static void fs_free_subtree(fs_node_t *node) {
    if (!node) {
        return;
    }
    fs_node_t *child = node->children;
    while (child) {
        fs_node_t *next = child->next_sibling;
        fs_free_subtree(child);
        child = next;
    }
    if (node->data) {
        kfree(node->data);
    }
    kfree(node);
}

static void fs_clear_children(fs_node_t *node) {
    if (!node) {
        return;
    }
    fs_node_t *child = node->children;
    while (child) {
        fs_node_t *next = child->next_sibling;
        fs_free_subtree(child);
        child = next;
    }
    node->children = NULL;
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
    const char *readme =
        "MyOs RAM filesystem demo.\n"
        "Try: ls, cd, pwd, cat, touch, write, append, mkdir, rm, savefs, loadfs.\n";
    fs_write_file("/docs/readme.txt", readme, strlen(readme));
}

void fs_init(void) {
    fs_root = fs_alloc_node("/", FS_NODE_DIRECTORY);
    if (!fs_root) {
        return;
    }
    fs_root->parent = fs_root;
    fs_cwd = fs_root;
    
    if (fs_persistence_available() && !fs_image_buffer) {
        fs_image_buffer = (uint8_t *)kmalloc(FS_IMAGE_BUFFER_SIZE);
        if (fs_image_buffer && fs_load() == FS_OK) {
            return;
        }
    }
    
    fs_seed();
    if (fs_persistence_available() && fs_image_buffer) {
        fs_save();
    }
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

static void fs_build_path_from_node(fs_node_t *node, char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size < 2 || !node) {
        if (buffer && buffer_size > 0) {
            buffer[0] = '\0';
        }
        return;
    }

    const size_t max_components = FS_MAX_PATH_LEN / 2;
    fs_node_t *components[max_components];
    size_t depth = 0;
    fs_node_t *current = node;
    while (current && current != fs_root && depth < max_components) {
        components[depth++] = current;
        current = current->parent;
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

void fs_get_cwd(char *buffer, size_t buffer_size) {
    fs_build_path_from_node(fs_cwd, buffer, buffer_size);
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

fs_status_t fs_remove(const char *path, int recursive) {
    fs_node_t *node = fs_walk(path);
    if (!node) {
        return FS_ERR_NOENT;
    }
    if (node == fs_root) {
        return FS_ERR_INVALID;
    }

    if (node->type == FS_NODE_DIRECTORY && node->children && !recursive) {
        return FS_ERR_NOTEMPTY;
    }

    if (node == fs_cwd) {
        fs_cwd = node->parent ? node->parent : fs_root;
    }

    fs_detach_child(node);
    fs_free_subtree(node);
    return FS_OK;
}

typedef struct {
    uint8_t *buffer;
    size_t capacity;
    size_t position;
} fs_stream_t;

static int fs_stream_write(fs_stream_t *stream, const void *data, size_t length) {
    if (stream->position + length > stream->capacity) {
        return 0;
    }
    memcpy(stream->buffer + stream->position, data, length);
    stream->position += length;
    return 1;
}

static fs_status_t fs_write_entry(fs_stream_t *stream, fs_node_t *node, uint32_t *entry_count) {
    char path[FS_MAX_PATH_LEN];
    fs_build_path_from_node(node, path, sizeof(path));
    size_t path_len = strlen(path);
    if (path_len == 0 || path_len >= FS_MAX_PATH_LEN) {
        return FS_ERR_INVALID;
    }
    if (path_len > 0xFFFF) {
        return FS_ERR_INVALID;
    }

    fs_image_entry_t entry;
    entry.type = (uint8_t)node->type;
    entry.reserved = 0;
    entry.path_len = (uint16_t)path_len;
    entry.data_len = (node->type == FS_NODE_FILE) ? (uint32_t)node->size : 0;

    if (!fs_stream_write(stream, &entry, sizeof(entry))) {
        return FS_ERR_NOMEM;
    }
    if (!fs_stream_write(stream, path, path_len)) {
        return FS_ERR_NOMEM;
    }
    if (entry.data_len > 0) {
        if (!fs_stream_write(stream, node->data, node->size)) {
            return FS_ERR_NOMEM;
        }
    }

    (*entry_count)++;
    return FS_OK;
}

static fs_status_t fs_serialize_node(fs_stream_t *stream, fs_node_t *node, uint32_t *entry_count) {
    if (node != fs_root) {
        fs_status_t status = fs_write_entry(stream, node, entry_count);
        if (status != FS_OK) {
            return status;
        }
    }

    fs_node_t *child = node->children;
    while (child) {
        fs_status_t status = fs_serialize_node(stream, child, entry_count);
        if (status != FS_OK) {
            return status;
        }
        child = child->next_sibling;
    }
    return FS_OK;
}

static fs_status_t fs_serialize_to_buffer(size_t *out_size) {
    if (!fs_image_buffer) {
        return FS_ERR_NOMEM;
    }

    fs_stream_t stream = {
        .buffer = fs_image_buffer,
        .capacity = FS_IMAGE_BUFFER_SIZE,
        .position = sizeof(fs_image_header_t)
    };

    uint32_t entry_count = 0;
    fs_status_t status = fs_serialize_node(&stream, fs_root, &entry_count);
    if (status != FS_OK) {
        return status;
    }

    fs_image_header_t header;
    header.magic = FS_IMAGE_MAGIC;
    header.version = FS_IMAGE_VERSION;
    header.entry_count = entry_count;
    header.total_size = (uint32_t)stream.position;

    if (header.total_size > FS_IMAGE_BUFFER_SIZE) {
        return FS_ERR_NOMEM;
    }

    memcpy(fs_image_buffer, &header, sizeof(header));

    size_t padding = 0;
    if (stream.position % FS_IMAGE_SECTOR_SIZE != 0) {
        padding = FS_IMAGE_SECTOR_SIZE - (stream.position % FS_IMAGE_SECTOR_SIZE);
        if (stream.position + padding > FS_IMAGE_BUFFER_SIZE) {
            return FS_ERR_NOMEM;
        }
        memset(fs_image_buffer + stream.position, 0, padding);
        stream.position += padding;
    }

    if (out_size) {
        *out_size = stream.position;
    }

    return FS_OK;
}

static fs_status_t fs_deserialize_from_buffer(size_t total_size, uint32_t entry_count) {
    if (total_size < sizeof(fs_image_header_t) || total_size > FS_IMAGE_BUFFER_SIZE) {
        return FS_ERR_INVALID;
    }

    const uint8_t *cursor = fs_image_buffer + sizeof(fs_image_header_t);
    size_t remaining = total_size - sizeof(fs_image_header_t);

    fs_clear_children(fs_root);
    fs_cwd = fs_root;

    char path[FS_MAX_PATH_LEN];

    for (uint32_t i = 0; i < entry_count; ++i) {
        if (remaining < sizeof(fs_image_entry_t)) {
            return FS_ERR_INVALID;
        }
        const fs_image_entry_t *entry = (const fs_image_entry_t *)cursor;
        cursor += sizeof(fs_image_entry_t);
        remaining -= sizeof(fs_image_entry_t);

        if (entry->path_len == 0 || entry->path_len >= FS_MAX_PATH_LEN) {
            return FS_ERR_INVALID;
        }
        if (remaining < entry->path_len) {
            return FS_ERR_INVALID;
        }

        memcpy(path, cursor, entry->path_len);
        path[entry->path_len] = '\0';
        cursor += entry->path_len;
        remaining -= entry->path_len;

        if (remaining < entry->data_len) {
            return FS_ERR_INVALID;
        }

        const uint8_t *data = cursor;
        cursor += entry->data_len;
        remaining -= entry->data_len;

        if (entry->type == FS_NODE_DIRECTORY) {
            fs_status_t status = fs_mkdir(path);
            if (status != FS_OK && status != FS_ERR_EXIST) {
                return status;
            }
        } else {
            fs_status_t status = fs_create_file(path);
            if (status != FS_OK && status != FS_ERR_EXIST) {
                return status;
            }
            status = fs_write_file(path, data, entry->data_len);
            if (status != FS_OK) {
                return status;
            }
        }
    }

    return FS_OK;
}

fs_status_t fs_save(void) {
    if (!ata_is_available()) {
        return FS_ERR_INVALID;
    }

    size_t serialized_size = 0;
    fs_status_t status = fs_serialize_to_buffer(&serialized_size);
    if (status != FS_OK) {
        return status;
    }

    if (serialized_size == 0 || serialized_size > FS_IMAGE_BUFFER_SIZE) {
        return FS_ERR_INVALID;
    }

    uint16_t sectors = (uint16_t)(serialized_size / FS_IMAGE_SECTOR_SIZE);
    if (sectors == 0 || sectors > FS_IMAGE_LBA_COUNT) {
        return FS_ERR_INVALID;
    }

    if (ata_write_sectors(FS_IMAGE_LBA_START, sectors, fs_image_buffer) != 0) {
        return FS_ERR_INVALID;
    }

    return FS_OK;
}

fs_status_t fs_load(void) {
    if (!ata_is_available()) {
        return FS_ERR_INVALID;
    }

    if (!fs_image_buffer) {
        return FS_ERR_NOMEM;
    }

    if (ata_read_sectors(FS_IMAGE_LBA_START, FS_IMAGE_LBA_COUNT, fs_image_buffer) != 0) {
        return FS_ERR_INVALID;
    }

    fs_image_header_t header;
    memcpy(&header, fs_image_buffer, sizeof(header));

    if (header.magic != FS_IMAGE_MAGIC || header.version != FS_IMAGE_VERSION) {
        return FS_ERR_INVALID;
    }

    if (header.total_size < sizeof(fs_image_header_t) || header.total_size > FS_IMAGE_BUFFER_SIZE) {
        return FS_ERR_INVALID;
    }

    if (header.entry_count == 0) {
        fs_clear_children(fs_root);
        fs_cwd = fs_root;
        return FS_OK;
    }

    return fs_deserialize_from_buffer(header.total_size, header.entry_count);
}

int fs_persistence_available(void) {
    return ata_is_available();
}


