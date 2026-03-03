.code16
.global trampoline_start
.global trampoline_end

trampoline_start:
    cli
    xor %ax, %ax
    mov %ax, %ds
    lgdtl (trampoline_gdt_ptr - trampoline_start + 0x8000)
    mov %cr0, %eax
    inc %eax
    mov %eax, %cr0
    ljmpl $0x08, $(ap_entry - trampoline_start + 0x8000)

.code32
ap_entry:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    
    # Beritahu Core 0 bahwa Core 1 sudah standby
    movl $0xCAFEBABE, 0x9000

ap_loop:
    # Tunggu sinyal 0x1 dari Core 0
    cmpl $0x1, 0x9000
    jne ap_loop

    # Di sini Core 1 secara teknis bisa menjalankan fungsi C, 
    # namun untuk minimalis, kita beri sinyal DONE (0x2) balik
    movl $0x2, 0x9000
    jmp ap_loop

trampoline_gdt_ptr:
    .word 23
    .long (trampoline_gdt - trampoline_start + 0x8000)

trampoline_gdt:
    .quad 0x0000000000000000
    .quad 0x00cf9a000000ffff
    .quad 0x00cf92000000ffff
trampoline_end:
