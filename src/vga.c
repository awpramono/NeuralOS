#include "system.h"

// ============================================================================
// NeuralOS VGA Driver — 80x25 Text Mode with Scrolling & Status Bar
// ============================================================================

// Port I/O (global versions)
void outb_port(uint16_t port, uint8_t val) {
  __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

uint8_t inb_port(uint16_t port) {
  uint8_t ret;
  __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

volatile char *video_memory = (volatile char *)0xB8000;
int cursor_x = 0;
int cursor_y = 1; // Start at row 1 (row 0 = status bar)

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define STATUS_ROW 0 // Top row for status bar
#define TEXT_START 1 // Text area starts at row 1
#define TEXT_END 24  // Text area ends at row 24 (inclusive)

// Hardware cursor update (makes blinking cursor visible)
static void update_hw_cursor() {
  uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;
  outb_port(0x3D4, 14);
  outb_port(0x3D5, (pos >> 8) & 0xFF);
  outb_port(0x3D4, 15);
  outb_port(0x3D5, pos & 0xFF);
}

// Scroll text area up by one line (preserves status bar)
static void scroll_up() {
  // Move rows TEXT_START+1..TEXT_END up by one row
  for (int y = TEXT_START; y < TEXT_END; y++) {
    for (int x = 0; x < VGA_WIDTH; x++) {
      int dst = (y * VGA_WIDTH + x) * 2;
      int src = ((y + 1) * VGA_WIDTH + x) * 2;
      video_memory[dst] = video_memory[src];
      video_memory[dst + 1] = video_memory[src + 1];
    }
  }
  // Clear the last line
  for (int x = 0; x < VGA_WIDTH; x++) {
    int pos = (TEXT_END * VGA_WIDTH + x) * 2;
    video_memory[pos] = ' ';
    video_memory[pos + 1] = 0x07;
  }
}

void clear_screen() {
  for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
    video_memory[i * 2] = ' ';
    video_memory[i * 2 + 1] = 0x07;
  }
  cursor_x = 0;
  cursor_y = TEXT_START;
  update_hw_cursor();
}

void print_char(char c, char color) {
  if (c == '\n') {
    cursor_x = 0;
    cursor_y++;
  } else if (c == '\b') {
    if (cursor_x > 0) {
      cursor_x--;
      // Clear the character
      int pos = (cursor_y * VGA_WIDTH + cursor_x) * 2;
      video_memory[pos] = ' ';
      video_memory[pos + 1] = 0x07;
    } else if (cursor_y > TEXT_START) {
      cursor_y--;
      cursor_x = VGA_WIDTH - 1;
    }
  } else {
    int pos = (cursor_y * VGA_WIDTH + cursor_x) * 2;
    video_memory[pos] = c;
    video_memory[pos + 1] = color;
    cursor_x++;
  }

  // Auto-wrap
  if (cursor_x >= VGA_WIDTH) {
    cursor_x = 0;
    cursor_y++;
  }

  // Scroll when reaching bottom
  while (cursor_y > TEXT_END) {
    scroll_up();
    cursor_y--;
  }

  update_hw_cursor();
}

void print_string(const char *str, char color) {
  while (*str) {
    print_char(*str++, color);
  }
}

void print_number(uint32_t n, char color) {
  if (n == 0) {
    print_char('0', color);
    return;
  }
  char buffer[10];
  int i = 0;
  while (n > 0) {
    buffer[i++] = (n % 10) + '0';
    n /= 10;
  }
  while (i > 0) {
    print_char(buffer[--i], color);
  }
}

void print_hex(uint32_t n, char color) {
  print_string("0x", color);
  if (n == 0) {
    print_char('0', color);
    return;
  }
  char buffer[8];
  int i = 0;
  while (n > 0) {
    int digit = n % 16;
    buffer[i++] = (digit < 10) ? (digit + '0') : (digit - 10 + 'A');
    n /= 16;
  }
  while (i > 0) {
    print_char(buffer[--i], color);
  }
}

int string_compare(const char *str1, const char *str2) {
  while (*str1 && (*str1 == *str2)) {
    str1++;
    str2++;
  }
  return (*str1 == *str2);
}

// ============================================================================
// Status Bar — persistent info at top of screen
// ============================================================================

void update_status_bar() {
  // Colors: white on blue for status bar
  char bg = 0x1F; // White text on blue background

  // Clear status bar
  for (int x = 0; x < VGA_WIDTH; x++) {
    int pos = (STATUS_ROW * VGA_WIDTH + x) * 2;
    video_memory[pos] = ' ';
    video_memory[pos + 1] = bg;
  }

  // Write status text directly to video memory (don't move cursor)
  const char *title = " NeuralOS v3.5 Agentic ";
  int x = 0;
  while (*title && x < VGA_WIDTH) {
    int pos = (STATUS_ROW * VGA_WIDTH + x) * 2;
    video_memory[pos] = *title;
    video_memory[pos + 1] = 0x1E; // Yellow on blue
    title++;
    x++;
  }

  // Separator
  int pos = (STATUS_ROW * VGA_WIDTH + x) * 2;
  video_memory[pos] = '|';
  video_memory[pos + 1] = 0x19;
  x++;

  // Cores info
  volatile uint32_t *mailbox = (volatile uint32_t *)0x9000;
  uint32_t num_cores = mailbox[6];
  if (num_cores == 0)
    num_cores = 1;

  const char *cores_label = " Cores:";
  while (*cores_label && x < VGA_WIDTH) {
    pos = (STATUS_ROW * VGA_WIDTH + x) * 2;
    video_memory[pos] = *cores_label;
    video_memory[pos + 1] = bg;
    cores_label++;
    x++;
  }
  pos = (STATUS_ROW * VGA_WIDTH + x) * 2;
  video_memory[pos] = '0' + num_cores;
  video_memory[pos + 1] = 0x1A; // Green on blue
  x++;

  // Separator
  pos = (STATUS_ROW * VGA_WIDTH + x) * 2;
  video_memory[pos] = ' ';
  video_memory[pos + 1] = bg;
  x++;
  pos = (STATUS_ROW * VGA_WIDTH + x) * 2;
  video_memory[pos] = '|';
  video_memory[pos + 1] = 0x19;
  x++;

  // Memory usage
  uint32_t heap_used = get_heap_usage() - 0x1000000;
  const char *mem_label = " Mem:";
  while (*mem_label && x < VGA_WIDTH) {
    pos = (STATUS_ROW * VGA_WIDTH + x) * 2;
    video_memory[pos] = *mem_label;
    video_memory[pos + 1] = bg;
    mem_label++;
    x++;
  }

  // Print heap KB directly
  uint32_t mem_kb = heap_used / 1024;
  char mem_buf[8];
  int mi = 0;
  if (mem_kb == 0) {
    mem_buf[mi++] = '0';
  } else {
    uint32_t tmp = mem_kb;
    while (tmp > 0) {
      mem_buf[mi++] = '0' + (tmp % 10);
      tmp /= 10;
    }
  }
  while (mi > 0) {
    pos = (STATUS_ROW * VGA_WIDTH + x) * 2;
    video_memory[pos] = mem_buf[--mi];
    video_memory[pos + 1] = 0x1A;
    x++;
  }
  const char *kb = "KB ";
  while (*kb && x < VGA_WIDTH) {
    pos = (STATUS_ROW * VGA_WIDTH + x) * 2;
    video_memory[pos] = *kb;
    video_memory[pos + 1] = bg;
    kb++;
    x++;
  }

  // AI status
  pos = (STATUS_ROW * VGA_WIDTH + x) * 2;
  video_memory[pos] = '|';
  video_memory[pos + 1] = 0x19;
  x++;

  const char *ai_label;
  char ai_color;
  if (llama_is_loaded()) {
    ai_label = " AI:Llama2 ";
    ai_color = 0x1A; // Green on blue
  } else {
    ai_label = " AI:32tok ";
    ai_color = 0x1E; // Yellow on blue
  }
  while (*ai_label && x < VGA_WIDTH) {
    pos = (STATUS_ROW * VGA_WIDTH + x) * 2;
    video_memory[pos] = *ai_label;
    video_memory[pos + 1] = ai_color;
    ai_label++;
    x++;
  }

  // Right-align: "Agent ON"
  const char *agent = "Agent ON";
  int agent_start = VGA_WIDTH - 9;
  for (int i = 0; agent[i] && agent_start + i < VGA_WIDTH; i++) {
    pos = (STATUS_ROW * VGA_WIDTH + agent_start + i) * 2;
    video_memory[pos] = agent[i];
    video_memory[pos + 1] = 0x1C; // Red on blue (bright)
  }
}
