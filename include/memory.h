#ifndef _MYOS_MEMORY_H
#define _MYOS_MEMORY_H

#include <stddef.h>
#include <stdint.h>

void memory_init(uintptr_t heap_start, size_t heap_size);
void *kmalloc(size_t size);
void *kmalloc_aligned(size_t size, size_t alignment);
void kfree(void *ptr);
void *calloc(size_t num, size_t size);
void *realloc(void *ptr, size_t new_size);
size_t memory_bytes_used(void);
size_t memory_heap_size(void);
size_t memory_blocks_count(void);
size_t memory_free_blocks_count(void);
size_t memory_largest_free_block(void);

#endif /* _MYOS_MEMORY_H */


