#include "system.h"

static inline uint8_t inb_port(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

void get_input(char *buffer, int max_len) {
    int idx = 0;
    uint32_t idle_ticks = 0;
    const uint32_t IDLE_THRESHOLD = 80000000; // Sekitar 10-15 detik di QEMU CPU poll-loop
    
    while (1) {
        char c = keyboard_poll_char();
        if (c) {
            idle_ticks = 0; // Reset timer saat ada aktivitas
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
            while(inb_port(0x60) < 0x80); // Wait for key release
        } else {
            // Pengguna sedang diam (idle)
            idle_ticks++;
            if (idx == 0 && idle_ticks >= IDLE_THRESHOLD) {
                // Panggil interaksi proaktif, lalu gambar lagi prompt-nya
                agent_proactive_prompt();
                idle_ticks = 0;  // Reset ke 0 setelah memberikan proaktif
                print_string("\nUSER > ", 0x0A);
            }
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
// Old cmd_* functions removed — Agent handles all commands now
// See src/agent.c for intent classification and dispatch


// ============================================================================
// KERNEL MAIN
// ============================================================================

void kernel_main(uint32_t magic, uint32_t ebx_mboot_ptr) {
    (void)magic; (void)ebx_mboot_ptr;
    
    // Bring up COM1 Serial Telemetry first
    serial_init();
    
    clear_screen();
    print_string("  _   _                      _  ___  ____  \n", 0x0D);
    print_string(" | \\ | | ___ _   _ _ __ __ _| |/ _ \\/ ___| \n", 0x0D);
    print_string(" |  \\| |/ _ \\ | | | '__/ _` | | | | \\___ \\ \n", 0x0D);
    print_string(" | |\\  |  __/ |_| | | | (_| | | |_| |___) |\n", 0x0D);
    print_string(" |_| \\_|\\___|\\__,_|_|  \\__,_|_|\\___/|____/ \n", 0x0D);
    print_string("\n", 0x07);
    print_string(" NeuralOS v3.5 - Agentic AI Operating System\n", 0x0B);
    print_string(" ============================================\n", 0x08);

    enable_fpu();
    init_heap(0x1000000);
    init_smp();
    update_status_bar();

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

    // Update status bar with loaded model info
    update_status_bar();

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
