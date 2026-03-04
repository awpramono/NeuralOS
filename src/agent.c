#include "system.h"

// ============================================================================
// NeuralOS Agent Layer — AI-Driven Intent Classification & Command Dispatch
// Transforms the OS from passive shell → intelligent reasoning engine
// ============================================================================

// Session state
static uint32_t g_query_count = 0;

// ----------------------------------------------------------------------------
// Intent types — what the user wants the OS to do

typedef enum {
    INTENT_SYSTEM_MEM,      // Query memory status
    INTENT_SYSTEM_CPU,      // Query CPU/core info
    INTENT_SYSTEM_INFO,     // General system info
    INTENT_SYSTEM_HELP,     // Help/commands
    INTENT_SYSTEM_CLEAR,    // Clear screen
    INTENT_SYSTEM_VER,      // Version info
    INTENT_AI_STORY,        // Generate a story (creative)
    INTENT_AI_CHAT,         // General conversation
    INTENT_AI_EXPLAIN,      // Explain something
    INTENT_AI_CODE,         // Code/technical question
    INTENT_UNKNOWN          // Couldn't classify
} IntentType;

// Intent result with confidence
typedef struct {
    IntentType type;
    int confidence;  // 0-100
    const char *reason;
} IntentResult;

// ----------------------------------------------------------------------------
// Keyword patterns for intent classification

typedef struct {
    const char *keyword;
    IntentType intent;
    int weight;  // higher = stronger signal
} KeywordRule;

// Case-insensitive substring search
static int contains_ci(const char *haystack, const char *needle) {
    for (int i = 0; haystack[i]; i++) {
        int match = 1;
        for (int j = 0; needle[j]; j++) {
            char h = haystack[i+j];
            char n = needle[j];
            if (h >= 'a' && h <= 'z') h -= 32;
            if (n >= 'a' && n <= 'z') n -= 32;
            if (h != n) { match = 0; break; }
        }
        if (match) return 1;
    }
    return 0;
}

// Keyword rules table — ordered by specificity
static const KeywordRule RULES[] = {
    // Memory queries
    {"memory",    INTENT_SYSTEM_MEM,  90},
    {"memori",    INTENT_SYSTEM_MEM,  90},
    {"ram",       INTENT_SYSTEM_MEM,  85},
    {"heap",      INTENT_SYSTEM_MEM,  80},
    {"alokasi",   INTENT_SYSTEM_MEM,  75},
    {"berapa mem",INTENT_SYSTEM_MEM,  95},

    // CPU queries
    {"cpu",       INTENT_SYSTEM_CPU,  85},
    {"core",      INTENT_SYSTEM_CPU,  80},
    {"prosesor",  INTENT_SYSTEM_CPU,  85},
    {"processor", INTENT_SYSTEM_CPU,  85},
    {"smp",       INTENT_SYSTEM_CPU,  80},
    {"multicore", INTENT_SYSTEM_CPU,  85},

    // System info
    {"info",      INTENT_SYSTEM_INFO, 70},
    {"system",    INTENT_SYSTEM_INFO, 60},
    {"sistem",    INTENT_SYSTEM_INFO, 60},
    {"status",    INTENT_SYSTEM_INFO, 65},
    {"spesifikasi", INTENT_SYSTEM_INFO, 80},

    // Help
    {"help",      INTENT_SYSTEM_HELP, 90},
    {"tolong",    INTENT_SYSTEM_HELP, 70},
    {"bantuan",   INTENT_SYSTEM_HELP, 85},
    {"perintah",  INTENT_SYSTEM_HELP, 75},
    {"command",   INTENT_SYSTEM_HELP, 80},
    {"apa saja",  INTENT_SYSTEM_HELP, 70},

    // Clear
    {"clear",     INTENT_SYSTEM_CLEAR, 95},
    {"bersihkan", INTENT_SYSTEM_CLEAR, 90},
    {"cls",       INTENT_SYSTEM_CLEAR, 95},

    // Version
    {"version",   INTENT_SYSTEM_VER, 90},
    {"versi",     INTENT_SYSTEM_VER, 90},
    {"ver",       INTENT_SYSTEM_VER, 85},

    // Story/Creative
    {"story",     INTENT_AI_STORY, 90},
    {"cerita",    INTENT_AI_STORY, 90},
    {"dongeng",   INTENT_AI_STORY, 95},
    {"once upon", INTENT_AI_STORY, 95},
    {"tell me",   INTENT_AI_STORY, 80},
    {"tulis",     INTENT_AI_STORY, 70},
    {"write",     INTENT_AI_STORY, 70},

    // Code/Technical
    {"code",      INTENT_AI_CODE, 80},
    {"kode",      INTENT_AI_CODE, 80},
    {"program",   INTENT_AI_CODE, 75},
    {"compile",   INTENT_AI_CODE, 85},
    {"debug",     INTENT_AI_CODE, 85},
    {"function",  INTENT_AI_CODE, 75},
    {"algorithm", INTENT_AI_CODE, 85},

    // Explain
    {"explain",   INTENT_AI_EXPLAIN, 85},
    {"jelaskan",  INTENT_AI_EXPLAIN, 85},
    {"apa itu",   INTENT_AI_EXPLAIN, 90},
    {"what is",   INTENT_AI_EXPLAIN, 90},
    {"how does",  INTENT_AI_EXPLAIN, 85},
    {"bagaimana", INTENT_AI_EXPLAIN, 75},
    {"mengapa",   INTENT_AI_EXPLAIN, 80},
    {"why",       INTENT_AI_EXPLAIN, 80},
};

#define NUM_RULES (int)(sizeof(RULES) / sizeof(RULES[0]))

// ----------------------------------------------------------------------------
// Intent Classifier — multi-signal hybrid

IntentResult classify_intent(const char *input) {
    IntentResult result = { INTENT_AI_CHAT, 30, "default: general chat" };

    // Score each intent based on keyword matches
    int scores[11] = {0};
    const char *reasons[11] = {0};

    for (int r = 0; r < NUM_RULES; r++) {
        if (contains_ci(input, RULES[r].keyword)) {
            IntentType t = RULES[r].intent;
            if (RULES[r].weight > scores[t]) {
                scores[t] = RULES[r].weight;
                reasons[t] = RULES[r].keyword;
            }
        }
    }

    // Find highest scoring intent
    int best_score = 0;
    IntentType best_intent = INTENT_AI_CHAT;
    for (int i = 0; i < 11; i++) {
        if (scores[i] > best_score) {
            best_score = scores[i];
            best_intent = (IntentType)i;
            result.reason = reasons[i];
        }
    }

    // Question patterns boost system intents
    if (contains_ci(input, "berapa") || contains_ci(input, "how much") ||
        contains_ci(input, "how many") || contains_ci(input, "seberapa")) {
        if (best_intent >= INTENT_SYSTEM_MEM && best_intent <= INTENT_SYSTEM_INFO) {
            best_score += 15;
        }
    }

    result.type = best_intent;
    result.confidence = best_score > 100 ? 100 : best_score;

    // If no strong match, default to chat
    if (best_score < 40) {
        result.type = INTENT_AI_CHAT;
        result.confidence = 50;
        result.reason = "no strong match";
    }

    return result;
}

// ----------------------------------------------------------------------------
// System State Reporter — provides live data for AI context

static void report_memory_state() {
    uint32_t heap_used = get_heap_usage() - 0x1000000;
    print_string("AI > ", 0x0D);
    print_string("[System Analysis] ", 0x0B);
    print_string("Memory Report:\n", 0x0F);
    print_string("      Total RAM    : 512 MB\n", 0x0F);
    print_string("      Heap used    : ", 0x0F);
    print_number(heap_used / 1024, 0x0E);
    print_string(" KB\n", 0x0F);
    print_string("      Heap free    : ~", 0x0F);
    print_number((512*1024) - (heap_used/1024), 0x0E);
    print_string(" KB\n", 0x0F);
    print_string("      Allocator    : Bump (linear)\n", 0x0F);
    print_string("      AI Model RAM : ~", 0x0F);
    print_number(heap_used > 1024*1024 ? (heap_used - 100*1024)/1024 : 0, 0x0E);
    print_string(" KB\n", 0x0F);
}

static void report_cpu_state() {
    volatile uint32_t *mailbox = (volatile uint32_t*)0x9000;
    uint32_t num_cores = mailbox[6];  // offset 0x18 = cores online
    if (num_cores == 0) num_cores = 1;

    print_string("AI > ", 0x0D);
    print_string("[System Analysis] ", 0x0B);
    print_string("CPU Report:\n", 0x0F);
    print_string("      Architecture : x86 32-bit (i386)\n", 0x0F);
    print_string("      Cores online : ", 0x0F);
    print_number(num_cores, 0x0E);
    print_string("\n", 0x0F);
    print_string("      SMP          : Active (APIC IPI)\n", 0x0F);
    print_string("      FPU          : x87 (80-bit extended)\n", 0x0F);
    print_string("      MatMul       : Parallel (", 0x0F);
    print_number(num_cores, 0x0E);
    print_string("-way stride)\n", 0x0F);
}

static void report_ai_state() {
    int vs = llama_get_vocab_size();
    print_string("AI > ", 0x0D);
    print_string("[System Analysis] ", 0x0B);
    print_string("AI Engine Report:\n", 0x0F);

    if (llama_is_loaded()) {
        print_string("      Transformer  : Llama2 (active)\n", 0x0F);
        print_string("      Vocab Size   : ", 0x0F);
        print_number(vs, 0x0E);
        print_string(" tokens\n", 0x0F);
        print_string("      Architecture : RoPE + GQA + SwiGLU\n", 0x0F);
        print_string("      Math Backend : x87 FPU hardware\n", 0x0F);
    } else {
        print_string("      Engine       : 32-token (fast)\n", 0x0F);
    }
    print_string("      Sampling     : Temperature 0.8\n", 0x0F);
    print_string("      Max Tokens   : 256\n", 0x0F);
}

static void report_full_system() {
    report_cpu_state();
    report_memory_state();
    report_ai_state();
}

// ----------------------------------------------------------------------------
// Agent Dispatch — execute the classified intent

void agent_dispatch(const char *input) {
    g_query_count++;
    IntentResult intent = classify_intent(input);

    // Debug: show classification + query number
    print_string("[Agent #", 0x08);
    print_number(g_query_count, 0x08);
    print_string("] Intent: ", 0x08);
    switch (intent.type) {
        case INTENT_SYSTEM_MEM:  print_string("SYSTEM_MEM", 0x08); break;
        case INTENT_SYSTEM_CPU:  print_string("SYSTEM_CPU", 0x08); break;
        case INTENT_SYSTEM_INFO: print_string("SYSTEM_INFO", 0x08); break;
        case INTENT_SYSTEM_HELP: print_string("SYSTEM_HELP", 0x08); break;
        case INTENT_SYSTEM_CLEAR:print_string("SYSTEM_CLEAR", 0x08); break;
        case INTENT_SYSTEM_VER:  print_string("SYSTEM_VER", 0x08); break;
        case INTENT_AI_STORY:    print_string("AI_STORY", 0x08); break;
        case INTENT_AI_CHAT:     print_string("AI_CHAT", 0x08); break;
        case INTENT_AI_EXPLAIN:  print_string("AI_EXPLAIN", 0x08); break;
        case INTENT_AI_CODE:     print_string("AI_CODE", 0x08); break;
        default:                 print_string("UNKNOWN", 0x08); break;
    }
    print_string(" (", 0x08);
    print_number(intent.confidence, 0x08);
    print_string("%) matched: ", 0x08);
    if (intent.reason) print_string(intent.reason, 0x08);
    print_string("\n", 0x08);

    // Execute based on intent
    switch (intent.type) {
        case INTENT_SYSTEM_MEM:
            report_memory_state();
            break;

        case INTENT_SYSTEM_CPU:
            report_cpu_state();
            break;

        case INTENT_SYSTEM_INFO:
            report_full_system();
            break;

        case INTENT_SYSTEM_HELP:
            print_string("AI > ", 0x0D);
            print_string("Here are the things I can help with:\n", 0x0F);
            print_string("      System: memory, CPU, info, version, clear\n", 0x0F);
            print_string("      AI:     story, chat, explain, code\n", 0x0F);
            print_string("      Just talk to me naturally!\n", 0x0E);
            break;

        case INTENT_SYSTEM_CLEAR:
            clear_screen();
            update_status_bar();
            print_string("NeuralOS v3.5 — AI Agent Mode\n", 0x0B);
            break;

        case INTENT_SYSTEM_VER:
            print_string("AI > ", 0x0D);
            print_string("NeuralOS v3.5 Agentic Edition\n", 0x0F);
            print_string("      Kernel : Bare-metal x86 + Llama2 Transformer\n", 0x0F);
            print_string("      AI     : 264K params, Q8 quantization, 4-core SMP\n", 0x0F);
            print_string("      Built  : Pure C + x87 ASM, no stdlib\n", 0x0F);
            break;

        case INTENT_AI_STORY:
            if (llama_is_loaded()) {
                print_string("[Agent -> Llama2 Creative Engine]\n", 0x0D);
                llama_generate_with_prompt(input, 256);
            } else {
                print_string("AI > ", 0x0D);
                print_string("Story engine not loaded. Use: make run-llama\n", 0x0C);
            }
            break;

        case INTENT_AI_CODE:
        case INTENT_AI_EXPLAIN:
            // Route to Llama2 if loaded, with context prefix
            if (llama_is_loaded()) {
                print_string("[Agent -> Llama2 Analytical Engine]\n", 0x0D);
                llama_generate_with_prompt(input, 256);
            } else {
                run_neural_engine((gguf_header_t*)0, input);
            }
            break;

        case INTENT_AI_CHAT:
        default:
            // General chat: use Llama2 if loaded, else 32-token engine
            if (llama_is_loaded()) {
                llama_generate_with_prompt(input, 256);
            } else {
                run_neural_engine((gguf_header_t*)0, input);
            }
            break;
    }
}
