    .section .multiboot
    .align 4
    .equ ALIGN, 1 << 0
    .equ MEMINFO, 1 << 1
    .equ FLAGS, ALIGN | MEMINFO
    .equ MAGIC, 0x1BADB002
    .equ CHECKSUM, -(MAGIC + FLAGS)

    .long MAGIC
    .long FLAGS
    .long CHECKSUM

    .section .bss
    .align 16
stack_bottom:
    .skip 16384
stack_top:

    .section .text
    .global _start
    .type _start, @function
_start:
    mov $stack_top, %esp

    lgdt (gdt_ptr)

    push $0x08
    push $.reload_cs
    retf

.reload_cs:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    call kernel_main

    cli
1:  hlt
    jmp 1b

.section .data
.align 4
gdt_start:
    .long 0, 0              
gdt_code:                   
    .word 0xFFFF, 0x0000
    .byte 0x00, 0x9A, 0xCF, 0x00
gdt_data:                   
    .word 0xFFFF, 0x0000
    .byte 0x00, 0x92, 0xCF, 0x00
gdt_end:

gdt_ptr:
    .word gdt_end - gdt_start - 1
    .long gdt_start
