#include "system.h"

// Intel E1000 MMIO Register Offsets
#define E1000_REG_CTRL 0x00000
#define E1000_REG_STATUS 0x00008
#define E1000_REG_EEPROM 0x00014
#define E1000_REG_CTRL_EXT 0x00018
#define E1000_REG_ICR 0x000C0
#define E1000_REG_IMS 0x000D0
#define E1000_REG_RCTL 0x00100
#define E1000_REG_TCTL 0x00400
#define E1000_REG_TIPG 0x00410
#define E1000_REG_RDBAL 0x02800
#define E1000_REG_RDBAH 0x02804
#define E1000_REG_RDLEN 0x02808
#define E1000_REG_RDH 0x02810
#define E1000_REG_RDT 0x02818
#define E1000_REG_TDBAL 0x03800
#define E1000_REG_TDBAH 0x03804
#define E1000_REG_TDLEN 0x03808
#define E1000_REG_TDH 0x03810
#define E1000_REG_TDT 0x03818
#define E1000_REG_MTA 0x05200
#define E1000_REG_RAL 0x05400
#define E1000_REG_RAH 0x05404

// TX/RX Rings Structures
typedef struct {
  uint64_t addr;
  uint16_t length;
  uint16_t cso;
  uint8_t cmd;
  uint8_t status;
  uint8_t css;
  uint8_t special;
} __attribute__((packed)) e1000_tx_desc;

typedef struct {
  uint64_t addr;
  uint16_t length;
  uint16_t checksum;
  uint8_t status;
  uint8_t errors;
  uint16_t special;
} __attribute__((packed)) e1000_rx_desc;

#define E1000_NUM_TX_DESC 8
#define E1000_NUM_RX_DESC 8

static uint8_t *tx_ptr;
static uint8_t *rx_ptr;
static e1000_tx_desc *tx_descs;
static e1000_rx_desc *rx_descs;
static uint32_t tx_cur = 0;
static uint32_t rx_cur = 0;
uint8_t mac_addr[6];

extern int pci_e1000_found;
extern uint32_t e1000_mmio_base;

// Helper: MMIO Write
static void mmio_write32(uint32_t base, uint32_t offset, uint32_t value) {
  (*(volatile uint32_t *)(base + offset)) = value;
}

// Helper: MMIO Read
static uint32_t mmio_read32(uint32_t base, uint32_t offset) {
  return (*(volatile uint32_t *)(base + offset));
}

// Helper: Read MAC Address from EEPROM
static uint16_t eeprom_read(uint32_t base, uint8_t addr) {
  uint32_t temp = 0;
  mmio_write32(base, E1000_REG_EEPROM, 1 | ((uint32_t)addr << 8));
  while (!((temp = mmio_read32(base, E1000_REG_EEPROM)) & (1 << 4)))
    ; // Wait for done
  return (uint16_t)((temp >> 16) & 0xFFFF);
}

static void read_mac_address() {
  uint16_t t;
  t = eeprom_read(e1000_mmio_base, 0);
  mac_addr[0] = t & 0xFF;
  mac_addr[1] = t >> 8;
  t = eeprom_read(e1000_mmio_base, 1);
  mac_addr[2] = t & 0xFF;
  mac_addr[3] = t >> 8;
  t = eeprom_read(e1000_mmio_base, 2);
  mac_addr[4] = t & 0xFF;
  mac_addr[5] = t >> 8;

  print_string("[NET] MAC Address: ", 0x0F);
  for (int i = 0; i < 6; i++) {
    print_hex(mac_addr[i], 0x0F);
    if (i < 5)
      print_string(":", 0x0F);
  }
  print_string("\n", 0x0F);
}

static void e1000_setup_rx() {
  // Allocate 16-byte aligned memory for descriptors (bump allocator should be
  // aligned fine)
  rx_descs = (e1000_rx_desc *)mem_alloc(
      sizeof(e1000_rx_desc) * E1000_NUM_RX_DESC + 16);
  // Align to 16 bytes
  rx_descs = (e1000_rx_desc *)(((uint32_t)rx_descs + 15) & ~15);

  // Allocate memory for packets
  for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
    rx_ptr = (uint8_t *)mem_alloc(8192 + 16);
    rx_descs[i].addr = (uint32_t)rx_ptr;
    rx_descs[i].status = 0;
  }

  mmio_write32(e1000_mmio_base, E1000_REG_RDBAL, (uint32_t)rx_descs);
  mmio_write32(e1000_mmio_base, E1000_REG_RDBAH, 0);
  mmio_write32(e1000_mmio_base, E1000_REG_RDLEN, E1000_NUM_RX_DESC * 16);
  mmio_write32(e1000_mmio_base, E1000_REG_RDH, 0);
  mmio_write32(e1000_mmio_base, E1000_REG_RDT, E1000_NUM_RX_DESC - 1);

  // Enable RX: EN (bit 1) | Store Bad Packets (bit 2) | Broadcast Accept (bit
  // 15) | Strip CRC (bit 26)
  mmio_write32(e1000_mmio_base, E1000_REG_RCTL,
               (1 << 1) | (1 << 2) | (1 << 15) | (1 << 26));
}

static void e1000_setup_tx() {
  tx_descs = (e1000_tx_desc *)mem_alloc(
      sizeof(e1000_tx_desc) * E1000_NUM_TX_DESC + 16);
  tx_descs = (e1000_tx_desc *)(((uint32_t)tx_descs + 15) & ~15);

  for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
    tx_ptr = (uint8_t *)mem_alloc(8192 + 16);
    tx_descs[i].addr = (uint32_t)tx_ptr;
    tx_descs[i].cmd = 0;
  }

  mmio_write32(e1000_mmio_base, E1000_REG_TDBAL, (uint32_t)tx_descs);
  mmio_write32(e1000_mmio_base, E1000_REG_TDBAH, 0);
  mmio_write32(e1000_mmio_base, E1000_REG_TDLEN, E1000_NUM_TX_DESC * 16);
  mmio_write32(e1000_mmio_base, E1000_REG_TDH, 0);
  mmio_write32(e1000_mmio_base, E1000_REG_TDT, 0);

  // Enable TX Setup
  // TCTL_EN (bit 1) | TCTL_PSP (bit 3)
  mmio_write32(e1000_mmio_base, E1000_REG_TCTL, (1 << 1) | (1 << 3));
  mmio_write32(e1000_mmio_base, E1000_REG_TIPG,
               0x0060200A); // Inter packet gap (IEEE 802.3 mode)
}

void e1000_init() {
  if (!pci_e1000_found)
    return;

  print_string("[NET] Initializing E1000 Network Driver...\n", 0x0B);

  // Clear Multicast Table Array
  for (int i = 0; i < 128; i++) {
    mmio_write32(e1000_mmio_base, E1000_REG_MTA + (i * 4), 0);
  }

  read_mac_address();
  e1000_setup_rx();
  e1000_setup_tx();

  // Link Up
  uint32_t ctrl = mmio_read32(e1000_mmio_base, E1000_REG_CTRL);
  mmio_write32(e1000_mmio_base, E1000_REG_CTRL, ctrl | 0x40); // Set Link Up

  // Clear Interrupt Mask
  mmio_write32(e1000_mmio_base, E1000_REG_IMS,
               0x1F6DC);                       // Enable common interrupts
  mmio_read32(e1000_mmio_base, E1000_REG_ICR); // Read to clear current ints

  print_string("[NET] E1000 Driver Online (TX/RX Rings set up!).\n", 0x0A);
}

// Transmit Raw Packet
void net_send_packet(const uint8_t *payload, uint16_t length) {
  if (!pci_e1000_found)
    return;

  tx_descs[tx_cur].addr = (uint32_t)payload;
  tx_descs[tx_cur].length = length;
  // CMD: EOP (bit 0), IFCS (bit 1), RS (bit 3)
  tx_descs[tx_cur].cmd = (1 << 0) | (1 << 1) | (1 << 3);
  tx_descs[tx_cur].status = 0;

  uint8_t old_cur = tx_cur;
  tx_cur = (tx_cur + 1) % E1000_NUM_TX_DESC;
  mmio_write32(e1000_mmio_base, E1000_REG_TDT, tx_cur);

  // Wait until Complete (DD bit 0)
  while (!(tx_descs[old_cur].status & 0x01))
    ;

  serial_print_string("[NET] Packet transmitted successfully!\n");
}

extern void net_receive_packet(uint8_t *packet, uint16_t length);

void e1000_poll() {
  if (!pci_e1000_found)
    return;

  // RX Descriptor Status Bit 0 represents DD (Descriptor Done)
  while (rx_descs[rx_cur].status & 0x01) {
    uint8_t *packet = (uint8_t *)rx_descs[rx_cur].addr;
    uint16_t length = rx_descs[rx_cur].length;

    if (!(rx_descs[rx_cur].errors)) {
      // Pass it to TCP/IP Stack
      net_receive_packet(packet, length);
    }

    // Reset status and tell E1000 we consumed it
    rx_descs[rx_cur].status = 0;

    uint16_t next_rx = (rx_cur + 1) % E1000_NUM_RX_DESC;
    mmio_write32(e1000_mmio_base, E1000_REG_RDT, rx_cur); // RDT trails RDH
    rx_cur = next_rx;
  }
}
