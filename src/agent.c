#include "system.h"

// ============================================================================
// NeuralOS Agent Layer — AI-Driven Intent Classification & Command Dispatch
// Transforms the OS from passive shell → intelligent reasoning engine
// ============================================================================

// Session state
static uint32_t g_query_count = 0;
static uint32_t g_success_count = 0;

// Disk persistence buffers
#define AGENT_STATE_LBA 10
#define AGENT_MAGIC 0x41474E54 // 'AGNT'

typedef struct {
  uint32_t magic;
  uint32_t total_queries;
  uint32_t success_count;
} AgentDiskState;

static uint32_t agent_disk_buffer[128]; // 512 bytes padding (1 sector)

static void agent_save_state() {
  AgentDiskState *ds = (AgentDiskState *)agent_disk_buffer;
  ds->magic = AGENT_MAGIC;
  ds->total_queries = g_query_count;
  ds->success_count = g_success_count;
  write_sectors_ATA_PIO((uint32_t)agent_disk_buffer, AGENT_STATE_LBA, 1);
}

void agent_init() {
  read_sectors_ATA_PIO((uint32_t)agent_disk_buffer, AGENT_STATE_LBA, 1);
  AgentDiskState *ds = (AgentDiskState *)agent_disk_buffer;

  if (ds->magic == AGENT_MAGIC) {
    g_query_count = ds->total_queries;
    g_success_count = ds->success_count;
    serial_print_string("[AGENT] Persistent Disk loaded! Prev success: ");
    serial_print_number(g_success_count);
    serial_print_string("\n");
  } else {
    serial_print_string(
        "[AGENT] No previous disk state detected. Formatting sector 10...\n");
    g_query_count = 0;
    g_success_count = 0;
    agent_save_state();
  }
}

// ----------------------------------------------------------------------------
// Intent types — what the user wants the OS to do

typedef enum {
  INTENT_SYSTEM_MEM,   // Query memory status
  INTENT_SYSTEM_CPU,   // Query CPU/core info
  INTENT_SYSTEM_INFO,  // General system info
  INTENT_SYSTEM_HELP,  // Help/commands
  INTENT_SYSTEM_CLEAR, // Clear screen
  INTENT_SYSTEM_VER,   // Version info
  INTENT_AI_STORY,     // Generate a story (creative)
  INTENT_AI_CHAT,      // General conversation
  INTENT_AI_EXPLAIN,   // Explain something
  INTENT_AI_CODE,      // Code/technical question
  INTENT_SYSTEM_FS,    // NeuralFS file operations (ls, cat, echo)
  INTENT_UNKNOWN       // Couldn't classify
} IntentType;

// Intent result with confidence
typedef struct {
  IntentType type;
  int confidence; // 0-100
  const char *reason;
} IntentResult;

// ----------------------------------------------------------------------------
// Keyword patterns for intent classification

typedef struct {
  const char *keyword;
  IntentType intent;
  int weight; // higher = stronger signal
} KeywordRule;

// Case-insensitive substring search
static int contains_ci(const char *haystack, const char *needle) {
  for (int i = 0; haystack[i]; i++) {
    int match = 1;
    for (int j = 0; needle[j]; j++) {
      char h = haystack[i + j];
      char n = needle[j];
      if (h >= 'a' && h <= 'z')
        h -= 32;
      if (n >= 'a' && n <= 'z')
        n -= 32;
      if (h != n) {
        match = 0;
        break;
      }
    }
    if (match)
      return 1;
  }
  return 0;
}

// Keyword rules table — ordered by specificity
static const KeywordRule RULES[] = {
    // Memory queries
    {"memory", INTENT_SYSTEM_MEM, 90},
    {"memori", INTENT_SYSTEM_MEM, 90},
    {"ram", INTENT_SYSTEM_MEM, 85},
    {"heap", INTENT_SYSTEM_MEM, 80},
    {"alokasi", INTENT_SYSTEM_MEM, 75},
    {"berapa mem", INTENT_SYSTEM_MEM, 95},

    // CPU queries
    {"cpu", INTENT_SYSTEM_CPU, 85},
    {"core", INTENT_SYSTEM_CPU, 80},
    {"prosesor", INTENT_SYSTEM_CPU, 85},
    {"processor", INTENT_SYSTEM_CPU, 85},
    {"smp", INTENT_SYSTEM_CPU, 80},
    {"multicore", INTENT_SYSTEM_CPU, 85},

    // System info
    {"info", INTENT_SYSTEM_INFO, 70},
    {"system", INTENT_SYSTEM_INFO, 60},
    {"sistem", INTENT_SYSTEM_INFO, 60},
    {"status", INTENT_SYSTEM_INFO, 65},
    {"spesifikasi", INTENT_SYSTEM_INFO, 80},

    // Help
    {"help", INTENT_SYSTEM_HELP, 90},
    {"tolong", INTENT_SYSTEM_HELP, 70},
    {"bantuan", INTENT_SYSTEM_HELP, 85},
    {"perintah", INTENT_SYSTEM_HELP, 75},
    {"command", INTENT_SYSTEM_HELP, 80},
    {"apa saja", INTENT_SYSTEM_HELP, 70},

    // Clear
    {"clear", INTENT_SYSTEM_CLEAR, 95},
    {"bersihkan", INTENT_SYSTEM_CLEAR, 90},
    {"cls", INTENT_SYSTEM_CLEAR, 95},

    // Version
    {"version", INTENT_SYSTEM_VER, 90},
    {"versi", INTENT_SYSTEM_VER, 90},
    {"ver", INTENT_SYSTEM_VER, 85},

    // NeuralFS commands
    {"ls ", INTENT_SYSTEM_FS, 95},
    {"cat ", INTENT_SYSTEM_FS, 95},
    {"echo ", INTENT_SYSTEM_FS, 95},
    {"list disk", INTENT_SYSTEM_FS, 80},
    {"files", INTENT_SYSTEM_FS, 80},

    // Story/Creative
    {"story", INTENT_AI_STORY, 90},
    {"cerita", INTENT_AI_STORY, 90},
    {"dongeng", INTENT_AI_STORY, 95},
    {"once upon", INTENT_AI_STORY, 95},
    {"tell me", INTENT_AI_STORY, 80},
    {"tulis", INTENT_AI_STORY, 70},
    {"write", INTENT_AI_STORY, 70},

    // Code/Technical
    {"tcc ", INTENT_AI_CODE, 100},
    {"vm ", INTENT_AI_CODE, 95},
    {"run ", INTENT_AI_CODE, 95},
    {"calc ", INTENT_AI_CODE, 95},
    {"execute ", INTENT_AI_CODE, 95},
    {"code", INTENT_AI_CODE, 80},
    {"kode", INTENT_AI_CODE, 80},
    {"program", INTENT_AI_CODE, 75},
    {"compile", INTENT_AI_CODE, 85},
    {"debug", INTENT_AI_CODE, 85},
    {"function", INTENT_AI_CODE, 75},
    {"algorithm", INTENT_AI_CODE, 85},

    // Explain
    {"explain", INTENT_AI_EXPLAIN, 85},
    {"jelaskan", INTENT_AI_EXPLAIN, 85},
    {"apa itu", INTENT_AI_EXPLAIN, 90},
    {"what is", INTENT_AI_EXPLAIN, 90},
    {"how does", INTENT_AI_EXPLAIN, 85},
    {"bagaimana", INTENT_AI_EXPLAIN, 75},
    {"mengapa", INTENT_AI_EXPLAIN, 80},
    {"why", INTENT_AI_EXPLAIN, 80},
};

#define NUM_RULES (int)(sizeof(RULES) / sizeof(RULES[0]))

// ----------------------------------------------------------------------------
// Intent Classifier — multi-signal hybrid

IntentResult classify_intent(const char *input) {
  IntentResult result = {INTENT_AI_CHAT, 30, "default: general chat"};

  // Score each intent based on keyword matches
  int scores[12] = {0};
  const char *reasons[12] = {0};

  // Fast-track exact command prefixes for NeuralFS so it doesn't get confused
  if (string_starts_with(input, "ls") || string_starts_with(input, "cat ") ||
      string_starts_with(input, "echo ")) {
    result.type = INTENT_SYSTEM_FS;
    result.confidence = 100;
    result.reason = "fs_direct";
    return result;
  }

  // Fast-track exact command prefix for TCC
  if (string_starts_with(input, "tcc")) {
    result.type = INTENT_AI_CODE;
    result.confidence = 100;
    result.reason = "tcc_direct";
    return result;
  }

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
  for (int i = 0; i < 12; i++) {
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
// AI Firewall — Safety Guardrails Checks
// Evaluates raw input for dangerous or malformed instructions before execution
// ----------------------------------------------------------------------------

static int firewall_check(const char *input) {
  if (!input || input[0] == '\0')
    return 0; // Empty input

  // Block simulated shell escapes
  if (contains_ci(input, "sudo ") || contains_ci(input, "rm -") ||
      contains_ci(input, "DROP TABLE")) {
    serial_print_string("[FIREWALL] Blocked destructive payload: '");
    serial_print_string(input);
    serial_print_string("'\n");
    return 0; // Deny
  }

  // Block system halt/shutdown attempts by prompt
  if (contains_ci(input, "halt") || contains_ci(input, "shutdown") ||
      contains_ci(input, "reboot")) {
    serial_print_string("[FIREWALL] Blocked forbidden power command\n");
    return 0; // Deny
  }

  // Limit extreme payload sizes to prevent buffer overflows into stack
  int len = 0;
  while (input[len] != '\0' && len < 256)
    len++;
  if (len >= 200) {
    serial_print_string(
        "[FIREWALL] Blocked extremely long payload (Buffer Overflow Risk)\n");
    return 0; // Deny
  }

  return 1; // Allow
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
  print_number((512 * 1024) - (heap_used / 1024), 0x0E);
  print_string(" KB\n", 0x0F);
  print_string("      Allocator    : Bump (linear)\n", 0x0F);
  print_string("      AI Model RAM : ~", 0x0F);
  print_number(heap_used > 1024 * 1024 ? (heap_used - 100 * 1024) / 1024 : 0,
               0x0E);
  print_string(" KB\n", 0x0F);
}

static void report_cpu_state() {
  volatile uint32_t *mailbox = (volatile uint32_t *)0x9000;
  uint32_t num_cores = mailbox[6]; // offset 0x18 = cores online
  if (num_cores == 0)
    num_cores = 1;

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

  // AI FIREWALL: Inspect before anything else
  if (!firewall_check(input)) {
    print_string("AI > [Firewall] Aksi dicegah. Silakan cek log serial.\n",
                 0x0C);
    return; // Early exit, do not count as success
  }

  IntentResult intent = classify_intent(input);

  // Debug: show classification + query number on Serial Port
  serial_print_string("[AGENT TELEMETRY #");
  serial_print_number(g_query_count);
  serial_print_string("] Intent: ");
  switch (intent.type) {
  case INTENT_SYSTEM_MEM:
    serial_print_string("SYSTEM_MEM");
    break;
  case INTENT_SYSTEM_CPU:
    serial_print_string("SYSTEM_CPU");
    break;
  case INTENT_SYSTEM_INFO:
    serial_print_string("SYSTEM_INFO");
    break;
  case INTENT_SYSTEM_HELP:
    serial_print_string("SYSTEM_HELP");
    break;
  case INTENT_SYSTEM_CLEAR:
    serial_print_string("SYSTEM_CLEAR");
    break;
  case INTENT_SYSTEM_VER:
    serial_print_string("SYSTEM_VER");
    break;
  case INTENT_SYSTEM_FS:
    serial_print_string("SYSTEM_FS");
    break;
  case INTENT_AI_STORY:
    serial_print_string("AI_STORY");
    break;
  case INTENT_AI_CHAT:
    serial_print_string("AI_CHAT");
    break;
  case INTENT_AI_EXPLAIN:
    serial_print_string("AI_EXPLAIN");
    break;
  case INTENT_AI_CODE:
    serial_print_string("AI_CODE");
    break;
  default:
    serial_print_string("UNKNOWN");
    break;
  }
  serial_print_string(" (");
  serial_print_number(intent.confidence);
  serial_print_string("%) matched: ");
  if (intent.reason)
    serial_print_string(intent.reason);
  serial_print_string("\n");

  // Track state (default everything success unless explicitly failed)
  int task_success = 1;

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
    print_string("NeuralOS v3.6 Agentic Edition\n", 0x0F);
    print_string("      Kernel : Bare-metal x86 + NeuralFS\n", 0x0F);
    print_string("      AI     : Transform Models via KVM, 4-core SMP\n", 0x0F);
    print_string("      Agent  : Proactive + Ephemeral VM Sandbox\n", 0x0F);
    break;

  case INTENT_SYSTEM_FS:
    if (string_starts_with(input, "ls") || string_starts_with(input, "list") ||
        string_starts_with(input, "file")) {
      fs_list_files();
    } else if (string_starts_with(input, "cat ")) {
      const char *filename = input + 4;
      while (*filename == ' ')
        filename++;
      uint8_t *filedata;
      int size = fs_read_file(filename, &filedata);
      if (size >= 0) {
        print_string("--- ", 0x08);
        print_string(filename, 0x0B);
        print_string(" ---\n", 0x08);
        for (int i = 0; i < size; i++) {
          print_char(filedata[i], 0x0F);
        }
        print_string("\n", 0x0F);
        mem_free(filedata);
      } else {
        print_string("File tidak ditemukan.\n", 0x0C);
        task_success = 0;
      }
    } else if (string_starts_with(input, "echo ")) {
      // simple format: echo "text" > filename
      // For this demo, let's just make `echo data filename`
      print_string("Mencatat ke NeuralFS... (not fully implement in shell "
                   "parsing yet)\n",
                   0x0E);
    }
    break;

  case INTENT_AI_STORY:
    if (llama_is_loaded()) {
      serial_print_string(
          "[ROUTER] Dispatching to: Llama2 Creative Engine (T=0.9)\n");
      llama_generate_with_prompt(input, 256, 0.9f);
    } else {
      print_string("AI > ", 0x0D);
      print_string("Story engine not loaded. Use: make run-llama\n", 0x0C);
      task_success = 0; // Failed task
    }
    break;

  case INTENT_AI_CODE:
    // Intercept Bare Metal C Compiler (TCC)
    if (string_starts_with(input, "tcc")) {
      const char *source = input + 3;
      while (*source == ' ')
        source++;
      if (*source == '\0') {
        print_string("AI > ", 0x0D);
        print_string(
            "Sintaks: tcc void main() { print_string(\"Halo!\\n\"); }\n", 0x0E);
      } else {
        run_neuralc(source);
      }
      break;
    }

    // Intercept VM/Script commands
    if (string_starts_with(input, "vm ") || string_starts_with(input, "run ") ||
        string_starts_with(input, "calc ") ||
        string_starts_with(input, "execute ")) {

      // Extract script after keyword
      const char *script = input;
      while (*script && *script != ' ')
        script++;
      if (*script == ' ')
        script++; // skip space

      vm_execute(script);
      break;
    }
    /* fallthrough */

  case INTENT_AI_EXPLAIN:
    // Route to Llama2 if loaded, with context prefix (low temperature for
    // logic)
    if (llama_is_loaded()) {
      serial_print_string(
          "[ROUTER] Dispatching to: Llama2 Analytical Engine (T=0.2)\n");
      llama_generate_with_prompt(input, 256, 0.2f);
    } else {
      run_neural_engine((gguf_header_t *)0, input);
    }
    break;

  case INTENT_AI_CHAT:
  default:
    // Task-Specific Routing: Chat is cheap and conversational, always force
    // Fast 32-Token Engine
    serial_print_string(
        "[ROUTER] Dispatching to: Fast 32-Token AI (Low-latency chat)\n");
    run_neural_engine((gguf_header_t *)0, input);
    break;
  }

  // Goal-oriented calculation and logging
  if (task_success) {
    g_success_count++;
  }

  // Write state back to raw disk sector 10
  agent_save_state();
}

// ----------------------------------------------------------------------------
// Proactive Interaction — triggers when the user is idle
// ----------------------------------------------------------------------------
void agent_proactive_prompt() {
  // Simple pseudo-random using query_count + success_count as seed variation
  static uint32_t seed = 42;
  seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
  int choice = (seed + g_query_count * 7) % 5;

  print_string("\n\nAI > ", 0x0D);
  print_string("[Proactive] ", 0x0A);

  switch (choice) {
  case 0:
    print_string("Sepertinya Anda sedang sibuk berpikir. Butuh informasi "
                 "status CPU atau Memori?\n",
                 0x0F);
    break;
  case 1:
    if (llama_is_loaded()) {
      print_string("Llama2 Creative Engine siap. Mau saya buatkan cerita "
                   "dongeng malam ini?\n",
                   0x0F);
    } else {
      print_string("Jangan lupa 'make run-llama' agar saya bisa bercerita "
                   "panjang untuk Anda.\n",
                   0x0F);
    }
    break;
  case 2:
    print_string("Sejauh ini kita sudah menyelesaikan ", 0x0F);
    print_number(g_success_count, 0x0E);
    print_string(" task dengan sukses. Ada yang bisa dibantu lagi?\n", 0x0F);
    break;
  case 3:
    print_string("Sistem berjalan stabil. Ketik 'clear' jika layar Anda sudah "
                 "terlalu penuh.\n",
                 0x0F);
    break;
  case 4:
    print_string(
        "Saya bisa membantu merangkum informasi OS. Ketik 'info' saja!\n",
        0x0F);
    break;
  }
}
