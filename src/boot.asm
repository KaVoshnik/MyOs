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
    dq pd_table + 0x03
    times 511 dq 0

align 4096
pd_table:
%assign i 0
%rep 512
    dq (i << 21) | 0x183        ; 1GiB identity (2MiB pages)
%assign i i+1
%endrep

SECTION .bss
align 16
stack_bottom:
    resb 4096
stack_top:

