#include "system.h"

// ============================================================================
// NeuralOS v3.0 - Bare-Metal Tiny LLM Engine
// 32-token vocabulary, 2-layer neural network (Embed + Projection)
// Weights generated at boot (eliminates disk I/O fragility)
// ============================================================================

#define VOCAB_SIZE 32
#define MAX_GEN_STEPS 10
#define END_TOKEN 31

// Dynamic core count string (filled at init)
static char core_count_str[16] = "?-core ";

// 32-token vocabulary
char* vocab[VOCAB_SIZE] = {
    /* 00 */ "Halo! ",
    /* 01 */ "Saya ",
    /* 02 */ "adalah ",
    /* 03 */ "NeuralOS, ",
    /* 04 */ "asisten ",
    /* 05 */ "AI ",
    /* 06 */ "Anda. ",
    /* 07 */ "Sistem ",
    /* 08 */ "memori: ",
    /* 09 */ "CPU: ",
    /* 10 */ core_count_str,    // filled dynamically at boot
    /* 11 */ "aktif. ",
    /* 12 */ "512MB ",
    /* 13 */ "RAM ",
    /* 14 */ "tersedia. ",
    /* 15 */ "Perintah: ",
    /* 16 */ "HELP ",
    /* 17 */ "INFO ",
    /* 18 */ "MEM ",
    /* 19 */ "CLEAR ",
    /* 20 */ "AI. ",
    /* 21 */ "Siap ",
    /* 22 */ "melayani! ",
    /* 23 */ "Neural ",
    /* 24 */ "engine ",
    /* 25 */ "berjalan ",
    /* 26 */ "di ",
    /* 27 */ "bare-metal. ",
    /* 28 */ "Ketik ",
    /* 29 */ "perintah ",
    /* 30 */ "apapun. ",
    /* 31 */ ""          // [END]
};

volatile uint32_t *ap_signal = (uint32_t *)0x9000;

// ---- WEIGHT BUFFERS (allocated in BSS, initialized at boot) ----
static float embed_w[VOCAB_SIZE * VOCAB_SIZE];
static float proj_w[VOCAB_SIZE * VOCAB_SIZE];

// ---- STATIC INFERENCE BUFFERS ----
static float state[VOCAB_SIZE];
static float logits[VOCAB_SIZE];
static float norm_w[VOCAB_SIZE];

void enable_fpu() { 
    uint32_t cr0; 
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0)); 
    cr0 &= ~(1 << 2); cr0 |= (1 << 1); cr0 |= (1 << 5); 
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0)); 
    __asm__ volatile("fninit"); 
}

int argmax(float* probabilities, int size) {
    int best_id = 0; float max_val = probabilities[0];
    for (int i = 1; i < size; i++) {
        if (probabilities[i] > max_val) { max_val = probabilities[i]; best_id = i; }
    }
    return best_id;
}

static int to_lower(int c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

static int contains(const char* haystack, const char* needle) {
    for (int i = 0; haystack[i]; i++) {
        int j = 0;
        while (needle[j] && to_lower(haystack[i+j]) == to_lower(needle[j])) j++;
        if (needle[j] == '\0') return 1;
    }
    return 0;
}

// Classify user input into a starting token
static int classify_input(const char* prompt) {
    if (contains(prompt, "HELLO") || contains(prompt, "HI") || contains(prompt, "HALO"))
        return 0;
    if (contains(prompt, "HELP") || contains(prompt, "?"))
        return 15;
    if (contains(prompt, "MEM") || contains(prompt, "RAM"))
        return 7;
    if (contains(prompt, "CPU") || contains(prompt, "CORE"))
        return 9;
    if (contains(prompt, "INFO") || contains(prompt, "STATUS"))
        return 23;
    if (contains(prompt, "AI") || contains(prompt, "SIAPA"))
        return 1;
    return 21;
}

// ============================================================================
// WEIGHT INITIALIZATION - "Training" the neural network at boot time
// This sets up the Embedding and Projection weight matrices that encode
// the language model's knowledge of token transitions.
// ============================================================================
static void set_transition(int dst, int src, float weight) {
    proj_w[dst * VOCAB_SIZE + src] = weight;
}

void init_neural_weights() {
    int d = VOCAB_SIZE;
    
    // LAYER 1: Embedding matrix (near-identity)
    // Each token gets a unique representation vector.
    // E[i][i] = 8.0 (strong self-signal), E[i][j!=i] = 0.01 (noise)
    for (int i = 0; i < d; i++) {
        for (int j = 0; j < d; j++) {
            embed_w[i * d + j] = (i == j) ? 8.0f : 0.01f;
        }
    }
    
    // LAYER 2: Projection matrix (transition knowledge)
    // P[dst][src] = HIGH means token src should be followed by token dst.
    // Initialize all to -1.0 (no transition)
    for (int i = 0; i < d * d; i++) {
        proj_w[i] = -1.0f;
    }
    
    // Chain 0 (HELLO): Halo! -> Saya -> adalah -> NeuralOS, -> asisten -> AI -> Anda.
    // Transitions: 0->1, 1->2, 2->3, 3->4, 4->5, 5->6
    set_transition(1, 0, 10.0f);
    set_transition(2, 1, 10.0f);
    set_transition(3, 2, 10.0f);
    set_transition(4, 3, 10.0f);
    set_transition(5, 4, 10.0f);
    set_transition(6, 5, 10.0f);
    
    // Chain 7 (MEMORY): Sistem -> memori: -> 512MB -> RAM -> tersedia.
    // Transitions: 7->8, 8->12, 12->13, 13->14
    set_transition(8, 7, 10.0f);
    set_transition(12, 8, 10.0f);
    set_transition(13, 12, 10.0f);
    set_transition(14, 13, 10.0f);
    
    // Chain 9 (CPU): CPU: -> 2-core -> aktif.
    // Transitions: 9->10, 10->11
    set_transition(10, 9, 10.0f);
    set_transition(11, 10, 10.0f);
    
    // Chain 15 (HELP): Perintah: -> HELP -> INFO -> MEM -> CLEAR -> AI.
    // Transitions: 15->16, 16->17, 17->18, 18->19, 19->20
    set_transition(16, 15, 10.0f);
    set_transition(17, 16, 10.0f);
    set_transition(18, 17, 10.0f);
    set_transition(19, 18, 10.0f);
    set_transition(20, 19, 10.0f);
    
    // Chain 21 (DEFAULT): Siap -> melayani! -> Ketik -> perintah -> apapun.
    // Transitions: 21->22, 22->28, 28->29, 29->30
    set_transition(22, 21, 10.0f);
    set_transition(28, 22, 10.0f);
    set_transition(29, 28, 10.0f);
    set_transition(30, 29, 10.0f);
    
    // Chain 23 (INFO): Neural -> engine -> berjalan -> di -> bare-metal. -> 2-core -> aktif.
    // Transitions: 23->24, 24->25, 25->26, 26->27, 27->10, (10->11 already set)
    set_transition(24, 23, 10.0f);
    set_transition(25, 24, 10.0f);
    set_transition(26, 25, 10.0f);
    set_transition(27, 26, 10.0f);
    set_transition(10, 27, 10.0f);   // reuses 10->11 from CPU chain
    
    // END TOKEN (31): Terminate after the last token of each chain
    // P[END][last_real_token] = 10.0
    set_transition(END_TOKEN, 6, 10.0f);    // end of HELLO/ABOUT-AI chains
    set_transition(END_TOKEN, 14, 10.0f);   // end of MEMORY chain
    set_transition(END_TOKEN, 11, 10.0f);   // end of CPU/INFO chains
    set_transition(END_TOKEN, 20, 10.0f);   // end of HELP chain
    set_transition(END_TOKEN, 30, 10.0f);   // end of DEFAULT chain
    
    // Conflict resolution: token 22 has both END signal (if shared) 
    // and continuation (22->28). Boost continuation.
    // Token 11 is end for both CPU and INFO, no conflict (always terminal).
    // No active conflicts in current chain design.
    
    // Initialize normalization weights
    for (int i = 0; i < d; i++) {
        norm_w[i] = 1.0f;
    }
    
    // Build dynamic core count string from SMP mailbox
    uint32_t nc = *(volatile uint32_t*)0x9018;
    if (nc < 1) nc = 1;
    if (nc < 10) {
        core_count_str[0] = '0' + (char)nc;
        core_count_str[1] = '-'; core_count_str[2] = 'c'; core_count_str[3] = 'o';
        core_count_str[4] = 'r'; core_count_str[5] = 'e'; core_count_str[6] = ' ';
        core_count_str[7] = '\0';
    }
    
    print_string("[AI] Neural weights initialized: ", 0x0A);
    print_number(d, 0x0E);
    print_string("x", 0x0A);
    print_number(d, 0x0E);
    print_string(" embed + proj matrices\n", 0x0A);
}

// ============================================================================
// INFERENCE ENGINE
// ============================================================================
void run_neural_engine(gguf_header_t *brain_ptr, const char* prompt) {
    (void)brain_ptr; // GGUF validated at boot, weights are in-memory
    
    int d = VOCAB_SIZE;

    // ---- CLASSIFY INPUT ----
    int current_token = classify_input(prompt);

    print_string("AI   > ", 0x0D); 
    print_string(vocab[current_token], 0x0F); 

    // ---- AUTOREGRESSIVE INFERENCE LOOP ----
    for (int step = 0; step < MAX_GEN_STEPS; step++) {
        // Layer 1: Embedding lookup
        for(int i = 0; i < d; i++) {
            state[i] = embed_w[current_token * d + i];
        }

        // Layer 2: RMSNorm (normalization)
        rmsnorm(logits, state, norm_w, d);
        for(int i = 0; i < d; i++) state[i] = logits[i];

        // Layer 3: N-WAY PARALLEL matmul across all CPU cores
        // With 4 cores: each core computes 8 rows (32/4)
        {
            uint32_t num_cores = *(volatile uint32_t*)0x9018;
            if (num_cores < 1) num_cores = 1;
            
            // Set up SMP mailbox parameters for worker cores
            *(float* volatile*)0x9004 = logits;   // xout
            *(float* volatile*)0x9008 = state;    // x
            *(float* volatile*)0x900C = proj_w;   // w
            *(volatile uint32_t*)0x9010 = (uint32_t)d;  // n
            *(volatile uint32_t*)0x9014 = (uint32_t)d;  // d
            
            // Reset workers_done counter
            *(volatile uint32_t*)0x9020 = 0;
            
            // Signal ALL worker cores to start
            *ap_signal = 0x1;
            
            // Core 0 computes its share (rows 0, num_cores, 2*num_cores, ...)
            matmul_parallel(logits, state, proj_w, d, d, 0, (int)num_cores);
            
            // Wait for all worker cores to finish
            if (num_cores > 1) {
                while(*(volatile uint32_t*)0x9020 < num_cores - 1);
            }
            
            // Reset signal (releases workers from phase-2 wait)
            *ap_signal = 0x0;
        }

        // Layer 4: SiLU activation
        for(int i = 0; i < d; i++) logits[i] = silu(logits[i]);

        // Layer 5: Softmax probability distribution
        softmax(logits, d);

        // Layer 6: Greedy decode (argmax)
        int next_token = argmax(logits, d);
        if (next_token == END_TOKEN) break;

        print_string(vocab[next_token], 0x0F);
        current_token = next_token;

        // Typing delay effect
        for(volatile int t = 0; t < 15000000; t++);
    }
    print_string("\n", 0x07);
}
