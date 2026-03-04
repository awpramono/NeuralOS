#include "system.h"

static inline uint8_t inb_port(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

void get_input(char *buffer, int max_len) {
    int idx = 0;
    while (1) {
        char c = keyboard_read_char();
        if (c) {
            if (c == '\n') {
                print_char('\n', 0x0F);
                buffer[idx] = '\0';
                return;
            } else if (c == '\b' && idx > 0) {
                idx--;
                print_char('\b', 0x07); print_char(' ', 0x07); print_char('\b', 0x07);
            } else if (idx < max_len - 1 && c != '\b') {
                buffer[idx++] = c;
                print_char(c, 0x0E);
            }
            while(inb_port(0x60) < 0x80);
        }
    }
}

// Simple case-insensitive compare
static int ci_compare(const char *a, const char *b) {
    while (*a && *b) {
        char ca = (*a >= 'a' && *a <= 'z') ? *a - 32 : *a;
        char cb = (*b >= 'a' && *b <= 'z') ? *b - 32 : *b;
        if (ca != cb) return 0;
        a++; b++;
    }
    return (*a == *b);
}

// Check if string starts with prefix
static int starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*str == '\0') return 0;
        char cs = (*str >= 'a' && *str <= 'z') ? *str - 32 : *str;
        char cp = (*prefix >= 'a' && *prefix <= 'z') ? *prefix - 32 : *prefix;
        if (cs != cp) return 0;
        str++; prefix++;
    }
    return 1;
}

// ============================================================================
// BUILT-IN OS COMMANDS (executed natively, no AI needed)
// ============================================================================

static void cmd_help() {
    print_string("\n", 0x07);
    print_string("+----------------------------------------------+\n", 0x0B);
    print_string("|          NeuralOS v3.0 - Command List        |\n", 0x0B);
    print_string("+----------------------------------------------+\n", 0x0B);
    print_string("| HELP    - Tampilkan daftar perintah ini      |\n", 0x07);
    print_string("| INFO    - Informasi sistem                   |\n", 0x07);
    print_string("| MEM     - Status memori                      |\n", 0x07);
    print_string("| CLEAR   - Bersihkan layar                    |\n", 0x07);
    print_string("| VER     - Versi NeuralOS                     |\n", 0x07);
    print_string("+----------------------------------------------+\n", 0x0B);
    print_string("| Ketik apapun selain perintah di atas untuk   |\n", 0x0D);
    print_string("| berbicara dengan AI Engine secara langsung!  |\n", 0x0D);
    print_string("+----------------------------------------------+\n", 0x0B);
}

static void cmd_info() {
    print_string("\n", 0x07);
    print_string("[System Info]\n", 0x0B);
    print_string("  OS       : NeuralOS v3.0 (Bare-Metal AI)\n", 0x0F);
    print_string("  Arch     : x86 (i386), 32-bit Protected Mode\n", 0x0F);
    print_string("  CPU      : ", 0x0F);
    print_number(*(volatile uint32_t*)0x9018, 0x0E);
    print_string(" cores (SMP via APIC IPI)\n", 0x0F);
    print_string("  RAM      : 512MB (QEMU)\n", 0x0F);
    print_string("  Heap     : Bump Allocator @ ", 0x0F);
    print_hex(0x1000000, 0x0E);
    print_string("\n", 0x07);
    print_string("  VGA      : 80x25 text mode @ 0xB8000\n", 0x0F);
    print_string("  Keyboard : PS/2 polling driver\n", 0x0F);
    print_string("  Disk     : ATA PIO mode\n", 0x0F);
    print_string("  AI Model : TinyLLM-32tok (GGUF in-memory)\n", 0x0F);
    print_string("  Math     : FPU + Taylor Series (exp,sin,cos,sqrt)\n", 0x0F);
}

static void cmd_mem() {
    print_string("\n", 0x07);
    print_string("[Memory Status]\n", 0x0B);
    print_string("  Heap Base     : ", 0x0F);
    print_hex(0x1000000, 0x0E);
    print_string("\n", 0x07);
    print_string("  Heap Current  : ", 0x0F);
    uint32_t usage = get_heap_usage();
    print_hex(usage, 0x0E);
    print_string("\n", 0x07);
    print_string("  Allocated     : ", 0x0F);
    print_number(usage - 0x1000000, 0x0E);
    print_string(" bytes\n", 0x07);
    print_string("  Total RAM     : 512 MB\n", 0x0F);
    print_string("  AI Brain      : loaded in RAM\n", 0x0A);
}

static void cmd_ver() {
    print_string("\n", 0x07);
    print_string("  _   _                      _  ___  ____  \n", 0x0D);
    print_string(" | \\ | | ___ _   _ _ __ __ _| |/ _ \\/ ___| \n", 0x0D);
    print_string(" |  \\| |/ _ \\ | | | '__/ _` | | | | \\___ \\ \n", 0x0D);
    print_string(" | |\\  |  __/ |_| | | | (_| | | |_| |___) |\n", 0x0D);
    print_string(" |_| \\_|\\___|\\__,_|_|  \\__,_|_|\\___/|____/ \n", 0x0D);
    print_string("\n", 0x07);
    print_string("  Version 3.0 - Bare-Metal Tiny LLM OS\n", 0x0B);
    print_string("  Built with: GCC + GNU AS + LD (i386)\n", 0x07);
    print_string("  AI Engine : 32-token autoregressive LLM\n", 0x07);
    print_string("  License   : MIT (Educational Purpose)\n", 0x08);
}

// ============================================================================
// KERNEL MAIN
// ============================================================================

void kernel_main(uint32_t magic, uint32_t ebx_mboot_ptr) {
    (void)magic; (void)ebx_mboot_ptr;
    
    clear_screen();
    print_string("  _   _                      _  ___  ____  \n", 0x0D);
    print_string(" | \\ | | ___ _   _ _ __ __ _| |/ _ \\/ ___| \n", 0x0D);
    print_string(" |  \\| |/ _ \\ | | | '__/ _` | | | | \\___ \\ \n", 0x0D);
    print_string(" | |\\  |  __/ |_| | | | (_| | | |_| |___) |\n", 0x0D);
    print_string(" |_| \\_|\\___|\\__,_|_|  \\__,_|_|\\___/|____/ \n", 0x0D);
    print_string("\n", 0x07);
    print_string(" NeuralOS v3.0 - Bare-Metal Tiny LLM OS\n", 0x0B);
    print_string(" ========================================\n", 0x08);

    enable_fpu();
    init_heap(0x1000000);
    init_smp();

    // Initialize the simple 32-token AI engine (always available)
    print_string("[Boot] Initializing 32-token AI engine...\n", 0x0E);
    init_neural_weights();

    // Attempt to load Llama2 model from disk
    // Q8 model at sector 300 (if present), otherwise float32 at sector 200
    // Tokenizer at sector 130000
    int has_llama = 0;
    {
        void* probe = mem_alloc(512);

        // Try Q8 model first (sector 300)
        // Q8 magic marker 'NNQ8' at bytes 508-511 of first sector
        read_sectors_ATA_PIO((uint32_t)probe, 300, 1);
        int *q8cfg = (int*)probe;
        uint8_t *marker = (uint8_t*)probe + 508;
        if (q8cfg[0] > 0 && q8cfg[0] < 10000 && q8cfg[2] > 0 && q8cfg[2] < 100
            && marker[0] == 'N' && marker[1] == 'N' && marker[2] == 'Q' && marker[3] == '8') {
            print_string("[Boot] Q8 quantized model detected!\n", 0x0D);
            has_llama = llama_load_model_q8(300);
        }

        // Fallback: try float32 model (sector 200)
        if (!has_llama) {
            read_sectors_ATA_PIO((uint32_t)probe, 200, 1);
            int *cfg = (int*)probe;
            if (cfg[0] > 0 && cfg[0] < 10000 && cfg[2] > 0 && cfg[2] < 100) {
                has_llama = llama_load_model(200);
            }
        }

        if (has_llama) {
            llama_load_tokenizer(130000, 1024000);
        } else {
            print_string("[Boot] No Llama2 model on disk.\n", 0x08);
            print_string("  Run: python3 scripts/prepare_model.py\n", 0x08);
        }
    }

    if (has_llama) {
        print_string("\n[System] Llama2 Transformer loaded! Type LLAMA to generate.\n", 0x0A);
    }

    if (has_llama) {
        print_string("\n[System] NeuralOS Agentic Edition — AI Agent Active\n", 0x0A);
        print_string("[System] Talk naturally! I understand intent, not just commands.\n", 0x0E);
    } else {
        print_string("[System] Type HELP for commands, or talk to the AI.\n", 0x0A);
    }

    char prompt_buffer[128];

    while (1) {
        print_string("\nUSER > ", 0x0A);
        get_input(prompt_buffer, 128);

        if (prompt_buffer[0] == '\0') continue;

        // Direct override: LLAMA command for raw transformer access
        if (ci_compare(prompt_buffer, "LLAMA")) {
            if (has_llama) {
                print_string("[Direct: Llama2 Transformer]\n", 0x0D);
                llama_generate(256);
            } else {
                print_string("[!] Llama2 not loaded.\n", 0x0C);
            }
        } else if (starts_with(prompt_buffer, "LLAMA ") || starts_with(prompt_buffer, "llama ")) {
            if (has_llama) {
                print_string("[Direct: Llama2 Prompt]\n", 0x0D);
                llama_generate_with_prompt(prompt_buffer + 6, 256);
            } else {
                print_string("[!] Llama2 not loaded.\n", 0x0C);
            }
        } else {
            // ALL other input goes through Agent AI dispatcher
            agent_dispatch(prompt_buffer);
        }
    }
}
