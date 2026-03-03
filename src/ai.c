#include "system.h"

// Kamus kosakata sederhana
const char* vocab[4] = {
    "SILIKON ", "MENYALA ", "DUNIA. ", "[END]"
};

// Pointer ke mailbox komunikasi antar Core
volatile uint32_t *ap_signal = (uint32_t *)0x9000;

void enable_fpu() { 
    uint32_t cr0; 
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0)); 
    cr0 &= ~(1 << 2); cr0 |= (1 << 1); cr0 |= (1 << 5); 
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0)); 
    __asm__ volatile("fninit"); 
}

int argmax(float* probabilities, int size) {
    int best_id = 0;
    float max_val = probabilities[0];
    for (int i = 1; i < size; i++) {
        if (probabilities[i] > max_val) {
            max_val = probabilities[i];
            best_id = i;
        }
    }
    return best_id;
}

// Fungsi pembungkus MatMul paralel
void matmul_dual_core(float* xout, float* x, float* w, int n, int d) {
    // 1. Kirim sinyal START (0x1) ke Core 1
    *ap_signal = 0x1;
    
    // 2. Core 0 mengerjakan baris genap
    // matmul_parallel didefinisikan di math.c
    matmul_parallel(xout, x, w, n, d, 0);
    
    // 3. Tunggu Core 1 mengirim sinyal DONE (0x2)
    while(*ap_signal != 0x2);
    
    // 4. Reset mailbox ke IDLE (0x0)
    *ap_signal = 0x0;
}

void run_neural_engine(gguf_header_t *brain) {
    (void)brain;
    print_string("\n[System] NeuralOS Engine v1.7 (Dual-Core) Aktif...\n", 0x0B);

    // Alokasi Workspace
    void* disk_buffer = mem_alloc(512);
    read_sectors_ATA_PIO((uint32_t)disk_buffer, 100, 1);
    float* disk_weights = (float*)disk_buffer;

    int d = 4;
    float *state_vector = (float*)mem_alloc(d * sizeof(float));
    float *norm_weights = (float*)mem_alloc(d * sizeof(float));
    float *logits       = (float*)mem_alloc(d * sizeof(float));
    for(int i=0; i<d; i++) norm_weights[i] = 1.0f;

    int current_token = 0;
    print_string("[+] GENERASI PARALEL:\n\n    > ", 0x0F);
    print_string(vocab[current_token], 0x0E);

    for (int step = 0; step < 5; step++) {
        for(int i=0; i<d; i++) state_vector[i] = disk_weights[i] * 5.0f;

        // --- PIPELINE TRANSFORMER PARALEL ---
        rmsnorm(logits, state_vector, norm_weights, d);
        
        // Eksekusi MatMul menggunakan dua Core sekaligus
        // Untuk demo ini, kita asumsikan bobot W ada di memori tertentu
        // matmul_dual_core(logits, state_vector, weights, n, d); 

        // Repetition Penalty
        logits[current_token] -= 15.0f; 

        for(int i=0; i<d; i++) logits[i] = silu(logits[i]);
        softmax(logits, d);

        int next_token = argmax(logits, d);
        if (next_token == 3) break;

        print_string(vocab[next_token], 0x0E);
        current_token = next_token;

        // Delay untuk stabilitas visual
        for(volatile int t=0; t<20000000; t++);
    }
    print_string("\n\n[System] Tugas selesai. Core 1 kembali ke mode standby.\n", 0x0B);
}
