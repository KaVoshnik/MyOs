#ifndef _MYOS_THREAD_H
#define _MYOS_THREAD_H

#include <stdint.h>
#include <stddef.h>

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

typedef struct thread_snapshot {
    uint64_t id;
    thread_state_t state;
    const char *name;
} thread_snapshot_t;

void thread_system_init(void);
uint64_t thread_create(const char *name, thread_entry_t entry, void *arg, size_t stack_size);
void thread_exit(int status);
void thread_yield(void);
int thread_join(uint64_t id, int *exit_status);
int thread_kill(uint64_t id);
uint64_t thread_current_id(void);
const char *thread_state_name(thread_state_t state);
size_t thread_snapshot_list(thread_snapshot_t *buffer, size_t capacity);

#endif /* _MYOS_THREAD_H */


