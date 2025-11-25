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

    /* Create idle thread so scheduler always has a runnable task */
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
    stack_top &= ~(uintptr_t)0xFUL; /* 16-byte alignment */

    memset(&slot->context, 0, sizeof(slot->context));
    slot->context.rsp = stack_top;
    slot->context.rip = (uint64_t)thread_trampoline;
    slot->context.rflags = 0x202; /* IF flag set */

    thread_enqueue(slot);
    interrupts_restore_state(was_enabled);
    return slot->id;
}

void thread_exit(int status) {
    int was_enabled = interrupts_save_and_disable();
    thread_t *self = current_thread;
    if (!self) {
        interrupts_enable();
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

    /* Should never reach here */
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
            return -3; /* Already has a waiter */
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
        /* No previous thread (should not happen after init) */
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
#include <thread.h>
#include <memory.h>
#include <string.h>
#include <terminal.h>
#include <pit.h>
#include <interrupts.h>

#define THREAD_MAX_COUNT 16
#define THREAD_STACK_SIZE 0x4000
#define THREAD_NAME_MAX   31

typedef enum thread_state {
    THREAD_UNUSED = 0,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_SLEEPING,
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
} thread_context_t;

typedef struct thread {
    thread_context_t context;
    uint8_t *stack;
    size_t stack_size;
    thread_state_t state;
    thread_entry_t entry;
    void *arg;
    uint64_t sleep_until_tick;
    char name[THREAD_NAME_MAX + 1];
    uint32_t id;
} thread_t;

static thread_t threads[THREAD_MAX_COUNT];
static thread_t *current_thread = NULL;
static uint32_t next_thread_id = 1;
static int thread_system_ready = 0;

static void thread_context_switch(thread_context_t *old_ctx, thread_context_t *new_ctx) __attribute__((naked));
static void thread_trampoline(void);
static void thread_idle(void *arg);
static void thread_schedule_locked(void);
static void thread_cleanup_zombies(void);
static thread_t *thread_pick_next(void);
static void thread_print_uint(uint64_t value);

__attribute__((naked))
static void thread_context_switch(thread_context_t *old_ctx, thread_context_t *new_ctx) {
    __asm__ volatile(
        "movq %r15, 0(%rdi)\n\t"
        "movq %r14, 8(%rdi)\n\t"
        "movq %r13, 16(%rdi)\n\t"
        "movq %r12, 24(%rdi)\n\t"
        "movq %rbx, 32(%rdi)\n\t"
        "movq %rbp, 40(%rdi)\n\t"
        "movq %rsp, 48(%rdi)\n\t"

        "movq 48(%rsi), %rsp\n\t"
        "movq 40(%rsi), %rbp\n\t"
        "movq 32(%rsi), %rbx\n\t"
        "movq 24(%rsi), %r12\n\t"
        "movq 16(%rsi), %r13\n\t"
        "movq 8(%rsi), %r14\n\t"
        "movq 0(%rsi), %r15\n\t"
        "ret\n\t"
    );
}

static void thread_reset(thread_t *thread) {
    if (!thread) {
        return;
    }
    if (thread->stack) {
        kfree(thread->stack);
    }
    memset(thread, 0, sizeof(thread_t));
    thread->state = THREAD_UNUSED;
}

void thread_system_init(void) {
    if (thread_system_ready) {
        return;
    }
    memset(threads, 0, sizeof(threads));
    thread_t *boot = &threads[0];
    boot->state = THREAD_RUNNING;
    boot->id = next_thread_id++;
    strncpy(boot->name, "boot", THREAD_NAME_MAX);
    current_thread = boot;
    thread_system_ready = 1;
    thread_create(thread_idle, NULL, "idle");
}

static thread_t *thread_alloc_slot(void) {
    for (size_t i = 0; i < THREAD_MAX_COUNT; ++i) {
        if (threads[i].state == THREAD_UNUSED) {
            return &threads[i];
        }
    }
    return NULL;
}

int thread_create(thread_entry_t entry, void *arg, const char *name) {
    if (!thread_system_ready || entry == NULL) {
        return -1;
    }
    interrupts_disable();
    thread_t *slot = thread_alloc_slot();
    if (!slot) {
        interrupts_enable();
        return -1;
    }

    memset(&slot->context, 0, sizeof(slot->context));
    slot->stack_size = THREAD_STACK_SIZE;
    slot->stack = (uint8_t *)kmalloc(slot->stack_size);
    if (!slot->stack) {
        interrupts_enable();
        return -1;
    }

    uintptr_t stack_top = (uintptr_t)slot->stack + slot->stack_size;
    stack_top &= ~((uintptr_t)0xF);
    uint64_t *stack64 = (uint64_t *)stack_top;
    *(--stack64) = (uint64_t)thread_trampoline;
    slot->context.rsp = (uint64_t)stack64;
    slot->context.rbp = 0;
    slot->context.rbx = 0;
    slot->context.r12 = 0;
    slot->context.r13 = 0;
    slot->context.r14 = 0;
    slot->context.r15 = 0;

    slot->entry = entry;
    slot->arg = arg;
    slot->state = THREAD_READY;
    slot->sleep_until_tick = 0;
    slot->id = next_thread_id++;
    if (name && name[0] != '\0') {
        strncpy(slot->name, name, THREAD_NAME_MAX);
    } else {
        strncpy(slot->name, "thread", THREAD_NAME_MAX);
    }
    slot->name[THREAD_NAME_MAX] = '\0';
    interrupts_enable();
    return (int)slot->id;
}

static void thread_exit(void) __attribute__((noreturn));

static void thread_exit(void) {
    interrupts_disable();
    if (current_thread) {
        current_thread->state = THREAD_ZOMBIE;
    }
    thread_schedule_locked();
    __builtin_unreachable();
}

static void thread_trampoline(void) {
    interrupts_enable();
    if (current_thread && current_thread->entry) {
        current_thread->entry(current_thread->arg);
    }
    thread_exit();
}

static void thread_idle(void *arg) {
    (void)arg;
    while (1) {
        interrupts_enable();
        __asm__ volatile("hlt");
        interrupts_disable();
        thread_yield();
    }
}

static void thread_cleanup_zombies(void) {
    for (size_t i = 0; i < THREAD_MAX_COUNT; ++i) {
        thread_t *t = &threads[i];
        if (t->state == THREAD_ZOMBIE && t != current_thread) {
            thread_reset(t);
        }
    }
}

static thread_t *thread_pick_next(void) {
    if (!current_thread) {
        return NULL;
    }
    size_t start = (size_t)(current_thread - threads);
    for (size_t offset = 1; offset <= THREAD_MAX_COUNT; ++offset) {
        size_t idx = (start + offset) % THREAD_MAX_COUNT;
        thread_t *candidate = &threads[idx];
        if (candidate->state == THREAD_READY) {
            return candidate;
        }
    }
    return current_thread;
}

static void thread_schedule_locked(void) {
    thread_cleanup_zombies();
    thread_t *next = thread_pick_next();
    if (!next || next == current_thread) {
        return;
    }
    thread_t *prev = current_thread;
    if (prev && prev->state == THREAD_RUNNING) {
        prev->state = THREAD_READY;
    }
    next->state = THREAD_RUNNING;
    current_thread = next;
    thread_context_switch(&prev->context, &next->context);
}

void thread_yield(void) {
    if (!thread_system_ready) {
        return;
    }
    interrupts_disable();
    thread_schedule_locked();
    interrupts_enable();
}

void thread_sleep_ticks(uint64_t ticks) {
    if (!thread_system_ready || ticks == 0) {
        thread_yield();
        return;
    }
    interrupts_disable();
    if (current_thread) {
        current_thread->sleep_until_tick = pit_ticks() + ticks;
        current_thread->state = THREAD_SLEEPING;
    }
    thread_schedule_locked();
    interrupts_enable();
}

void thread_sleep_ms(uint64_t milliseconds) {
    uint32_t freq = pit_current_frequency();
    if (freq == 0) {
        freq = 100;
    }
    uint64_t ticks = (milliseconds * freq + 999) / 1000;
    if (ticks == 0) {
        ticks = 1;
    }
    thread_sleep_ticks(ticks);
}

const char *thread_current_name(void) {
    if (!current_thread) {
        return "unknown";
    }
    return current_thread->name[0] ? current_thread->name : "thread";
}

void thread_dump_state(void) {
    if (!thread_system_ready) {
        terminal_write_line("Thread system not initialized.");
        return;
    }
    interrupts_disable();
    terminal_write_line("Active threads:");
    for (size_t i = 0; i < THREAD_MAX_COUNT; ++i) {
        thread_t *t = &threads[i];
        if (t->state == THREAD_UNUSED) {
            continue;
        }
        const char *state = "unknown";
        switch (t->state) {
            case THREAD_READY: state = "ready"; break;
            case THREAD_RUNNING: state = "running"; break;
            case THREAD_SLEEPING: state = "sleeping"; break;
            case THREAD_ZOMBIE: state = "zombie"; break;
            default: break;
        }
        terminal_write("  #");
        thread_print_uint(t->id);
        terminal_write(" ");
        terminal_write(t->name);
        terminal_write(" [");
        terminal_write(state);
        terminal_write("]");
        if (t == current_thread) {
            terminal_write(" <current>");
        }
        terminal_putc('\n');
    }
    interrupts_enable();
}

void thread_tick(void) {
    if (!thread_system_ready) {
        return;
    }
    uint64_t now = pit_ticks();
    for (size_t i = 0; i < THREAD_MAX_COUNT; ++i) {
        thread_t *t = &threads[i];
        if (t->state == THREAD_SLEEPING && now >= t->sleep_until_tick) {
            t->state = THREAD_READY;
            t->sleep_until_tick = 0;
        }
    }
}

static void thread_print_uint(uint64_t value) {
    char buffer[21];
    int index = 20;
    buffer[index] = '\0';
    if (value == 0) {
        buffer[--index] = '0';
    } else {
        while (value > 0 && index > 0) {
            buffer[--index] = (char)('0' + (value % 10));
            value /= 10;
        }
    }
    terminal_write(&buffer[index]);
}


