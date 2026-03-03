#ifndef SYSTEM_H
#define SYSTEM_H

// 1. Tipe Data Dasar
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

// 2. Struktur Multiboot (Wajib untuk GRUB/QEMU)
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

// 3. Struktur AI (GGUF)
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t tensor_count;
    uint64_t metadata_kv_count;
} __attribute__((packed)) gguf_header_t;

// 4. VGA & Interaksi String
void clear_screen();
void print_char(char c, char color);
void print_string(const char *str, char color);
void print_number(uint32_t n, char color);
void print_hex(uint32_t n, char color);
int string_compare(const char *str1, const char *str2);

// 5. Memory Management
void init_heap(uint32_t start_addr);
void* mem_alloc(uint32_t size);

// 6. Math & AI Core
void enable_fpu();
float silu(float x);
void softmax(float *input, int size);
void rmsnorm(float* o, float* x, float* weight, int size);
void matmul_parallel(float* xout, float* x, float* w, int n, int d, int core_id);

// 7. Disk Driver & SMP
void read_sectors_ATA_PIO(uint32_t target_address, uint32_t LBA, uint8_t sector_count);
void init_smp();
void run_neural_engine(gguf_header_t *brain);

#endif
