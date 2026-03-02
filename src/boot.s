/* boot.s - NeuralOS v0.6 */
.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
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
    
    /* THE OVERRIDE: Lempar parameter Bootloader (Peta RAM) ke Kernel C */
    push %ebx  /* Parameter 2: Alamat Peta Memori Multiboot */
    push %eax  /* Parameter 1: Magic Number (Buktikan ini bootloader asli) */
    
    call kernel_main
    cli
1:  hlt
    jmp 1b
