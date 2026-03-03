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
    // Model: sector 200+, Tokenizer: sector 130000+
    // These are injected by scripts/prepare_model.py
    int has_llama = 0;
    {
        // Quick check: read first sector and see if it looks like a valid config
        void* probe = mem_alloc(512);
        read_sectors_ATA_PIO((uint32_t)probe, 200, 1);
        int *cfg = (int*)probe;
        // Sanity: dim should be > 0 and < 10000, n_layers > 0 and < 100
        if (cfg[0] > 0 && cfg[0] < 10000 && cfg[2] > 0 && cfg[2] < 100) {
            has_llama = llama_load_model(200);
            if (has_llama) {
                // Load tokenizer from disk (at sector 130000)
                // tok512.bin ~16KB for stories260K, tok32000.bin ~500KB for stories15M
                llama_load_tokenizer(130000, 1024000);  // read up to 1MB
            }
        } else {
            print_string("[Boot] No Llama2 model on disk.\n", 0x08);
            print_string("  Run: python3 scripts/prepare_model.py\n", 0x08);
        }
    }

    if (has_llama) {
        print_string("\n[System] Llama2 Transformer loaded! Type LLAMA to generate.\n", 0x0A);
    }
    print_string("[System] Type HELP for commands, or talk to the 32-token AI.\n", 0x0A);

    char prompt_buffer[128];

    while (1) {
        print_string("\nUSER > ", 0x0A);
        get_input(prompt_buffer, 128);

        // Check for built-in OS commands first
        if (ci_compare(prompt_buffer, "CLEAR")) {
            clear_screen();
            print_string("NeuralOS v3.0 - Screen Cleared\n", 0x0B);
        } else if (ci_compare(prompt_buffer, "HELP")) {
            cmd_help();
            if (has_llama) {
                print_string("| LLAMA   - Generate text with Llama2 Transformer |\n", 0x0D);
                print_string("+--------------------------------------------------+\n", 0x0B);
            }
        } else if (ci_compare(prompt_buffer, "INFO")) {
            cmd_info();
            if (has_llama) {
                print_string("  AI Model : Llama2 Transformer (", 0x0F);
                print_number(llama_get_vocab_size(), 0x0E);
                print_string(" tokens)\n", 0x0F);
            }
        } else if (ci_compare(prompt_buffer, "MEM")) {
            cmd_mem();
        } else if (ci_compare(prompt_buffer, "VER")) {
            cmd_ver();
        } else if (ci_compare(prompt_buffer, "LLAMA")) {
            if (has_llama) {
                print_string("[Generating with Llama2 Transformer...]\n", 0x0D);
                llama_generate(64);  // Generate up to 64 tokens
            } else {
                print_string("[!] Llama2 model not loaded.\n", 0x0C);
                print_string("    Run: python3 scripts/prepare_model.py\n", 0x0C);
                print_string("    Then: make run\n", 0x0C);
            }
        } else if (prompt_buffer[0] != '\0') {
            // Not a built-in command -> send to 32-token AI engine
            run_neural_engine((gguf_header_t*)0, prompt_buffer); 
        }
    }
}
