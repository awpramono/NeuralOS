#include "system.h"

static uint32_t heap_current = 0;

void init_heap(uint32_t start_addr) {
    heap_current = start_addr;
    print_string("[Memory] Sistem Heap (Alokasi Dinamis) aktif di: ", 0x0A);
    print_hex(start_addr, 0x0A);
    print_string("\n", 0x0F);
}

void* mem_alloc(uint32_t size) {
    if (heap_current == 0) return 0;
    
    // Keamanan Arsitektur: Pastikan alokasi selalu kelipatan 4-byte (Alignment)
    uint32_t aligned_size = (size + 3) & ~3;
    
    void* allocated_ptr = (void*)heap_current;
    heap_current += aligned_size; // Geser batas ujung memori
    
    return allocated_ptr;
}

void mem_free(void *ptr) {
    // Pada arsitektur bump-allocator MVP, kita tidak membebaskan blok spesifik.
    // Memori akan dikosongkan sekaligus dengan me-reset heap_current jika diperlukan.
    (void)ptr; 
}
