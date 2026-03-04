#include "system.h"

// Fungsi low-level untuk membaca 1 byte dari port hardware (Inline Assembly)
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

// Peta Scan Code PS/2 ke ASCII (QWERTY Standard)
static const char scancode_to_ascii[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`',
    0, '\\', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', 0,
    '*', 0, ' '
};

// Fungsi utama untuk menangkap 1 huruf dari keyboard tanpa memblokir
char keyboard_poll_char() {
    // Cek apakah ada data di buffer PS/2
    if ((inb(0x64) & 1) == 0) {
        return 0; // Tidak ada tombol ditekan, kembalikan 0 (idle)
    }
    
    // Tarik datanya dari Port 0x60
    uint8_t scancode = inb(0x60);
    
    // Sinyal "Press" selalu kurang dari 0x80
    // Pastikan tidak out-of-bounds dari scancode_to_ascii map
    if (scancode < 0x80 && scancode < sizeof(scancode_to_ascii)) {
        return scancode_to_ascii[scancode];
    }
    
    return 0; // Abaikan sinyal key-release atau unknown
}
