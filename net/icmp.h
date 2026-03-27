// net/icmp.h
#ifndef ICMP_H
#define ICMP_H

#include "types.h"
#include "ip.h"

#define ICMP_ECHO_REPLY          0
#define ICMP_DEST_UNREACHABLE    3
#define ICMP_SOURCE_QUENCH       4
#define ICMP_REDIRECT            5
#define ICMP_ECHO_REQUEST        8
#define ICMP_TIME_EXCEEDED       11
#define ICMP_PARAMETER_PROBLEM   12
#define ICMP_TIMESTAMP_REQUEST   13
#define ICMP_TIMESTAMP_REPLY     14

#define ICMP_NET_UNREACHABLE     0
#define ICMP_HOST_UNREACHABLE    1
#define ICMP_PROTOCOL_UNREACHABLE 2
#define ICMP_PORT_UNREACHABLE    3
#define ICMP_FRAG_NEEDED         4

#define ICMP_TTL_EXCEEDED        0
#define ICMP_FRAG_REASSEMBLY_TIMEOUT 1

struct icmp_header {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    union {
        struct {
            uint16_t identifier;
            uint16_t sequence_number;
        } echo;
        struct {
            uint32_t unused;
        } unreachable;
        struct {
            uint32_t unused;
        } time_exceeded;
    } data;
} __attribute__((packed));

struct icmp_error_message {
    struct ip_header original_ip_header;
    uint8_t original_data[8];
} __attribute__((packed));

void icmp_init(void);
int icmp_send_echo_request(uint32_t dest_ip, uint16_t id, uint16_t seq, void* data, size_t data_len);
int icmp_send_echo_reply(uint32_t dest_ip, uint16_t id, uint16_t seq, void* data, size_t data_len);
int icmp_send_dest_unreachable(uint32_t dest_ip, uint8_t code, struct ip_header* original_ip, void* original_data, size_t original_data_len);
int icmp_send_time_exceeded(uint32_t dest_ip, uint8_t code, struct ip_header* original_ip, void* original_data, size_t original_data_len);
void icmp_handle_packet(struct ip_header* iph);
uint16_t icmp_calculate_checksum(struct icmp_header* icmph, size_t total_len);

#endif

