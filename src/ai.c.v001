#include "system.h"

// 1. KAMUS KOSAKATA (VOCABULARY)
const char* vocab[4] = {
    "SILIKON ", // Token 0
    "MENYALA ", // Token 1
    "DUNIA. ",  // Token 2
    "[END]"     // Token 3
};

void enable_fpu() { 
    uint32_t cr0; __asm__ volatile("mov %%cr0, %0" : "=r"(cr0)); 
    cr0 &= ~(1 << 2); cr0 |= (1 << 1); cr0 |= (1 << 5); 
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0)); 
    __asm__ volatile("fninit"); 
}

// 2. FUNGSI PENCARI PEMENANG (ARGMAX)
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

void run_neural_engine(gguf_header_t *brain) {
    (void)brain;
    print_string("\n[System] Memulai Sekuens Autoregressive (The Loop)...\n", 0x0B);
    print_string("[AI] Prompt diterima: \"SILIKON\"\n", 0x0A);
    print_string("[+] GENERASI TEKS DIMULAI:\n\n    > ", 0x0F);

    // Alokasi memori dinamis untuk komputasi
    float *state_vector = (float*)mem_alloc(4 * sizeof(float));
    float *norm_weights = (float*)mem_alloc(4 * sizeof(float));
    float *logits = (float*)mem_alloc(4 * sizeof(float));
    
    // Inisialisasi bobot normalisasi (Skala 1.0)
    for(int i=0; i<4; i++) norm_weights[i] = 1.0f;

    // Token awal kita adalah 0 ("SILIKON")
    int current_token = 0;
    print_string(vocab[current_token], 0x0E); // Cetak kata pertama

    // THE INFERENCE LOOP (Maksimal 5 putaran untuk purwarupa)
    for (int step = 0; step < 5; step++) {
        
        // --- 1. SIMULASI EMBEDDING & KONTEKS ---
        // Di LLM nyata, kita mengambil baris matriks berdasarkan current_token.
        // Di purwarupa ini, kita memanipulasi vektor status secara matematis.
        for(int i=0; i<4; i++) state_vector[i] = 0.0f;
        
        // Kita ciptakan "gravitasi probabilitas" yang mendorong ke kata berikutnya
        if (current_token == 0) state_vector[1] = 5.0f; // Menuju "MENYALA"
        if (current_token == 1) state_vector[2] = 5.0f; // Menuju "DUNIA"
        if (current_token == 2) state_vector[3] = 5.0f; // Menuju "[END]"
        
        // --- 2. THE TRANSFORMER PIPELINE ---
        // A. Stabilisasi Sinyal (RMSNorm)
        rmsnorm(logits, state_vector, norm_weights, 4);
        
        // B. Gerbang Logika Memori (SiLU)
        for(int i=0; i<4; i++) logits[i] = silu(logits[i]);
        
        // C. Kalkulasi Probabilitas (Softmax)
        softmax(logits, 4);
        
        // --- 3. PEMILIHAN KATA (DECODING) ---
        int next_token = argmax(logits, 4);
        
        // Jika AI memutuskan untuk berhenti
        if (next_token == 3) { // 3 adalah ID untuk "[END]"
            print_string("\n\n[System] Sinyal [END] terdeteksi. Siklus Inferensi dihentikan.\n", 0x0B);
            break; 
        }
        
        // Cetak kata yang diprediksi ke layar
        print_string(vocab[next_token], 0x0E);
        
        // THE LOOP: Kata yang baru ditebak menjadi input untuk putaran berikutnya!
        current_token = next_token;
        
        // Delay visual buatan agar kita bisa melihat mesin "berpikir" (opsional)
        for(volatile int d=0; d<10000000; d++); 
    }
}
