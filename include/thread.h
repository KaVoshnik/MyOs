#ifndef _MYOS_THREAD_H
#define _MYOS_THREAD_H

#include <stdint.h>
#include <stddef.h>

struct interrupt_frame;

typedef void (*thread_entry_t)(void *arg);

typedef enum thread_state {
    THREAD_UNUSED = 0,
    THREAD_NEW,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_ZOMBIE
} thread_state_t;

typedef struct thread_context {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rsp;
    uint64_t rip;
    uint64_t rflags;
} thread_context_t;

typedef struct irq_regs {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rax;
} irq_regs_t;

typedef struct thread_snapshot {
    uint64_t id;
    thread_state_t state;
    const char *name;
    uint64_t run_ticks;
    uint64_t quantum_ticks;
} thread_snapshot_t;

void thread_system_init(void);
uint64_t thread_create(const char *name, thread_entry_t entry, void *arg, size_t stack_size);
void thread_exit(int status);
void thread_yield(void);
int thread_join(uint64_t id, int *exit_status);
uint64_t thread_current_id(void);
const char *thread_state_name(thread_state_t state);
size_t thread_snapshot_list(thread_snapshot_t *buffer, size_t capacity);
void thread_block_current(thread_state_t new_state);
void thread_unblock_by_id(uint64_t id);
void thread_handle_timer_interrupt(struct interrupt_frame *frame, irq_regs_t *regs);

#endif /* _MYOS_THREAD_H */
