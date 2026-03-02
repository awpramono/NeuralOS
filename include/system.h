#ifndef SYSTEM_H
#define SYSTEM_H
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef struct { uint32_t flags; uint32_t mem_lower; uint32_t mem_upper; uint32_t boot_device; uint32_t cmdline; uint32_t mods_count; uint32_t mods_addr; } multiboot_info_t;
typedef struct { uint32_t mod_start; uint32_t mod_end; uint32_t string; uint32_t reserved; } multiboot_module_t;
typedef struct { uint32_t magic; uint32_t version; uint64_t tensor_count; uint64_t metadata_kv_count; } __attribute__((packed)) gguf_header_t;

void clear_screen();
void print_char(char c, char color);
void print_string(const char *str, char color);
void print_number(uint32_t n, char color);
void print_hex(uint32_t n, char color);
int string_compare(const char *str1, const char *str2);

float exp_float(float x);
float sqrt_float(float x);
float sin_float(float x);
float cos_float(float x);
void softmax(float *input, int size);
void rmsnorm(float* o, float* x, float* weight, int size);
void matmul(float* xout, float* x, float* w, int n, int d);
void attention(float* out, float* q, float* k, float* v, int seq_len, int d);

// FUNGSI BARU: Gerbang Logika FFN
float sigmoid(float x);
float silu(float x);

void init_heap(uint32_t start_addr);
void* mem_alloc(uint32_t size);
void enable_fpu();
void run_neural_engine(gguf_header_t *brain);
#endif
