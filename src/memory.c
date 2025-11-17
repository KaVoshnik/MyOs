#include <memory.h>

static uintptr_t heap_base = 0;
static uintptr_t heap_next = 0;
static uintptr_t heap_end = 0;

void memory_init(uintptr_t heap_start, size_t heap_size) {
    heap_base = heap_start;
    heap_next = heap_start;
    heap_end = heap_start + heap_size;
}

static uintptr_t align_up(uintptr_t value, size_t alignment) {
    if (alignment <= 1) {
        return value;
    }
    uintptr_t mask = alignment - 1;
    return (value + mask) & ~mask;
}

void *kmalloc_aligned(size_t size, size_t alignment) {
    heap_next = align_up(heap_next, alignment);
    if (heap_next + size > heap_end) {
        return NULL;
    }
    void *ptr = (void *)heap_next;
    heap_next += size;
    return ptr;
}

void *kmalloc(size_t size) {
    return kmalloc_aligned(size, sizeof(void *));
}

void kfree(void *ptr) {
    (void)ptr; /* simple bump allocator cannot free */
}

size_t memory_bytes_used(void) {
    return (size_t)(heap_next - heap_base);
}

