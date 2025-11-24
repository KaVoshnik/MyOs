#include <memory.h>
#include <string.h>

#define ALIGNMENT 8
#define MIN_BLOCK_SIZE 16

typedef struct block_header {
    size_t size;
    struct block_header *next;
    struct block_header *prev;
    int free;
} block_header_t;

static block_header_t *heap_start = NULL;
static size_t heap_size = 0;
static size_t bytes_used = 0;

static size_t align_size(size_t size) {
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

static block_header_t *find_free_block(size_t size) {
    block_header_t *current = heap_start;
    while (current) {
        if (current->free && current->size >= size) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static void split_block(block_header_t *block, size_t size) {
    if (block->size - size < sizeof(block_header_t) + MIN_BLOCK_SIZE) {
        return;
    }

    block_header_t *new_block = (block_header_t *)((uintptr_t)block + sizeof(block_header_t) + size);
    new_block->size = block->size - size - sizeof(block_header_t);
    new_block->free = 1;
    new_block->prev = block;
    new_block->next = block->next;
    if (new_block->next) {
        new_block->next->prev = new_block;
    }

    block->size = size;
    block->next = new_block;
}

static void coalesce(block_header_t *block) {
    if (block->next && block->next->free &&
        ((uintptr_t)block->next == (uintptr_t)block + sizeof(block_header_t) + block->size)) {
        block_header_t *next = block->next;
        block->size += sizeof(block_header_t) + next->size;
        block->next = next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }

    if (block->prev && block->prev->free &&
        ((uintptr_t)block == (uintptr_t)block->prev + sizeof(block_header_t) + block->prev->size)) {
        block_header_t *prev = block->prev;
        prev->size += sizeof(block_header_t) + block->size;
        prev->next = block->next;
        if (block->next) {
            block->next->prev = prev;
        }
        block = prev;
    }
}

void memory_init(uintptr_t heap_start_addr, size_t size) {
    heap_start = (block_header_t *)heap_start_addr;
    heap_size = size;
    bytes_used = 0;
    
    heap_start->size = size - sizeof(block_header_t);
    heap_start->free = 1;
    heap_start->next = NULL;
    heap_start->prev = NULL;
}

void *kmalloc(size_t size) {
    if (size == 0 || heap_start == NULL) {
        return NULL;
    }
    
    size = align_size(size);
    if (size < MIN_BLOCK_SIZE) {
        size = MIN_BLOCK_SIZE;
    }
    
    block_header_t *block = find_free_block(size);
    if (!block) {
        return NULL;
    }

    split_block(block, size);
    block->free = 0;
    bytes_used += block->size;

    return (void *)((uintptr_t)block + sizeof(block_header_t));
}

void *kmalloc_aligned(size_t size, size_t alignment) {
    if (size == 0 || heap_start == NULL) {
        return NULL;
    }
    
    /* Ensure alignment is at least ALIGNMENT and is power of 2 */
    if (alignment < ALIGNMENT) {
        alignment = ALIGNMENT;
    }
    if ((alignment & (alignment - 1)) != 0) {
        return NULL; /* Alignment must be power of 2 */
    }
    
    /* Allocate extra space to ensure we can align */
    size_t total_size = size + alignment + sizeof(uintptr_t);
    void *ptr = kmalloc(total_size);
    if (ptr == NULL) {
        return NULL;
    }
    
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t adjusted = addr + sizeof(uintptr_t);
    uintptr_t aligned_addr = (adjusted + alignment - 1) & ~(alignment - 1);
    
    /* Store the original block header pointer before the aligned address */
    block_header_t *original_block = (block_header_t *)(addr - sizeof(block_header_t));
    *((uintptr_t *)(aligned_addr - sizeof(uintptr_t))) = (uintptr_t)original_block;
    
    return (void *)aligned_addr;
}

void kfree(void *ptr) {
    if (ptr == NULL || heap_start == NULL) {
        return;
    }
    
    block_header_t *block = NULL;
    
    /* First, try regular allocation (block header right before ptr) */
    block_header_t *regular_block = (block_header_t *)((uintptr_t)ptr - sizeof(block_header_t));
    
    /* Check if this is a valid regular allocation */
    if (regular_block >= heap_start && 
        (uintptr_t)regular_block < (uintptr_t)heap_start + heap_size &&
        (uintptr_t)ptr == (uintptr_t)regular_block + sizeof(block_header_t) &&
        !regular_block->free &&
        (uintptr_t)ptr < (uintptr_t)regular_block + sizeof(block_header_t) + regular_block->size) {
        block = regular_block;
    } else {
        /* Not a regular allocation, check for aligned allocation */
        if ((uintptr_t)ptr >= sizeof(uintptr_t)) {
            uintptr_t stored_ptr = *((uintptr_t *)((uintptr_t)ptr - sizeof(uintptr_t)));
            block_header_t *aligned_block = (block_header_t *)stored_ptr;
            
            if (aligned_block >= heap_start && 
                (uintptr_t)aligned_block < (uintptr_t)heap_start + heap_size &&
                (uintptr_t)ptr >= (uintptr_t)aligned_block + sizeof(block_header_t) &&
                (uintptr_t)ptr < (uintptr_t)aligned_block + sizeof(block_header_t) + aligned_block->size &&
                !aligned_block->free) {
                block = aligned_block;
            }
        }
    }
    
    if (block == NULL) {
        return; /* Invalid pointer */
    }
    
    if (block->free) {
        return; /* Already free */
    }
    
    bytes_used -= block->size;
    block->free = 1;
    
    coalesce(block);
}

size_t memory_bytes_used(void) {
    return bytes_used;
}

size_t memory_heap_size(void) {
    return heap_size;
}

void *calloc(size_t num, size_t size) {
    if (num == 0 || size == 0) {
        return NULL;
    }
    
    /* Check for overflow */
    size_t total = num * size;
    if (total / num != size) {
        return NULL; /* Overflow */
    }
    
    void *ptr = kmalloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *realloc(void *ptr, size_t new_size) {
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }
    
    if (ptr == NULL) {
        return kmalloc(new_size);
    }
    
    /* Get the original block */
    block_header_t *block = NULL;
    block_header_t *regular_block = (block_header_t *)((uintptr_t)ptr - sizeof(block_header_t));
    
    if (regular_block >= heap_start && 
        (uintptr_t)regular_block < (uintptr_t)heap_start + heap_size &&
        (uintptr_t)ptr == (uintptr_t)regular_block + sizeof(block_header_t) &&
        !regular_block->free &&
        (uintptr_t)ptr < (uintptr_t)regular_block + sizeof(block_header_t) + regular_block->size) {
        block = regular_block;
    } else {
        if ((uintptr_t)ptr >= sizeof(uintptr_t)) {
            uintptr_t stored_ptr = *((uintptr_t *)((uintptr_t)ptr - sizeof(uintptr_t)));
            block_header_t *aligned_block = (block_header_t *)stored_ptr;
            
            if (aligned_block >= heap_start && 
                (uintptr_t)aligned_block < (uintptr_t)heap_start + heap_size &&
                (uintptr_t)ptr >= (uintptr_t)aligned_block + sizeof(block_header_t) &&
                (uintptr_t)ptr < (uintptr_t)aligned_block + sizeof(block_header_t) + aligned_block->size &&
                !aligned_block->free) {
                block = aligned_block;
            }
        }
    }
    
    if (block == NULL) {
        return NULL; /* Invalid pointer */
    }
    
    size_t old_size = block->size;
    size_t aligned_new_size = align_size(new_size);
    if (aligned_new_size < MIN_BLOCK_SIZE) {
        aligned_new_size = MIN_BLOCK_SIZE;
    }
    
    /* If new size is smaller or equal, just return the same pointer */
    if (aligned_new_size <= old_size) {
        return ptr;
    }
    
    /* Try to expand in place if next block is free and large enough */
    if (block->next && block->next->free) {
        size_t available = block->next->size + sizeof(block_header_t);
        if (old_size + available >= aligned_new_size) {
            block_header_t *next = block->next;
            block->size = old_size + sizeof(block_header_t) + next->size;
            block->next = next->next;
            if (block->next) {
                block->next->prev = block;
            }
            split_block(block, aligned_new_size);
            return ptr;
        }
    }
    
    /* Need to allocate new block and copy */
    void *new_ptr = kmalloc(new_size);
    if (new_ptr == NULL) {
        return NULL;
    }
    
    size_t copy_size = (old_size < new_size) ? old_size : new_size;
    memcpy(new_ptr, ptr, copy_size);
    kfree(ptr);
    
    return new_ptr;
}

size_t memory_blocks_count(void) {
    size_t count = 0;
    block_header_t *current = heap_start;
    while (current) {
        ++count;
        current = current->next;
    }
    return count;
}

size_t memory_free_blocks_count(void) {
    size_t count = 0;
    block_header_t *current = heap_start;
    while (current) {
        if (current->free) {
            ++count;
        }
        current = current->next;
    }
    return count;
}

size_t memory_largest_free_block(void) {
    size_t largest = 0;
    block_header_t *current = heap_start;
    while (current) {
        if (current->free && current->size > largest) {
            largest = current->size;
        }
        current = current->next;
    }
    return largest;
}
