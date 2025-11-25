.text
.globl thread_context_switch
.type thread_context_switch, @function
thread_context_switch:
    # rdi -> old context, rsi -> new context
    mov %r15, 0(%rdi)
    mov %r14, 8(%rdi)
    mov %r13, 16(%rdi)
    mov %r12, 24(%rdi)
    mov %rbx, 32(%rdi)
    mov %rbp, 40(%rdi)
    mov %rsp, 48(%rdi)
    leaq .Lresume(%rip), %rax
    mov %rax, 56(%rdi)
    pushfq
    pop %rax
    mov %rax, 64(%rdi)

    mov 0(%rsi), %r15
    mov 8(%rsi), %r14
    mov 16(%rsi), %r13
    mov 24(%rsi), %r12
    mov 32(%rsi), %rbx
    mov 40(%rsi), %rbp
    mov 48(%rsi), %rsp
    mov 64(%rsi), %rax
    push %rax
    popfq
    mov 56(%rsi), %rax
    jmp *%rax
.Lresume:
    ret

