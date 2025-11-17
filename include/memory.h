#ifndef _MYOS_MEMORY_H
#define _MYOS_MEMORY_H

#include <stddef.h>
#include <stdint.h>

void memory_init(uintptr_t heap_start, size_t heap_size);
void *kmalloc(size_t size);
void *kmalloc_aligned(size_t size, size_t alignment);
void kfree(void *ptr);
size_t memory_bytes_used(void);

#endif /* _MYOS_MEMORY_H */

