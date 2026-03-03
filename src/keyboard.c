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

// Fungsi utama untuk menangkap 1 huruf dari keyboard
char keyboard_read_char() {
    uint8_t scancode;
    
    // Polling: Loop ini akan menahan CPU sampai bit 0 dari Port 0x64 bernilai 1
    // (Artinya ada data yang siap diambil di Port 0x60)
    while ((inb(0x64) & 1) == 0);
    
    // Tarik datanya dari Port 0x60
    scancode = inb(0x60);
    
    // Hardware mengirim 2 sinyal: saat tombol ditekan (Press) dan dilepas (Release).
    // Sinyal "Release" selalu memiliki nilai hex lebih dari 0x80. 
    // Kita hanya butuh sinyal "Press" (kurang dari 0x80).
    if (scancode < 0x80) {
        return scancode_to_ascii[scancode];
    }
    
    return 0; // Abaikan sinyal key-release
}
