#include "system.h"

volatile char *video_memory = (volatile char*)0xB8000;
int cursor = 0;

void clear_screen() {
    for (int i = 0; i < 80 * 25 * 2; i += 2) {
        video_memory[i] = ' '; video_memory[i+1] = 0x07;
    }
    cursor = 0;
}

void print_char(char c, char color) {
    if (c == '\n') { cursor = (cursor / 160 + 1) * 160; } 
    else { video_memory[cursor] = c; video_memory[cursor+1] = color; cursor += 2; }
    if (cursor >= 80 * 25 * 2) cursor = 0; 
}

void print_string(const char *str, char color) {
    int i = 0; while (str[i] != '\0') { print_char(str[i], color); i++; }
}

void print_number(uint32_t n, char color) {
    if (n == 0) { print_char('0', color); return; }
    char buffer[16]; int i = 0;
    while (n > 0) { buffer[i++] = (n % 10) + '0'; n /= 10; }
    while (i > 0) { print_char(buffer[--i], color); }
}

int string_compare(const char *str1, const char *str2) {
    int i = 0;
    while (str1[i] != '\0' && str2[i] != '\0') {
        if (str1[i] != str2[i]) return 0;
        i++;
    }
    return (str1[i] == '\0' && str2[i] == '\0');
}

void print_hex(uint32_t n, char color) {
    print_string("0x", color);
    if (n == 0) { print_char('0', color); return; }
    char buffer[8]; int i = 0;
    while (n > 0) {
        int rem = n % 16;
        buffer[i++] = (rem < 10) ? (rem + '0') : (rem - 10 + 'A');
        n /= 16;
    }
    while (i > 0) { print_char(buffer[--i], color); }
}
