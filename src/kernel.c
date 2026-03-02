#include "system.h"
gguf_header_t *global_ai_brain = 0; 
static inline unsigned char inb(unsigned short port) { unsigned char res; __asm__ volatile("inb %1, %0" : "=a"(res) : "Nd"(port)); return res; }
const char scancode_to_ascii[] = { 0,0,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' ' };

void kernel_main(uint32_t eax_magic, uint32_t ebx_mboot_ptr) {
    clear_screen();
    print_string("NeuralOS v1.2 - Dynamic Memory (KV Cache)\n", 0x0B); 
    enable_fpu();
    
    // NYALAKAN RADAR MEMORI DI BATAS AMAN 16 MB (0x1000000)
    init_heap(0x1000000);
    
    if (eax_magic == 0x2BADB002) {
        multiboot_info_t *mboot = (multiboot_info_t *)ebx_mboot_ptr;
        if (mboot->flags & (1 << 3) && mboot->mods_count > 0) { 
            multiboot_module_t *mod = (multiboot_module_t *)mboot->mods_addr;
            global_ai_brain = (gguf_header_t *)mod->mod_start;
        }
    }
    
    print_string("\nKetik 'ai' untuk simulasi KV Cache Dinamis!\n> ", 0x0F);
    char command_buffer[256]; int cmd_index = 0;

    while (1) {
        if (inb(0x64) & 0x01) {
            unsigned char scancode = inb(0x60);
            if (scancode < 0x80) {
                char c = scancode_to_ascii[scancode];
                if (c == '\n') {
                    command_buffer[cmd_index] = '\0';
                    if (string_compare(command_buffer, "ai")) {
                        run_neural_engine(global_ai_brain);
                    }
                    cmd_index = 0; print_string("> ", 0x0A);
                } else if (c == '\b') {
                    if (cmd_index > 0) { cmd_index--; print_string("\b \b", 0x0F); }
                } else if (c != 0 && cmd_index < 255) {
                    command_buffer[cmd_index] = c; cmd_index++; print_char(c, 0x0F);
                }
            }
        }
    }
}
