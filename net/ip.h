// net/ip.h
#ifndef IP_H
#define IP_H

#include "types.h"
#include "ethernet.h"

#define IP_PROTO_ICMP   1
#define IP_PROTO_TCP    6
#define IP_PROTO_UDP    17

#define IP_FLAG_DF      (1 << 14)
#define IP_FLAG_MF      (1 << 13)
#define IP_OFFSET_MASK  0x1FFF

struct ip_header {
    uint8_t  version_ihl;
    uint8_t  dscp_ecn;
    uint16_t total_len;
    uint16_t ident;
    uint16_t frag_off;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} __attribute__((packed));

struct ip_stats {
    uint64_t in_packets;
    uint64_t in_bytes;
    uint64_t in_hdr_errors;
    uint64_t in_addr_errors;
    uint64_t in_unknown_protos;
    uint64_t in_discards;
    uint64_t in_delivers;
    uint64_t out_requests;
    uint64_t out_discards;
    uint64_t out_no_routes;
} ip_stats;

void ip_init(void);
int ip_send_packet(uint32_t dst_ip, uint8_t proto, const void* data, size_t len);
void ip_input(struct ethernet_frame* frame);
uint32_t ip_get_local_ip(void);
void ip_set_local_ip(uint32_t ip);
uint16_t ip_checksum(const void* data, size_t len);
int ip_validate_header(const struct ip_header* hdr, size_t total_len);
int ip_is_broadcast(uint32_t ip);
int ip_is_multicast(uint32_t ip);
int ip_is_loopback(uint32_t ip);
int ip_is_private(uint32_t ip);

// === НОВОЕ: Универсальный вход для драйверов ===
void ip_input_from_driver(uint8_t* frame_data, uint32_t length);

#endif

