#include <process.h>
#include <memory.h>
#include <string.h>
#include <thread.h>
#include <pit.h>
#include <interrupts.h>

static process_t process_table[PROCESS_MAX_COUNT];
static uint64_t next_pid = 1;
static process_t *current_process = NULL;
static process_t *init_process = NULL;

static process_t *process_alloc_slot(void);
static process_t *process_find(uint64_t pid);
static void process_detach_from_parent(process_t *proc);
static void process_attach_to_parent(process_t *parent, process_t *child);
static void process_cleanup_zombies(void);

static int interrupts_save_and_disable(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; pop %0" : "=r"(flags));
    interrupts_disable();
    return (flags & (1ull << 9)) != 0;
}

static void interrupts_restore_state(int enabled) {
    if (enabled) {
        interrupts_enable();
    }
}

void process_system_init(void) {
    memset(process_table, 0, sizeof(process_table));
    
    /* Create init process (PID 1) */
    process_t *init = process_alloc_slot();
    if (init) {
        init->pid = 1;
        init->ppid = 0;
        init->state = PROCESS_RUNNING;
        init->main_thread_id = thread_current_id();
        init->thread_count = 1;
        strncpy(init->name, "init", PROCESS_NAME_MAX - 1);
        init->name[PROCESS_NAME_MAX - 1] = '\0';
        init->start_time = pit_seconds();
        init->parent = NULL;
        init->children = NULL;
        init->next_sibling = NULL;
        init->waiting = NULL;
        current_process = init;
        init_process = init;
        next_pid = 2;
    }
}

static process_t *process_alloc_slot(void) {
    for (size_t i = 0; i < PROCESS_MAX_COUNT; ++i) {
        if (process_table[i].state == PROCESS_UNUSED) {
            memset(&process_table[i], 0, sizeof(process_t));
            return &process_table[i];
        }
    }
    return NULL;
}

static process_t *process_find(uint64_t pid) {
    if (pid == 0) {
        return NULL;
    }
    for (size_t i = 0; i < PROCESS_MAX_COUNT; ++i) {
        if (process_table[i].state != PROCESS_UNUSED && process_table[i].pid == pid) {
            return &process_table[i];
        }
    }
    return NULL;
}

static void process_detach_from_parent(process_t *proc) {
    if (!proc || !proc->parent) {
        return;
    }
    
    process_t **cursor = &proc->parent->children;
    while (*cursor && *cursor != proc) {
        cursor = &(*cursor)->next_sibling;
    }
    if (*cursor == proc) {
        *cursor = proc->next_sibling;
    }
    proc->parent = NULL;
    proc->next_sibling = NULL;
}

static void process_attach_to_parent(process_t *parent, process_t *child) {
    if (!parent || !child) {
        return;
    }
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
}

uint64_t process_create(const char *name, thread_entry_t entry, void *arg, size_t stack_size) {
    if (!entry) {
        return 0;
    }
    
    int was_enabled = interrupts_save_and_disable();
    process_t *proc = process_alloc_slot();
    if (!proc) {
        interrupts_restore_state(was_enabled);
        return 0;
    }
    
    uint64_t thread_id = thread_create(name, entry, arg, stack_size);
    if (thread_id == 0) {
        proc->state = PROCESS_UNUSED;
        interrupts_restore_state(was_enabled);
        return 0;
    }
    
    proc->pid = next_pid++;
    proc->ppid = current_process ? current_process->pid : 1;
    proc->state = PROCESS_RUNNING;
    proc->main_thread_id = thread_id;
    proc->thread_count = 1;
    proc->exit_status = 0;
    proc->start_time = pit_seconds();
    proc->cpu_time = 0;
    
    if (!name || name[0] == '\0') {
        strncpy(proc->name, "process", PROCESS_NAME_MAX - 1);
    } else {
        strncpy(proc->name, name, PROCESS_NAME_MAX - 1);
    }
    proc->name[PROCESS_NAME_MAX - 1] = '\0';
    
    proc->parent = current_process;
    proc->children = NULL;
    proc->next_sibling = NULL;
    proc->waiting = NULL;
    
    if (current_process) {
        process_attach_to_parent(current_process, proc);
    }
    
    interrupts_restore_state(was_enabled);
    return proc->pid;
}

uint64_t process_fork(void) {
    if (!current_process) {
        return 0;
    }
    
    int was_enabled = interrupts_save_and_disable();
    
    /* For now, fork creates a new process with the same entry point */
    /* In a full implementation, this would copy the address space */
    process_t *child = process_alloc_slot();
    if (!child) {
        interrupts_restore_state(was_enabled);
        return 0;
    }
    
    child->pid = next_pid++;
    child->ppid = current_process->pid;
    child->state = PROCESS_RUNNING;
    child->main_thread_id = 0; /* Will be set when thread is created */
    child->thread_count = 0;
    child->exit_status = 0;
    child->start_time = pit_seconds();
    child->cpu_time = 0;
    
    /* Copy process name */
    strncpy(child->name, current_process->name, PROCESS_NAME_MAX - 1);
    child->name[PROCESS_NAME_MAX - 1] = '\0';
    
    child->parent = current_process;
    child->children = NULL;
    child->next_sibling = NULL;
    child->waiting = NULL;
    
    process_attach_to_parent(current_process, child);
    
    interrupts_restore_state(was_enabled);
    return child->pid;
}

int process_exec(const char *name, thread_entry_t entry, void *arg) {
    if (!current_process || !entry) {
        return -1;
    }
    
    int was_enabled = interrupts_save_and_disable();
    
    /* Update process name and entry point */
    if (name && name[0] != '\0') {
        strncpy(current_process->name, name, PROCESS_NAME_MAX - 1);
        current_process->name[PROCESS_NAME_MAX - 1] = '\0';
    }
    
    /* In a full implementation, this would load a new program */
    /* For now, we just update the name */
    
    interrupts_restore_state(was_enabled);
    return 0;
}

void process_exit(int status) {
    int was_enabled = interrupts_save_and_disable();
    process_t *self = current_process;
    if (!self) {
        interrupts_restore_state(was_enabled);
        thread_exit(status);
        return;
    }
    
    self->exit_status = status;
    self->state = PROCESS_ZOMBIE;
    
    /* Wake up parent if waiting */
    if (self->parent && self->parent->waiting == self) {
        self->parent->waiting = NULL;
        /* In full implementation, would wake parent */
    }
    
    /* Detach from parent */
    process_detach_from_parent(self);
    
    /* Make children orphans (adopt by init) */
    process_t *child = self->children;
    while (child) {
        process_t *next = child->next_sibling;
        child->parent = init_process;
        child->ppid = 1;
        if (init_process) {
            process_attach_to_parent(init_process, child);
        }
        child = next;
    }
    self->children = NULL;
    
    interrupts_restore_state(was_enabled);
    thread_exit(status);
}

int process_wait(uint64_t pid, int *exit_status) {
    if (!current_process) {
        return -1;
    }
    
    int was_enabled = interrupts_save_and_disable();
    process_t *target = NULL;
    
    if (pid == 0) {
        /* Wait for any child */
        target = current_process->children;
        while (target && target->state == PROCESS_ZOMBIE) {
            target = target->next_sibling;
        }
        if (!target) {
            interrupts_restore_state(was_enabled);
            return -1; /* No children to wait for */
        }
    } else {
        target = process_find(pid);
        if (!target) {
            interrupts_restore_state(was_enabled);
            return -1;
        }
        if (target->parent != current_process) {
            interrupts_restore_state(was_enabled);
            return -2; /* Not a child of current process */
        }
    }
    
    /* Wait for zombie */
    while (target && target->state != PROCESS_ZOMBIE) {
        target->waiting = current_process;
        current_process->state = PROCESS_SLEEPING;
        interrupts_restore_state(was_enabled);
        thread_yield();
        was_enabled = interrupts_save_and_disable();
        if (pid == 0) {
            target = current_process->children;
            while (target && target->state == PROCESS_ZOMBIE) {
                target = target->next_sibling;
            }
        } else {
            target = process_find(pid);
        }
        if (!target) {
            interrupts_restore_state(was_enabled);
            return -1;
        }
    }
    
    if (!target) {
        interrupts_restore_state(was_enabled);
        return -1;
    }
    
    if (exit_status) {
        *exit_status = target->exit_status;
    }
    
    /* Clean up zombie */
    uint64_t dead_pid = target->pid;
    process_detach_from_parent(target);
    target->state = PROCESS_UNUSED;
    interrupts_restore_state(was_enabled);
    return (int)dead_pid;
}

int process_kill(uint64_t pid) {
    if (pid == 0 || pid == 1) {
        return -1; /* Cannot kill PID 0 or init */
    }
    
    int was_enabled = interrupts_save_and_disable();
    process_t *target = process_find(pid);
    
    if (!target) {
        interrupts_restore_state(was_enabled);
        return -1;
    }
    
    if (target == current_process) {
        interrupts_restore_state(was_enabled);
        return -2; /* Cannot kill self */
    }
    
    if (target->state == PROCESS_UNUSED || target->state == PROCESS_ZOMBIE) {
        interrupts_restore_state(was_enabled);
        return -3; /* Already dead */
    }
    
    /* Kill all threads in process */
    if (target->main_thread_id != 0) {
        thread_kill(target->main_thread_id);
    }
    
    target->state = PROCESS_ZOMBIE;
    target->exit_status = -1; /* Killed */
    
    /* Wake up parent if waiting */
    if (target->parent && target->parent->waiting == target) {
        target->parent->waiting = NULL;
    }
    
    interrupts_restore_state(was_enabled);
    return 0;
}

uint64_t process_current_pid(void) {
    if (!current_process) {
        return 0;
    }
    return current_process->pid;
}

uint64_t process_get_ppid(uint64_t pid) {
    if (pid == 0) {
        return current_process ? current_process->ppid : 0;
    }
    process_t *proc = process_find(pid);
    return proc ? proc->ppid : 0;
}

process_state_t process_get_state(uint64_t pid) {
    if (pid == 0) {
        return current_process ? current_process->state : PROCESS_UNUSED;
    }
    process_t *proc = process_find(pid);
    return proc ? proc->state : PROCESS_UNUSED;
}

const char *process_get_name(uint64_t pid) {
    if (pid == 0) {
        return current_process ? current_process->name : NULL;
    }
    process_t *proc = process_find(pid);
    return proc ? proc->name : NULL;
}

const char *process_state_name(process_state_t state) {
    static const char *names[] = {
        "unused",
        "running",
        "sleeping",
        "zombie",
        "stopped"
    };
    if ((size_t)state >= sizeof(names) / sizeof(names[0])) {
        return "unknown";
    }
    return names[state];
}

size_t process_snapshot_list(process_snapshot_t *buffer, size_t capacity) {
    if (!buffer || capacity == 0) {
        return 0;
    }
    
    size_t count = 0;
    int was_enabled = interrupts_save_and_disable();
    for (size_t i = 0; i < PROCESS_MAX_COUNT && count < capacity; ++i) {
        if (process_table[i].state == PROCESS_UNUSED) {
            continue;
        }
        buffer[count].pid = process_table[i].pid;
        buffer[count].ppid = process_table[i].ppid;
        buffer[count].state = process_table[i].state;
        buffer[count].name = process_table[i].name;
        buffer[count].cpu_time = process_table[i].cpu_time;
        ++count;
    }
    interrupts_restore_state(was_enabled);
    return count;
}

