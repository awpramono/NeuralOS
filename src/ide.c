#include "system.h"

// I/O port helpers
static inline void outb(uint16_t port, uint8_t val) { __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port)); }
static inline uint8_t inb(uint16_t port) { uint8_t res; __asm__ volatile("inb %1, %0" : "=a"(res) : "Nd"(port)); return res; }
static inline uint16_t inw(uint16_t port) { uint16_t res; __asm__ volatile("inw %1, %0" : "=a"(res) : "Nd"(port)); return res; }

// Wait for drive to be ready (BSY clear, DRQ set)
static void ata_wait_ready() {
    while (inb(0x1F7) & 0x80);  // Wait for BSY to clear
}

static void ata_wait_drq() {
    while (!(inb(0x1F7) & 0x08));  // Wait for DRQ (data ready)
}

// Read a single sector (512 bytes) from disk to RAM
static void read_one_sector(uint16_t *dest, uint32_t LBA) {
    ata_wait_ready();
    outb(0x1F6, 0xE0 | ((LBA >> 24) & 0x0F));
    outb(0x1F2, 1);  // 1 sector
    outb(0x1F3, (uint8_t)LBA);
    outb(0x1F4, (uint8_t)(LBA >> 8));
    outb(0x1F5, (uint8_t)(LBA >> 16));
    outb(0x1F7, 0x20);  // READ SECTORS command
    ata_wait_drq();
    for (int i = 0; i < 256; i++) {
        dest[i] = inw(0x1F0);
    }
}

// Read multiple sectors reliably (one sector at a time with proper handshaking)
void read_sectors_ATA_PIO(uint32_t target_address, uint32_t LBA, uint8_t sector_count) {
    uint16_t *ptr = (uint16_t *)target_address;
    for (uint8_t s = 0; s < sector_count; s++) {
        read_one_sector(ptr + (s * 256), LBA + s);
    }
}

// Read large data from disk (supports > 255 sectors)
// Used for loading LLM model weights (60+ MB)
void read_disk_large(uint32_t target_address, uint32_t start_LBA, uint32_t total_sectors) {
    uint16_t *ptr = (uint16_t *)target_address;
    for (uint32_t s = 0; s < total_sectors; s++) {
        read_one_sector(ptr + (s * 256), start_LBA + s);
        // Print progress every 1024 sectors (~512KB)
        if (s > 0 && (s % 1024) == 0) {
            print_string("  [Disk] ", 0x08);
            print_number(s / 2, 0x08);  // KB loaded
            print_string("KB loaded...\n", 0x08);
        }
    }
}
