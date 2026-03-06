#include "system.h"

// FAT BIOS Parameter Block (BPB)
typedef struct {
  uint8_t jump_boot[3];
  char oem_name[8];
  uint16_t bytes_per_sector;
  uint8_t sectors_per_cluster;
  uint16_t reserved_sector_count;
  uint8_t table_count;
  uint16_t root_entry_count;
  uint16_t total_sectors_16;
  uint8_t media_type;
  uint16_t table_size_16;
  uint16_t sectors_per_track;
  uint16_t head_side_count;
  uint32_t hidden_sector_count;
  uint32_t total_sectors_32;

  // Extended FAT12 and FAT16
  uint8_t drive_number;
  uint8_t reserved1;
  uint8_t boot_signature;
  uint32_t volume_id;
  char volume_label[11];
  char fat_type_label[8];
} __attribute__((packed)) fat16_bpb_t;

// Standard FAT Directory Entry (32 bytes)
typedef struct {
  uint8_t name[11];
  uint8_t attr;
  uint8_t nt_reserved;
  uint8_t creation_time_tenths;
  uint16_t creation_time;
  uint16_t creation_date;
  uint16_t last_access_date;
  uint16_t first_cluster_high;
  uint16_t write_time;
  uint16_t write_date;
  uint16_t first_cluster_low;
  uint32_t file_size;
} __attribute__((packed)) fat_dir_entry_t;

void fat_init() {
  serial_print_string("[FAT] Probing FAT12/16 Filesystem...\n");
  print_string("[FAT] Initializing FAT Volume...\n", 0x0E);

  // Allocate 1 sector
  uint8_t *boot_sector = (uint8_t *)mem_alloc(512);

  // Read MBR/Boot Sector at LBA 0
  read_sectors_ATA_PIO((uint32_t)boot_sector, 0, 1);

  fat16_bpb_t *bpb = (fat16_bpb_t *)boot_sector;

  // Check boot signature 0xAA55 and standard jump instruction length
  if (boot_sector[510] == 0x55 && boot_sector[511] == 0xAA &&
      (bpb->jump_boot[0] == 0xEB || bpb->jump_boot[0] == 0xE9)) {

    print_string("[FS] Found FAT16/12 Volume: ", 0x0A);
    for (int i = 0; i < 8; i++)
      print_char(bpb->oem_name[i], 0x0B);
    print_string("\n", 0x0A);

    uint32_t total_sectors = (bpb->total_sectors_16 == 0)
                                 ? bpb->total_sectors_32
                                 : bpb->total_sectors_16;
    uint32_t root_dir_sectors =
        ((bpb->root_entry_count * 32) + (bpb->bytes_per_sector - 1)) /
        bpb->bytes_per_sector;
    uint32_t first_data_sector = bpb->reserved_sector_count +
                                 (bpb->table_count * bpb->table_size_16) +
                                 root_dir_sectors;

    print_string("  |- Bytes/Sector:  ", 0x08);
    print_number(bpb->bytes_per_sector, 0x0E);
    print_string("\n", 0x08);
    print_string("  |- Sec/Cluster:   ", 0x08);
    print_number(bpb->sectors_per_cluster, 0x0E);
    print_string("\n", 0x08);
    print_string("  |- Max Root Dirs: ", 0x08);
    print_number(bpb->root_entry_count, 0x0E);
    print_string("\n", 0x08);
    print_string("  |- Data Start LBA:", 0x08);
    print_number(first_data_sector, 0x0E);
    print_string("\n", 0x08);

    serial_print_string("[FAT] Mount Success.\n");
  } else {
    // Since we write a raw `disk.img` via python script with zeros,
    // we generally will fall here on first boot unless manually formatted.
    print_string("[FAT] No FAT filesystem found (Raw disk detected)\n", 0x0C);
    serial_print_string("[FAT] Probe failed. Missing 0xAA55 signature.\n");
  }

  mem_free(boot_sector);
}

// Future implementations for reading FAT chains:
// int fat_read_file(const char *filename, uint8_t **out_buffer) ...
