#include "system.h"

// Fungsi pembantu untuk menulis/membaca port (I/O)
static inline void outb(uint16_t port, uint8_t val) { __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port)); }
static inline uint8_t inb(uint16_t port) { uint8_t res; __asm__ volatile("inb %1, %0" : "=a"(res) : "Nd"(port)); return res; }
static inline uint16_t inw(uint16_t port) { uint16_t res; __asm__ volatile("inw %1, %0" : "=a"(res) : "Nd"(port)); return res; }

void read_sectors_ATA_PIO(uint32_t target_address, uint32_t LBA, uint8_t sector_count) {
    // 1. Persiapan: Pilih Master Drive dan kirim LBA (koordinat sektor)
    outb(0x1F6, 0xE0 | ((LBA >> 24) & 0x0F));
    outb(0x1F2, sector_count);
    outb(0x1F3, (uint8_t)LBA);
    outb(0x1F4, (uint8_t)(LBA >> 8));
    outb(0x1F5, (uint8_t)(LBA >> 16));
    
    // 2. Kirim perintah READ SECTORS (0x20)
    outb(0x1F7, 0x20);

    // 3. Tunggu disk siap (Polling)
    while (!(inb(0x1F7) & 0x08));

    // 4. Tarik data dari port 0x1F0 ke RAM
    uint16_t *ptr = (uint16_t *)target_address;
    for (int i = 0; i < (sector_count * 256); i++) {
        ptr[i] = inw(0x1F0);
    }
    
    print_string("[Disk] Berhasil membaca ", 0x0A);
    print_number(sector_count, 0x0E);
    print_string(" sektor ke memori.\n", 0x0A);
}
