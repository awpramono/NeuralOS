#include "system.h"

static uint32_t heap_start   = 0;
static uint32_t heap_current = 0;

void init_heap(uint32_t start_addr) {
    heap_start   = start_addr;
    heap_current = start_addr;
    print_string("[Memory] Heap initialized at: ", 0x0A);
    print_hex(start_addr, 0x0A);
    print_string("\n", 0x0F);
}

void* mem_alloc(uint32_t size) {
    if (heap_current == 0) return 0;
    
    // Ensure 4-byte alignment
    uint32_t aligned_size = (size + 3) & ~3;
    
    void* allocated_ptr = (void*)heap_current;
    heap_current += aligned_size;
    
    return allocated_ptr;
}

void mem_free(void *ptr) {
    // Bump allocator - no individual free
    (void)ptr; 
}

uint32_t get_heap_usage() {
    return heap_current;
}

void* mem_calloc(uint32_t count, uint32_t size) {
    uint32_t total = count * size;
    void* ptr = mem_alloc(total);
    if (ptr) {
        // Zero-initialize
        uint8_t* p = (uint8_t*)ptr;
        for (uint32_t i = 0; i < total; i++) p[i] = 0;
    }
    return ptr;
}

void* memcpy(void* dest, const void* src, uint32_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

void* memset(void* dest, int val, uint32_t n) {
    uint8_t* d = (uint8_t*)dest;
    for (uint32_t i = 0; i < n; i++) d[i] = (uint8_t)val;
    return dest;
}

int abs_int(int x) { return x < 0 ? -x : x; }
