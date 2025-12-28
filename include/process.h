#ifndef _MYOS_PROCESS_H
#define _MYOS_PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include <thread.h>

#define PROCESS_MAX_COUNT 64
#define PROCESS_NAME_MAX 64

typedef enum process_state {
    PROCESS_UNUSED = 0,
    PROCESS_RUNNING,
    PROCESS_SLEEPING,
    PROCESS_ZOMBIE,
    PROCESS_STOPPED
} process_state_t;

typedef struct process {
    uint64_t pid;                    /* Process ID */
    uint64_t ppid;                   /* Parent Process ID */
    process_state_t state;
    int exit_status;
    char name[PROCESS_NAME_MAX];
    
    /* Process memory space */
    void *heap_start;
    size_t heap_size;
    
    /* Thread management */
    uint64_t main_thread_id;
    uint64_t thread_count;
    
    /* Process tree */
    struct process *parent;
    struct process *children;
    struct process *next_sibling;
    
    /* Waiting processes */
    struct process *waiting;
    
    /* Statistics */
    uint64_t start_time;
    uint64_t cpu_time;
} process_t;

/* Process management */
void process_system_init(void);
uint64_t process_create(const char *name, thread_entry_t entry, void *arg, size_t stack_size);
uint64_t process_fork(void);
int process_exec(const char *name, thread_entry_t entry, void *arg);
void process_exit(int status);
int process_wait(uint64_t pid, int *exit_status);
int process_kill(uint64_t pid);

/* Process information */
uint64_t process_current_pid(void);
uint64_t process_get_ppid(uint64_t pid);
process_state_t process_get_state(uint64_t pid);
const char *process_get_name(uint64_t pid);
const char *process_state_name(process_state_t state);

/* Process snapshot for ps command */
typedef struct process_snapshot {
    uint64_t pid;
    uint64_t ppid;
    process_state_t state;
    const char *name;
    uint64_t cpu_time;
} process_snapshot_t;

size_t process_snapshot_list(process_snapshot_t *buffer, size_t capacity);

#endif /* _MYOS_PROCESS_H */

