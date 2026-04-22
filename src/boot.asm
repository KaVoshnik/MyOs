BITS 32

%define MULTIBOOT_MAGIC 0x1BADB002
%define MULTIBOOT_FLAGS 0x00000000
%define MULTIBOOT_CHECKSUM -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)
%define CODE_SEG 0x08
%define DATA_SEG 0x10

extern kernel_main

section .multiboot
align 4
    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM

section .text
global start

start:
    cli
    mov esp, stack_top
    
    ; Save multiboot info pointer (passed in EBX)
    mov [multiboot_info], ebx

    lgdt [gdt_descriptor]

    mov eax, cr4
    or eax, 1 << 5          ; set PAE
    mov cr4, eax

    mov eax, pml4_table
    mov cr3, eax

    mov ecx, 0xC0000080     ; IA32_EFER
    rdmsr
    or eax, 1 << 8          ; enable LME
    wrmsr

    mov eax, cr0
    or eax, 1 << 31         ; enable paging
    or eax, 1 << 0          ; protect mode
    mov cr0, eax

    jmp CODE_SEG:long_mode_entry

; 64-bit mode

SECTION .text
BITS 64

long_mode_entry:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov rsp, stack_top
    
    ; Pass multiboot info to kernel
    mov rdi, [multiboot_info]

    call kernel_main

.hang:
    hlt
    jmp .hang

; GDT definitions

SECTION .data
align 16
gdt:
    dq 0x0000000000000000        ; null
    dq 0x00AF9A000000FFFF        ; code
    dq 0x00AF92000000FFFF        ; data
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt - 1
    dq gdt

; Page tables

align 4096
pml4_table:
    dq pdpt_table + 0x03
    times 511 dq 0

align 4096
pdpt_table:
    dq pd_table_0 + 0x03   ; 0x00000000 - 0x3FFFFFFF  (first  1 GiB)
    dq pd_table_1 + 0x03   ; 0x40000000 - 0x7FFFFFFF  (second 1 GiB)
    dq pd_table_2 + 0x03   ; 0x80000000 - 0xBFFFFFFF  (third  1 GiB)
    dq pd_table_3 + 0x03   ; 0xC0000000 - 0xFFFFFFFF  (fourth 1 GiB — covers 0xE0000000 LFB)
    times 508 dq 0

; First GiB: 0x00000000 - 0x3FFFFFFF
align 4096
pd_table_0:
%assign i 0
%rep 512
    dq (i << 21) | 0x183
%assign i i+1
%endrep

; Second GiB: 0x40000000 - 0x7FFFFFFF
align 4096
pd_table_1:
%assign i 512
%rep 512
    dq (i << 21) | 0x183
%assign i i+1
%endrep

; Third GiB: 0x80000000 - 0xBFFFFFFF
align 4096
pd_table_2:
%assign i 1024
%rep 512
    dq (i << 21) | 0x183
%assign i i+1
%endrep

; Fourth GiB: 0xC0000000 - 0xFFFFFFFF  (MMIO region, VBE LFB lives here)
align 4096
pd_table_3:
%assign i 1536
%rep 512
    dq (i << 21) | 0x183
%assign i i+1
%endrep

SECTION .data
align 8
multiboot_info:
    dq 0

SECTION .bss
align 16
stack_bottom:
    resb 4096
stack_top:

