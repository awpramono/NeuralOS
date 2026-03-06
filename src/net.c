#include "system.h"

// ============================================================================
// Minimal TCP/IP Stack for NeuralOS (Phase 3)
// Supports: Ethernet II, ARP Replies, IPv4, ICMP Echo Reply (Ping)
// ============================================================================

extern uint8_t mac_addr[6];               // From e1000.c
static uint8_t my_ip[4] = {10, 0, 2, 15}; // QEMU user net default guest IP

// Byte-swap Utilities
uint16_t htons(uint16_t hostshort) {
  return (hostshort >> 8) | (hostshort << 8);
}
#define ntohs htons

uint32_t htonl(uint32_t hostlong) {
  return ((hostlong & 0xFF) << 24) | ((hostlong & 0xFF00) << 8) |
         ((hostlong >> 8) & 0xFF00) | ((hostlong >> 24) & 0xFF);
}
#define ntohl htonl

// ============================================================================
// Network Structures
// ============================================================================

typedef struct {
  uint8_t dest[6];
  uint8_t src[6];
  uint16_t type;
} __attribute__((packed)) eth_header_t;

typedef struct {
  uint16_t hw_type;
  uint16_t proto_type;
  uint8_t hw_len;
  uint8_t proto_len;
  uint16_t opcode;
  uint8_t sender_mac[6];
  uint8_t sender_ip[4];
  uint8_t target_mac[6];
  uint8_t target_ip[4];
} __attribute__((packed)) arp_packet_t;

typedef struct {
  uint8_t ihl_version; // version & Internet Header Length
  uint8_t tos;
  uint16_t total_len;
  uint16_t id;
  uint16_t frag_off;
  uint8_t ttl;
  uint8_t protocol;
  uint16_t checksum;
  uint8_t src_ip[4];
  uint8_t dest_ip[4];
} __attribute__((packed)) ipv4_header_t;

typedef struct {
  uint8_t type;
  uint8_t code;
  uint16_t checksum;
  uint16_t id;
  uint16_t seq;
} __attribute__((packed)) icmp_header_t;

#define ETH_TYPE_ARP 0x0806
#define ETH_TYPE_IPV4 0x0800
#define ARP_REQUEST 1
#define ARP_REPLY 2
#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP 6
#define IP_PROTO_UDP 17
#define ICMP_ECHO_REQ 8
#define ICMP_ECHO_REP 0

// ============================================================================
// Checksum Helpers
// ============================================================================

static uint16_t calculate_checksum(void *vdata, size_t length) {
  uint16_t *data = (uint16_t *)vdata;
  uint32_t sum = 0;

  while (length > 1) {
    sum += *data++;
    length -= 2;
  }

  if (length > 0) {
    sum += *(uint8_t *)data;
  }

  while (sum >> 16) {
    sum = (sum & 0xFFFF) + (sum >> 16);
  }

  return ~sum;
}

// ============================================================================
// Networking Core
// ============================================================================

void net_init() {
  print_string("[NET] Initializing Minimal TCP/IP Stack... IP: ", 0x0A);
  for (int i = 0; i < 4; i++) {
    print_number(my_ip[i], 0x0A);
    if (i < 3)
      print_string(".", 0x0A);
  }
  print_string("\n", 0x0A);
}

void net_handle_arp(eth_header_t *eth, arp_packet_t *arp) {
  if (ntohs(arp->hw_type) != 1 || ntohs(arp->proto_type) != 0x0800)
    return;

  // Check if it's an ARP Request for our IP
  if (ntohs(arp->opcode) == ARP_REQUEST && arp->target_ip[0] == my_ip[0] &&
      arp->target_ip[1] == my_ip[1] && arp->target_ip[2] == my_ip[2] &&
      arp->target_ip[3] == my_ip[3]) {

    serial_print_string("[NET] Received ARP Request. Sending Reply.\n");

    // Prepare ARP Reply buffer
    uint8_t reply_buf[sizeof(eth_header_t) + sizeof(arp_packet_t)];
    eth_header_t *reply_eth = (eth_header_t *)reply_buf;
    arp_packet_t *reply_arp =
        (arp_packet_t *)(reply_buf + sizeof(eth_header_t));

    // Ethernet Header
    for (int i = 0; i < 6; i++) {
      reply_eth->dest[i] = eth->src[i];
      reply_eth->src[i] = mac_addr[i];
    }
    reply_eth->type = htons(ETH_TYPE_ARP);

    // ARP Header
    reply_arp->hw_type = htons(1);
    reply_arp->proto_type = htons(0x0800);
    reply_arp->hw_len = 6;
    reply_arp->proto_len = 4;
    reply_arp->opcode = htons(ARP_REPLY);

    for (int i = 0; i < 6; i++) {
      reply_arp->sender_mac[i] = mac_addr[i];
      reply_arp->target_mac[i] = arp->sender_mac[i];
    }
    for (int i = 0; i < 4; i++) {
      reply_arp->sender_ip[i] = my_ip[i];
      reply_arp->target_ip[i] = arp->sender_ip[i];
    }

    net_send_packet(reply_buf, sizeof(reply_buf));
  }
}

void net_handle_icmp(eth_header_t *eth, ipv4_header_t *ip, icmp_header_t *icmp,
                     uint16_t icmp_len) {
  // Only handle Echo Requests
  if (icmp->type == ICMP_ECHO_REQ && icmp->code == 0) {
    serial_print_string("[NET] Received ICMP Echo (Ping). Sending Reply.\n");

    // Simply reuse the incoming packet buffer to construct reply
    // 1. Swap Ethernet MACs
    for (int i = 0; i < 6; i++) {
      eth->dest[i] = eth->src[i];
      eth->src[i] = mac_addr[i];
    }

    // 2. Swap IP addresses
    for (int i = 0; i < 4; i++) {
      ip->dest_ip[i] = ip->src_ip[i];
      ip->src_ip[i] = my_ip[i];
    }

    // Since we didn't change total_len or anything else in IP,
    // IP checksum remains valid (only swapped src/dest and TTL doesn't affect
    // reply checksum on origin). Safest is to rebuild IP checksum anyway.
    ip->ttl = 64;
    ip->checksum = 0;
    ip->checksum = calculate_checksum(ip, (ip->ihl_version & 0x0F) * 4);

    // 3. Modify ICMP
    icmp->type = ICMP_ECHO_REP;
    // Recompute ICMP checksum
    icmp->checksum = 0;
    icmp->checksum = calculate_checksum(icmp, icmp_len);

    // Send it back (Ethernet Header + IP Header + ICMP payload length)
    net_send_packet((uint8_t *)eth,
                    sizeof(eth_header_t) + ntohs(ip->total_len));
  }
}

void net_handle_ipv4(eth_header_t *eth, ipv4_header_t *ip) {
  // Check if it's for our IP or broadcast
  if ((ip->dest_ip[0] != my_ip[0] || ip->dest_ip[1] != my_ip[1] ||
       ip->dest_ip[2] != my_ip[2] || ip->dest_ip[3] != my_ip[3]) &&
      ip->dest_ip[3] != 255) {
    return;
  }

  uint16_t ip_hdr_len = (ip->ihl_version & 0x0F) * 4;

  if (ip->protocol == IP_PROTO_ICMP) {
    icmp_header_t *icmp = (icmp_header_t *)((uint8_t *)ip + ip_hdr_len);
    uint16_t icmp_len = ntohs(ip->total_len) - ip_hdr_len;
    net_handle_icmp(eth, ip, icmp, icmp_len);
  }
  // TCP and UDP hooks can go here
}

void net_receive_packet(uint8_t *packet, uint16_t length) {
  if (length < sizeof(eth_header_t))
    return;

  eth_header_t *eth = (eth_header_t *)packet;
  uint16_t type = ntohs(eth->type);

  if (type == ETH_TYPE_ARP) {
    arp_packet_t *arp = (arp_packet_t *)(packet + sizeof(eth_header_t));
    net_handle_arp(eth, arp);
  } else if (type == ETH_TYPE_IPV4) {
    ipv4_header_t *ip = (ipv4_header_t *)(packet + sizeof(eth_header_t));
    net_handle_ipv4(eth, ip);
  }
}
