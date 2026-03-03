#include "system.h"

volatile char *video_memory = (volatile char*)0xB8000;
int cursor_x = 0;
int cursor_y = 0;

void clear_screen() {
    for (int i = 0; i < 80 * 25; i++) {
        video_memory[i * 2] = ' ';
        video_memory[i * 2 + 1] = 0x07; // Light gray on black
    }
    cursor_x = 0;
    cursor_y = 0;
}

void print_char(char c, char color) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\b') {
        // LOGIKA BACKSPACE: Mundurkan kursor, jangan cetak simbol!
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) {
            // Mundur ke baris sebelumnya jika berada di ujung kiri layar
            cursor_y--;
            cursor_x = 79;
        }
    } else {
        video_memory[(cursor_y * 80 + cursor_x) * 2] = c;
        video_memory[(cursor_y * 80 + cursor_x) * 2 + 1] = color;
        cursor_x++;
    }
    
    // Auto-wrap dan Reset jika penuh
    if (cursor_x >= 80) { cursor_x = 0; cursor_y++; }
    if (cursor_y >= 25) { clear_screen(); } 
}

void print_string(const char *str, char color) {
    while (*str) {
        print_char(*str++, color);
    }
}

void print_number(uint32_t n, char color) {
    if (n == 0) { print_char('0', color); return; }
    char buffer[10];
    int i = 0;
    while (n > 0) { buffer[i++] = (n % 10) + '0'; n /= 10; }
    while (i > 0) { print_char(buffer[--i], color); }
}

void print_hex(uint32_t n, char color) {
    print_string("0x", color);
    if (n == 0) { print_char('0', color); return; }
    char buffer[8];
    int i = 0;
    while (n > 0) {
        int digit = n % 16;
        buffer[i++] = (digit < 10) ? (digit + '0') : (digit - 10 + 'A');
        n /= 16;
    }
    while (i > 0) { print_char(buffer[--i], color); }
}

int string_compare(const char *str1, const char *str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return (*str1 == *str2);
}
