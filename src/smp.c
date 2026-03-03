#include "system.h"

extern uint8_t trampoline_start;
extern uint8_t trampoline_end;

void init_smp() {
    print_string("\n[SMP] Menyiapkan Protokol Kebangkitan...\n", 0x0B);
    
    // 1. Salin kode trampolin ke alamat 0x8000
    uint8_t *dest = (uint8_t *)0x8000;
    uint8_t *src = &trampoline_start;
    uint32_t size = &trampoline_end - &trampoline_start;
    for(uint32_t i=0; i<size; i++) dest[i] = src[i];
    
    print_string("[SMP] Trampolin terpasang di 0x8000.\n", 0x0A);
    
    // 2. Akses Local APIC (Biasanya di 0xFEE00000)
    volatile uint32_t *lapic = (uint32_t *)0xFEE00000;
    
    // Beritahu Core 1 untuk Reset (INIT IPI)
    lapic[0x310/4] = (1 << 24);              // Target: Core 1
    lapic[0x300/4] = 0x0000C500;             // Level: Assert, Delivery: INIT
    
    // Tunggu 10ms (delay sederhana)
    for(volatile int i=0; i<1000000; i++);
    
    // Kirim STARTUP IPI ke alamat 0x8000 (Vektor 0x08)
    lapic[0x300/4] = 0x0000C608;
    
    print_string("[SMP] Sinyal SIPI dikirim ke Core 1.\n", 0x0D);
    
    // 3. Verifikasi: Tunggu Core 1 menulis 'CAFEBABE' ke alamat 0x9000
    volatile uint32_t *sync = (uint32_t *)0x9000;
    print_string("[SMP] Menunggu konfirmasi Core 1...", 0x0F);
    
    int timeout = 1000000;
    while(*sync != 0xCAFEBABE && timeout-- > 0);
    
    if(*sync == 0xCAFEBABE) {
        print_string(" [OKE]\n", 0x0A);
        print_string("[System] NeuralOS sekarang berjalan di MULTI-CORE mode.\n", 0x0B);
    } else {
        print_string(" [TIMEOUT]\n", 0x0C);
    }
}
