#include "system.h"

// ============================================================================
// NeuralFS - A minimal, flat, sequential File System for NeuralOS
// Designed for AI Agent storage, Ephemeral Scripts, and Cognitive Logs
//
// Layout on Disk:
// Sector 20: Superblock
// Sector 21-29: Directory Entries (Max 144 files)
// Sector 30+: Data Blocks
// ============================================================================

#define FS_SUPERBLOCK_LBA 20
#define FS_DIR_ENTRY_LBA 21
#define FS_DATA_START_LBA 30
#define FS_MAGIC 0x4E465331 // 'NFS1'

#define MAX_FILES 64
#define FILENAME_LEN 16

// Directory Entry (32 bytes)
typedef struct {
    char name[FILENAME_LEN]; // Null-terminated if < 16, else 16 chars
    uint32_t start_lba;
    uint32_t size_bytes;
    uint32_t flags;          // 0 = empty, 1 = file
    uint32_t reserved;
} __attribute__((packed)) DirEntry;

// Superblock (512 bytes)
typedef struct {
    uint32_t magic;
    uint32_t next_free_lba;
    uint32_t file_count;
    uint8_t  padding[500];
} __attribute__((packed)) Superblock;

static Superblock fs_super;
static DirEntry fs_dir[MAX_FILES]; // RAM cache (2048 bytes)
static uint8_t fs_initialized = 0;

// ----------------------------------------------------------------------------
// Internal Helpers
// ----------------------------------------------------------------------------

static void strncpy_safe(char *dest, const char *src, int n) {
    int i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
}

// Write superblock & directory cache to disk
static void fs_sync() {
    write_sectors_ATA_PIO((uint32_t)&fs_super, FS_SUPERBLOCK_LBA, 1);
    write_sectors_ATA_PIO((uint32_t)fs_dir, FS_DIR_ENTRY_LBA, sizeof(fs_dir) / 512);
}

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

void fs_init() {
    serial_print_string("[FS] Initializing NeuralFS...\n");
    
    // Read superblock
    read_sectors_ATA_PIO((uint32_t)&fs_super, FS_SUPERBLOCK_LBA, 1);
    
    if (fs_super.magic == FS_MAGIC) {
        serial_print_string("[FS] Found existing filesystem. Files: ");
        serial_print_number(fs_super.file_count);
        serial_print_string("\n");
        
        // Load directory entries
        read_sectors_ATA_PIO((uint32_t)fs_dir, FS_DIR_ENTRY_LBA, sizeof(fs_dir) / 512);
    } else {
        serial_print_string("[FS] Formatting fresh filesystem...\n");
        fs_super.magic = FS_MAGIC;
        fs_super.next_free_lba = FS_DATA_START_LBA;
        fs_super.file_count = 0;
        
        for (int i = 0; i < MAX_FILES; i++) {
            fs_dir[i].flags = 0;
            for (int k = 0; k < FILENAME_LEN; k++) fs_dir[i].name[k] = '\0';
        }
        
        fs_sync();
    }
    fs_initialized = 1;
}

// Create and write a file sequentially. Overwrites if exists.
int fs_write_file(const char *filename, const uint8_t *data, uint32_t size) {
    if (!fs_initialized) return -1;
    
    // Find existing or free slot
    int slot = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs_dir[i].flags == 1 && string_compare(fs_dir[i].name, filename)) {
            slot = i;
            break; // Overwrite existing (WARNING: leaks old data blocks in this simple FS)
        }
        if (slot == -1 && fs_dir[i].flags == 0) {
            slot = i; // First free slot
        }
    }
    
    if (slot == -1) {
        serial_print_string("[FS] Error: Directory full\n");
        return -2;
    }
    
    // Calculate sectors needed
    uint32_t sectors = (size + 511) / 512;
    if (sectors == 0) sectors = 1;
    
    // Prepare directory entry
    strncpy_safe(fs_dir[slot].name, filename, FILENAME_LEN);
    fs_dir[slot].start_lba = fs_super.next_free_lba;
    fs_dir[slot].size_bytes = size;
    fs_dir[slot].flags = 1;
    
    // Write data blocks to disk
    // Since we write 512-byte blocks, we copy data to a padded buffer
    uint8_t *buffer = (uint8_t*)mem_alloc(sectors * 512);
    memcpy(buffer, data, size);
    write_sectors_ATA_PIO((uint32_t)buffer, fs_super.next_free_lba, sectors);
    mem_free(buffer);
    
    // Update superblock
    fs_super.next_free_lba += sectors;
    if ((uint32_t)slot >= fs_super.file_count) {
        fs_super.file_count++;
    }
    
    fs_sync();
    
    serial_print_string("[FS] Wrote file: ");
    serial_print_string(filename);
    serial_print_string(" (");
    serial_print_number(size);
    serial_print_string(" bytes)\n");
    return 0; // Success
}

// Read a file into a newly allocated buffer. Returns size or -1 if not found.
int fs_read_file(const char *filename, uint8_t **out_data) {
    if (!fs_initialized) return -1;
    
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs_dir[i].flags == 1 && string_compare(fs_dir[i].name, filename)) {
            uint32_t size = fs_dir[i].size_bytes;
            uint32_t sectors = (size + 511) / 512;
            if (sectors == 0) sectors = 1;
            
            uint8_t *buffer = (uint8_t*)mem_alloc(sectors * 512);
            read_sectors_ATA_PIO((uint32_t)buffer, fs_dir[i].start_lba, sectors);
            
            *out_data = buffer;
            return size;
        }
    }
    
    serial_print_string("[FS] File not found: ");
    serial_print_string(filename);
    serial_print_string("\n");
    return -1; // Not found
}

// Print directory listing to screen
void fs_list_files() {
    if (!fs_initialized) return;
    
    print_string("NeuralFS Directory Listing:\n", 0x0E);
    print_string("---------------------------\n", 0x08);
    
    int count = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs_dir[i].flags == 1) {
            print_string("  ", 0x0F);
            print_string(fs_dir[i].name, 0x0B);
            print_string(" \t(", 0x08);
            print_number(fs_dir[i].size_bytes, 0x0E);
            print_string(" bytes)\n", 0x08);
            count++;
        }
    }
    
    if (count == 0) {
        print_string("  (Empty)\n", 0x08);
    }
    print_string("---------------------------\n", 0x08);
    print_number(count, 0x0F);
    print_string(" files total. FS Memory used: ", 0x0F);
    uint32_t used_kb = ((fs_super.next_free_lba - FS_DATA_START_LBA) * 512) / 1024;
    print_number(used_kb, 0x0E);
    print_string(" KB\n", 0x0F);
}
