#include <thread.h>
#include <memory.h>
#include <string.h>
#include <terminal.h>
#include <interrupts.h>

#define THREAD_MAX_COUNT 32
#define THREAD_STACK_DEFAULT 4096
#define THREAD_MIN_STACK 2048
#define THREAD_NAME_MAX 32
#define THREAD_QUANTUM_TICKS 5

extern void thread_context_switch(thread_context_t *old_ctx, thread_context_t *new_ctx);

typedef struct thread {
    uint64_t id;
    thread_state_t state;
    thread_context_t context;
    irq_regs_t regs;
    void *stack;
    size_t stack_size;
    thread_entry_t entry;
    void *arg;
    int exit_status;
    struct thread *waiting;
    struct thread *next;
    struct thread *prev;
    char name[THREAD_NAME_MAX];
    uint64_t run_ticks;
    uint64_t quantum_ticks_left;
    int in_ready_queue;
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
static void scheduler_reset_quantum(thread_t *thread);
static void save_context_from_interrupt(thread_t *thread, irq_regs_t *regs, struct interrupt_frame *frame);
static void load_context_to_interrupt(thread_t *thread, irq_regs_t *regs, struct interrupt_frame *frame);
static void thread_block_current_locked(thread_state_t new_state);

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
    bootstrap->in_ready_queue = 0;
    bootstrap->run_ticks = 0;
    scheduler_reset_quantum(bootstrap);

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
    slot->run_ticks = 0;
    slot->in_ready_queue = 0;
    scheduler_reset_quantum(slot);
    memset(&slot->regs, 0, sizeof(slot->regs));

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
        scheduler_reset_quantum(self->waiting);
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

void thread_handle_timer_interrupt(struct interrupt_frame *frame, irq_regs_t *regs) {
    if (!current_thread) {
        return;
    }

    save_context_from_interrupt(current_thread, regs, frame);
    current_thread->run_ticks++;

    if (current_thread->state == THREAD_RUNNING && current_thread->quantum_ticks_left > 0) {
        current_thread->quantum_ticks_left--;
    }

    int need_schedule = 0;
    if (current_thread->state != THREAD_RUNNING) {
        need_schedule = 1;
    } else if (current_thread->quantum_ticks_left == 0) {
        need_schedule = 1;
    }

    if (!need_schedule) {
        return;
    }

    thread_t *candidate = thread_dequeue();
    if (!candidate) {
        current_thread->state = THREAD_RUNNING;
        scheduler_reset_quantum(current_thread);
        return;
    }

    if (current_thread->state == THREAD_RUNNING) {
        current_thread->state = THREAD_READY;
        thread_enqueue(current_thread);
    }

    current_thread = candidate;
    current_thread->state = THREAD_RUNNING;
    scheduler_reset_quantum(current_thread);
    load_context_to_interrupt(current_thread, regs, frame);
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
        thread_block_current_locked(THREAD_BLOCKED);
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
        buffer[count].run_ticks = thread_table[i].run_ticks;
        buffer[count].quantum_ticks = thread_table[i].quantum_ticks_left;
        ++count;
    }
    interrupts_restore_state(was_enabled);
    return count;
}

static void thread_enqueue(thread_t *thread) {
    if (!thread || thread->in_ready_queue) {
        return;
    }
    thread->next = NULL;
    thread->prev = ready_tail;
    thread->in_ready_queue = 1;
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
     thread->in_ready_queue = 0;
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
    thread->run_ticks = 0;
    thread->quantum_ticks_left = 0;
    thread->in_ready_queue = 0;
    memset(&thread->regs, 0, sizeof(thread->regs));
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
    scheduler_reset_quantum(next);
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

static void thread_block_current_locked(thread_state_t new_state) {
    if (!current_thread) {
        return;
    }
    current_thread->state = new_state;
    scheduler_switch(0);
}

void thread_block_current(thread_state_t new_state) {
    int was_enabled = interrupts_save_and_disable();
    thread_block_current_locked(new_state);
    interrupts_restore_state(was_enabled);
}

void thread_unblock_by_id(uint64_t id) {
    int was_enabled = interrupts_save_and_disable();
    thread_t *thread = thread_find(id);
    if (thread && thread->state == THREAD_BLOCKED) {
        thread->state = THREAD_READY;
        scheduler_reset_quantum(thread);
        thread_enqueue(thread);
    }
    interrupts_restore_state(was_enabled);
}

static void scheduler_reset_quantum(thread_t *thread) {
    if (thread) {
        thread->quantum_ticks_left = THREAD_QUANTUM_TICKS;
    }
}

static void save_context_from_interrupt(thread_t *thread, irq_regs_t *regs, struct interrupt_frame *frame) {
    if (!thread || !regs || !frame) {
        return;
    }
    thread->context.r15 = regs->r15;
    thread->context.r14 = regs->r14;
    thread->context.r13 = regs->r13;
    thread->context.r12 = regs->r12;
    thread->context.rbx = regs->rbx;
    thread->context.rbp = regs->rbp;
    thread->context.rsp = frame->rsp;
    thread->context.rip = frame->rip;
    thread->context.rflags = frame->rflags;
    thread->regs = *regs;
}

static void load_context_to_interrupt(thread_t *thread, irq_regs_t *regs, struct interrupt_frame *frame) {
    if (!thread || !regs || !frame) {
        return;
    }
    *regs = thread->regs;
    regs->r15 = thread->context.r15;
    regs->r14 = thread->context.r14;
    regs->r13 = thread->context.r13;
    regs->r12 = thread->context.r12;
    regs->rbx = thread->context.rbx;
    regs->rbp = thread->context.rbp;
    frame->rsp = thread->context.rsp;
    frame->rip = thread->context.rip;
    frame->rflags = thread->context.rflags;
}

