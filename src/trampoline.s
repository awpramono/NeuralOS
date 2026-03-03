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
    
    # Read this core's APIC ID from Local APIC register
    # APIC ID is at 0xFEE00020, bits 24-27
    mov 0xFEE00020, %eax
    shr $24, %eax
    # %eax = APIC ID (1, 2, or 3 for worker cores)

    # Set up per-core stack: 0x8000 - APIC_ID * 0x1000
    # Core 1: stack at 0x7000 (4KB)
    # Core 2: stack at 0x6000 (4KB)
    # Core 3: stack at 0x5000 (4KB)
    mov %eax, %ecx
    shl $12, %ecx           # ecx = APIC_ID * 4096
    mov $0x8000, %esp
    sub %ecx, %esp          # esp = 0x8000 - APIC_ID * 4096

    # Enable FPU on this core (required for float matmul)
    mov %cr0, %eax
    and $0xFFFFFFFB, %eax   # Clear EM (bit 2)
    or  $0x22, %eax         # Set MP (bit 1) + NE (bit 5)
    mov %eax, %cr0
    fninit

    # Atomically increment cores_booted counter at 0x901C
    lock incl 0x901C

    # Jump to C worker function (address stored at 0x9100 by Core 0)
    jmp *0x9100

    # Fallback halt
1:  hlt
    jmp 1b

trampoline_gdt_ptr:
    .word 23
    .long (trampoline_gdt - trampoline_start + 0x8000)

trampoline_gdt:
    .quad 0x0000000000000000
    .quad 0x00cf9a000000ffff
    .quad 0x00cf92000000ffff
trampoline_end:
