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
