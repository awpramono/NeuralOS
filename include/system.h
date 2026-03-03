#ifndef SYSTEM_H
#define SYSTEM_H

// 1. Basic Data Types
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef long long          int64_t;
typedef int                int32_t;
typedef short              int16_t;
typedef signed char        int8_t;
typedef uint32_t           size_t;
typedef int32_t            ssize_t;

#define NULL ((void*)0)

// 2. Multiboot Structures
typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
} multiboot_info_t;

typedef struct {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
} multiboot_module_t;

// 3. GGUF Header Structure
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t tensor_count;
    uint64_t metadata_kv_count;
} __attribute__((packed)) gguf_header_t;

// 4. VGA & String Interaction
void clear_screen();
void print_char(char c, char color);
void print_string(const char *str, char color);
void print_number(uint32_t n, char color);
void print_hex(uint32_t n, char color);
int string_compare(const char *str1, const char *str2);

// 5. Memory Management
void init_heap(uint32_t start_addr);
void* mem_alloc(uint32_t size);
void* mem_calloc(uint32_t count, uint32_t size);
void mem_free(void *ptr);
uint32_t get_heap_usage();
void* memcpy(void* dest, const void* src, uint32_t n);
void* memset(void* dest, int val, uint32_t n);
int abs_int(int x);

// 6. Math & AI Core
void enable_fpu();
float exp_float(float x);
float sqrt_float(float x);
float log_float(float x);
float powf_float(float base, float exponent);
float sin_float(float x);
float cos_float(float x);
float sigmoid(float x);
float silu(float x);
void softmax(float *input, int size);
void rmsnorm(float* o, float* x, float* weight, int size);
void matmul(float* xout, float* x, float* w, int n, int d);
void matmul_parallel(float* xout, float* x, float* w, int n, int d, int core_id, int num_cores);

// 7. Disk Driver
void read_sectors_ATA_PIO(uint32_t target_address, uint32_t LBA, uint8_t sector_count);
void read_disk_large(uint32_t target_address, uint32_t start_LBA, uint32_t total_sectors);

// 8. SMP
void init_smp();

// 9. Old 32-token AI Engine
void init_neural_weights();
void run_neural_engine(gguf_header_t *brain, const char *prompt);

// 10. Llama2 Transformer Engine
int llama_load_model(uint32_t disk_start_sector);
int llama_load_tokenizer(uint32_t disk_start_sector, uint32_t size_bytes);
void llama_generate(int max_tokens);
void llama_generate_with_prompt(const char *prompt, int max_tokens);
int llama_is_loaded();
int llama_get_vocab_size();

// 11. Keyboard PS/2 Driver
char keyboard_read_char();

#endif
