#include <thread.h>
#include <memory.h>
#include <string.h>
#include <terminal.h>
#include <interrupts.h>

#define THREAD_MAX_COUNT 32
#define THREAD_STACK_DEFAULT 4096
#define THREAD_MIN_STACK 2048
#define THREAD_NAME_MAX 32

extern void thread_context_switch(thread_context_t *old_ctx, thread_context_t *new_ctx);

typedef struct thread {
    uint64_t id;
    thread_state_t state;
    thread_context_t context;
    void *stack;
    size_t stack_size;
    thread_entry_t entry;
    void *arg;
    int exit_status;
    struct thread *waiting;
    struct thread *next;
    struct thread *prev;
    char name[THREAD_NAME_MAX];
} thread_t;

static thread_t thread_table[THREAD_MAX_COUNT];
static thread_t *current_thread = NULL;
static thread_t *ready_head = NULL;
static thread_t *ready_tail = NULL;
static uint64_t next_thread_id = 1;

static void thread_enqueue(thread_t *thread);
static thread_t *thread_dequeue(void);
static thread_t *thread_find(uint64_t id);
static thread_t *thread_alloc_slot(void);
static void thread_destroy(thread_t *thread);
static void scheduler_switch(int requeue_current);
static void thread_trampoline(void);
static void idle_thread(void *arg);
static int interrupts_save_and_disable(void);
static void interrupts_restore_state(int enabled);

static const char *state_names[] = {
    "unused",
    "new",
    "ready",
    "running",
    "blocked",
    "zombie"
};

void thread_system_init(void) {
    int was_enabled = interrupts_save_and_disable();

    for (size_t i = 0; i < THREAD_MAX_COUNT; ++i) {
        thread_table[i].state = THREAD_UNUSED;
        thread_table[i].stack = NULL;
        thread_table[i].stack_size = 0;
        thread_table[i].waiting = NULL;
        thread_table[i].next = NULL;
        thread_table[i].prev = NULL;
        thread_table[i].name[0] = '\0';
    }

    thread_t *bootstrap = &thread_table[0];
    bootstrap->id = next_thread_id++;
    bootstrap->state = THREAD_RUNNING;
    bootstrap->entry = NULL;
    bootstrap->arg = NULL;
    strncpy(bootstrap->name, "bootstrap", THREAD_NAME_MAX - 1);
    current_thread = bootstrap;

    uint64_t idle_id = thread_create("idle", idle_thread, NULL, THREAD_MIN_STACK);
    (void)idle_id;

    interrupts_restore_state(was_enabled);
}

uint64_t thread_current_id(void) {
    if (!current_thread) {
        return 0;
    }
    return current_thread->id;
}

uint64_t thread_create(const char *name, thread_entry_t entry, void *arg, size_t stack_size) {
    if (!entry) {
        return 0;
    }

    if (stack_size < THREAD_MIN_STACK) {
        stack_size = THREAD_STACK_DEFAULT;
    }

    int was_enabled = interrupts_save_and_disable();
    thread_t *slot = thread_alloc_slot();
    if (!slot) {
        interrupts_restore_state(was_enabled);
        return 0;
    }

    void *stack = kmalloc(stack_size);
    if (!stack) {
        slot->state = THREAD_UNUSED;
        interrupts_restore_state(was_enabled);
        return 0;
    }

    memset(stack, 0, stack_size);
    slot->stack = stack;
    slot->stack_size = stack_size;
    slot->id = next_thread_id++;
    slot->entry = entry;
    slot->arg = arg;
    slot->exit_status = 0;
    slot->waiting = NULL;
    slot->state = THREAD_READY;
    slot->next = NULL;
    slot->prev = NULL;

    if (!name || name[0] == '\0') {
        strncpy(slot->name, "thread", THREAD_NAME_MAX - 1);
    } else {
        strncpy(slot->name, name, THREAD_NAME_MAX - 1);
    }
    slot->name[THREAD_NAME_MAX - 1] = '\0';

    uintptr_t stack_top = (uintptr_t)stack + stack_size;
    stack_top &= ~(uintptr_t)0xFUL;

    memset(&slot->context, 0, sizeof(slot->context));
    slot->context.rsp = stack_top;
    slot->context.rip = (uint64_t)thread_trampoline;
    slot->context.rflags = 0x202;

    thread_enqueue(slot);
    interrupts_restore_state(was_enabled);
    return slot->id;
}

void thread_exit(int status) {
    int was_enabled = interrupts_save_and_disable();
    thread_t *self = current_thread;
    if (!self) {
        interrupts_restore_state(was_enabled);
        return;
    }

    self->exit_status = status;
    self->state = THREAD_ZOMBIE;

    if (self->waiting && self->waiting->state == THREAD_BLOCKED) {
        self->waiting->state = THREAD_READY;
        thread_enqueue(self->waiting);
        self->waiting = NULL;
    }

    scheduler_switch(0);
    interrupts_restore_state(was_enabled);

    for (;;) {
        __asm__ volatile("hlt");
    }
}

void thread_yield(void) {
    int was_enabled = interrupts_save_and_disable();
    scheduler_switch(1);
    interrupts_restore_state(was_enabled);
}

int thread_join(uint64_t id, int *exit_status) {
    int was_enabled = interrupts_save_and_disable();
    thread_t *target = thread_find(id);
    if (!target) {
        interrupts_restore_state(was_enabled);
        return -1;
    }

    if (target == current_thread) {
        interrupts_restore_state(was_enabled);
        return -2;
    }

    while (target->state != THREAD_ZOMBIE) {
        if (target->waiting && target->waiting != current_thread) {
            interrupts_restore_state(was_enabled);
            return -3;
        }
        target->waiting = current_thread;
        current_thread->state = THREAD_BLOCKED;
        scheduler_switch(0);
        was_enabled = interrupts_save_and_disable();
    }

    if (exit_status) {
        *exit_status = target->exit_status;
    }

    thread_destroy(target);
    interrupts_restore_state(was_enabled);
    return 0;
}

const char *thread_state_name(thread_state_t state) {
    if ((size_t)state >= (sizeof(state_names) / sizeof(state_names[0]))) {
        return "unknown";
    }
    return state_names[state];
}

size_t thread_snapshot_list(thread_snapshot_t *buffer, size_t capacity) {
    if (!buffer || capacity == 0) {
        return 0;
    }

    size_t count = 0;
    int was_enabled = interrupts_save_and_disable();
    for (size_t i = 0; i < THREAD_MAX_COUNT && count < capacity; ++i) {
        if (thread_table[i].state == THREAD_UNUSED) {
            continue;
        }
        buffer[count].id = thread_table[i].id;
        buffer[count].state = thread_table[i].state;
        buffer[count].name = thread_table[i].name;
        ++count;
    }
    interrupts_restore_state(was_enabled);
    return count;
}

static void thread_enqueue(thread_t *thread) {
    thread->next = NULL;
    thread->prev = ready_tail;
    if (ready_tail) {
        ready_tail->next = thread;
    }
    ready_tail = thread;
    if (!ready_head) {
        ready_head = thread;
    }
}

static thread_t *thread_dequeue(void) {
    thread_t *thread = ready_head;
    if (!thread) {
        return NULL;
    }
    ready_head = thread->next;
    if (ready_head) {
        ready_head->prev = NULL;
    } else {
        ready_tail = NULL;
    }
    thread->next = NULL;
    thread->prev = NULL;
    return thread;
}

static thread_t *thread_find(uint64_t id) {
    if (id == 0) {
        return NULL;
    }
    for (size_t i = 0; i < THREAD_MAX_COUNT; ++i) {
        if (thread_table[i].state != THREAD_UNUSED && thread_table[i].id == id) {
            return &thread_table[i];
        }
    }
    return NULL;
}

static thread_t *thread_alloc_slot(void) {
    for (size_t i = 0; i < THREAD_MAX_COUNT; ++i) {
        if (thread_table[i].state == THREAD_UNUSED) {
            thread_table[i].state = THREAD_NEW;
            thread_table[i].id = 0;
            thread_table[i].stack = NULL;
            thread_table[i].stack_size = 0;
            thread_table[i].waiting = NULL;
            thread_table[i].next = NULL;
            thread_table[i].prev = NULL;
            thread_table[i].name[0] = '\0';
            return &thread_table[i];
        }
    }
    return NULL;
}

static void thread_destroy(thread_t *thread) {
    if (!thread) {
        return;
    }
    if (thread->stack) {
        kfree(thread->stack);
        thread->stack = NULL;
    }
    thread->stack_size = 0;
    thread->state = THREAD_UNUSED;
    thread->entry = NULL;
    thread->arg = NULL;
    thread->waiting = NULL;
    thread->next = NULL;
    thread->prev = NULL;
    thread->name[0] = '\0';
}

static void scheduler_switch(int requeue_current) {
    thread_t *previous = current_thread;
    if (previous && requeue_current && previous->state == THREAD_RUNNING) {
        previous->state = THREAD_READY;
        thread_enqueue(previous);
    }

    thread_t *next = thread_dequeue();
    if (!next) {
        if (previous && previous->state == THREAD_READY) {
            previous->state = THREAD_RUNNING;
        }
        return;
    }

    if (previous == next) {
        next->state = THREAD_RUNNING;
        return;
    }

    current_thread = next;
    next->state = THREAD_RUNNING;
    if (previous) {
        thread_context_switch(&previous->context, &next->context);
    } else {
        thread_context_switch(&current_thread->context, &current_thread->context);
    }
}

static void thread_trampoline(void) {
    interrupts_enable();
    thread_t *self = current_thread;
    if (!self || !self->entry) {
        thread_exit(-1);
        return;
    }
    self->entry(self->arg);
    thread_exit(0);
}

static void idle_thread(void *arg) {
    (void)arg;
    while (1) {
        thread_yield();
    }
}

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

