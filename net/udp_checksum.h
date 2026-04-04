#ifndef UDP_CHECKSUM_H
#define UDP_CHECKSUM_H

#include "types.h"

// Псевдо-заголовок для вычисления UDP-чек-суммы (RFC 768)
struct udp_pseudo_hdr {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint8_t zero;
    uint8_t protocol;
    uint16_t udp_len;
} __attribute__((packed));

uint16_t udp_checksum(const void* data, size_t len,
                      uint32_t src_ip, uint32_t dst_ip, uint16_t udp_len);

#endif

