#include "system.h"

// Basic PCI Configuration Space Access via I/O Ports
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

static inline void outl_port(uint16_t port, uint32_t val) {
  __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func,
                                uint8_t offset) {
  uint32_t address;
  uint32_t lbus = (uint32_t)bus;
  uint32_t lslot = (uint32_t)slot;
  uint32_t lfunc = (uint32_t)func;

  // Create configuration address as per PCI spec
  address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) |
                       (offset & 0xFC) | ((uint32_t)0x80000000));

  outl_port(PCI_CONFIG_ADDRESS, address);

  // Read the data
  uint32_t ret = 0;
  __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(PCI_CONFIG_DATA));
  return ret;
}

static void pci_config_write_word(uint8_t bus, uint8_t slot, uint8_t func,
                                  uint8_t offset, uint16_t value) {
  uint32_t address;
  address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) |
                       (offset & 0xFC) | ((uint32_t)0x80000000));

  outl_port(PCI_CONFIG_ADDRESS, address);

  __asm__ volatile("outw %0, %1"
                   :
                   : "a"(value), "Nd"(PCI_CONFIG_DATA + (offset & 2)));
}

uint16_t pci_get_vendor_id(uint8_t bus, uint8_t slot, uint8_t func) {
  uint32_t r0 = pci_config_read(bus, slot, func, 0);
  return r0 & 0xFFFF;
}

uint16_t pci_get_device_id(uint8_t bus, uint8_t slot, uint8_t func) {
  uint32_t r0 = pci_config_read(bus, slot, func, 0);
  return (r0 >> 16) & 0xFFFF;
}

uint32_t pci_get_bar(uint8_t bus, uint8_t slot, uint8_t func, int bar_index) {
  uint8_t offset = 0x10 + (bar_index * 4);
  return pci_config_read(bus, slot, func, offset);
}

void pci_enable_bus_master(uint8_t bus, uint8_t slot, uint8_t func) {
  // Read Command Register (offset 0x04)
  uint32_t cmd_reg = pci_config_read(bus, slot, func, 0x04);
  // Set Bit 2 (Bus Master) and Bit 1 (Memory Space)
  cmd_reg |= 0x04 | 0x02;
  pci_config_write_word(bus, slot, func, 0x04, (uint16_t)(cmd_reg & 0xFFFF));
}

// Global discovered E1000 parameters
int pci_e1000_found = 0;
uint8_t e1000_bus, e1000_slot, e1000_func;
uint32_t e1000_mmio_base;

void pci_init() {
  print_string("[PCI] Enumerating PCI Bus...\n", 0x0E);

  for (uint16_t bus = 0; bus < 256; bus++) {
    for (uint8_t slot = 0; slot < 32; slot++) {
      uint16_t vendor_id = pci_get_vendor_id(bus, slot, 0);
      if (vendor_id == 0xFFFF)
        continue; // Device doesn't exist

      uint16_t device_id = pci_get_device_id(bus, slot, 0);

      // Check for Intel E1000 (82540EM Gigabit Ethernet Controller)
      // Common QEMU Vendor/Device: 0x8086 / 0x100E
      if (vendor_id == 0x8086 && device_id == 0x100E) {
        pci_e1000_found = 1;
        e1000_bus = bus;
        e1000_slot = slot;
        e1000_func = 0;

        // Get BAR0 (MMIO Base)
        uint32_t bar0 = pci_get_bar(bus, slot, 0, 0);
        e1000_mmio_base = bar0 & 0xFFFFFFF0; // Clear lower flags

        print_string("[PCI] Found Intel E1000 NIC! MMIO Base: 0x", 0x0A);
        print_hex(e1000_mmio_base, 0x0A);
        print_string("\n", 0x0A);

        // Enable Bus Mastering Memory Space
        pci_enable_bus_master(bus, slot, 0);
      }
    }
  }

  if (!pci_e1000_found) {
    print_string("[PCI] Warning: Intel E1000 NIC NOT found. Is QEMU setup with "
                 "-device e1000?\n",
                 0x0C);
  }
}
