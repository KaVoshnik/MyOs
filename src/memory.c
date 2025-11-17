#include <memory.h>
#include <string.h>

#define ALIGNMENT 8
#define MIN_BLOCK_SIZE 16

typedef struct block_header {
    size_t size;
    struct block_header *next;
    int free;
} block_header_t;

static block_header_t *heap_start = NULL;
static block_header_t *free_list = NULL;
static size_t heap_size = 0;
static size_t bytes_used = 0;

static size_t align_size(size_t size) {
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

static block_header_t *find_free_block(size_t size) {
    block_header_t *current = free_list;
    while (current != NULL) {
        if (current->free && current->size >= size) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static block_header_t *request_space(size_t size) {
    block_header_t *block;
    
    if (heap_start == NULL) {
        return NULL;
    }
    
    block_header_t *last = heap_start;
    while (last->next != NULL) {
        last = last->next;
    }
    
    uintptr_t next_addr = (uintptr_t)last + sizeof(block_header_t) + last->size;
    if (next_addr + sizeof(block_header_t) + size > (uintptr_t)heap_start + heap_size) {
        return NULL; /* Out of memory */
    }
    
    block = (block_header_t *)next_addr;
    block->size = size;
    block->free = 0;
    block->next = NULL;
    last->next = block;
    
    bytes_used += size;
    return block;
}

static void split_block(block_header_t *block, size_t size) {
    if (block->size - size >= sizeof(block_header_t) + MIN_BLOCK_SIZE) {
        block_header_t *new_block = (block_header_t *)((uintptr_t)block + sizeof(block_header_t) + size);
        new_block->size = block->size - size - sizeof(block_header_t);
        new_block->free = 1;
        new_block->next = block->next;
        block->next = new_block;
        block->size = size;
        
        /* Add to free list */
        new_block->next = free_list;
        free_list = new_block;
    }
}

void memory_init(uintptr_t heap_start_addr, size_t size) {
    heap_start = (block_header_t *)heap_start_addr;
    heap_size = size;
    bytes_used = 0;
    free_list = NULL;
    
    /* Initialize first block as free */
    heap_start->size = size - sizeof(block_header_t);
    heap_start->free = 1;
    heap_start->next = NULL;
    free_list = heap_start;
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
    
    if (block == NULL) {
        block = request_space(size);
        if (block == NULL) {
            return NULL; /* Out of memory */
        }
    } else {
        /* Remove from free list */
        if (free_list == block) {
            free_list = block->next;
        } else {
            block_header_t *prev = free_list;
            while (prev != NULL && prev->next != block) {
                prev = prev->next;
            }
            if (prev != NULL) {
                prev->next = block->next;
            }
        }
        block->free = 0;
        bytes_used += block->size;
        split_block(block, size);
    }
    
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
    
    /* Align the pointer */
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
    
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
    
    /* Add to free list */
    block->next = free_list;
    free_list = block;
    
    /* Try to merge with next block */
    if (block->next != NULL && (uintptr_t)block->next == (uintptr_t)block + sizeof(block_header_t) + block->size) {
        if (block->next->free) {
            block->size += sizeof(block_header_t) + block->next->size;
            block->next = block->next->next;
        }
    }
    
    /* Try to merge with previous block */
    block_header_t *current = heap_start;
    while (current != NULL && current->next != block) {
        current = current->next;
    }
    if (current != NULL && current->free && 
        (uintptr_t)block == (uintptr_t)current + sizeof(block_header_t) + current->size) {
        current->size += sizeof(block_header_t) + block->size;
        current->next = block->next;
        
        /* Remove block from free list */
        if (free_list == block) {
            free_list = block->next;
        } else {
            block_header_t *prev = free_list;
            while (prev != NULL && prev->next != block) {
                prev = prev->next;
            }
            if (prev != NULL) {
                prev->next = block->next;
            }
        }
    }
}

size_t memory_bytes_used(void) {
    return bytes_used;
}
