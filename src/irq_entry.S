.text
.macro SAVE_REGS
    push %rax
    push %rbp
    push %rbx
    push %rcx
    push %rdx
    push %rdi
    push %rsi
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15
.endm

.macro RESTORE_REGS
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rsi
    pop %rdi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rbp
    pop %rax
.endm

.extern irq_common_handler

.set IRQ_BASE, 0x20

.globl irq_stub_0
.type irq_stub_0, @function
irq_stub_0:
    SAVE_REGS
    mov $IRQ_BASE, %rdi
    mov %rsi, %rsp
    lea 120(%rsp), %rdx
    call irq_common_handler
    RESTORE_REGS
    iretq

