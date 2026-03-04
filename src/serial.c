#include "system.h"

// ============================================================================
// NeuralOS Serial Port Driver (COM1)
// Used for remote telemetry, background logging, and AI debugging
// Base Address for COM1 is typically 0x3F8
// ============================================================================

#define PORT_COM1 0x3F8

// Low-level port I/O functions
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

// ----------------------------------------------------------------------------
// Initialization
// ----------------------------------------------------------------------------
void serial_init() {
    outb(PORT_COM1 + 1, 0x00);    // Disable all interrupts
    outb(PORT_COM1 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(PORT_COM1 + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(PORT_COM1 + 1, 0x00);    //                  (hi byte)
    outb(PORT_COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(PORT_COM1 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(PORT_COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    
    serial_print_string("\n========================================\n");
    serial_print_string("  NeuralOS v3.6 - Serial COM1 Active \n");
    serial_print_string("  Telemetry & Agent Logging Started    \n");
    serial_print_string("========================================\n\n");
}

// ----------------------------------------------------------------------------
// Check if transmit buffer is empty (safe to write)
// ----------------------------------------------------------------------------
static int serial_is_transmit_empty() {
    return inb(PORT_COM1 + 5) & 0x20;
}

// ----------------------------------------------------------------------------
// Telemetry Writers
// ----------------------------------------------------------------------------
void serial_print_char(char a) {
    while (serial_is_transmit_empty() == 0);
    outb(PORT_COM1, a);
}

void serial_print_string(const char *str) {
    if (!str) return;
    for (int i = 0; str[i] != '\0'; i++) {
        serial_print_char(str[i]);
    }
}

void serial_print_number(int num) {
    if (num == 0) {
        serial_print_char('0');
        return;
    }
    if (num < 0) {
        serial_print_char('-');
        num = -num;
    }
    
    char buffer[12];
    int idx = 0;
    while (num > 0 && idx < 12) {
        buffer[idx++] = (num % 10) + '0';
        num /= 10;
    }
    
    while (idx > 0) {
        serial_print_char(buffer[--idx]);
    }
}
