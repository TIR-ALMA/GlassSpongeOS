// net/ip.c
#include "ip.h"
#include "arp.h"
#include "icmp.h"
#include "ethernet.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "mm.h"

struct ip_stats ip_stats = {0};
static uint32_t local_ip = 0xC0A80102; // 192.168.1.2
static uint8_t local_mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

void ip_init(void) {
    printf("[IP] Initializing IPv4 stack...\n");
    memset(&ip_stats, 0, sizeof(ip_stats));
}

uint32_t ip_get_local_ip(void) {
    return local_ip;
}

void ip_set_local_ip(uint32_t ip) {
    local_ip = ip;
}

uint16_t ip_checksum(const void* data, size_t len) {
    const uint16_t* buf = (const uint16_t*)data;
    uint32_t sum = 0;
    size_t i;

    for (i = 0; i < len / 2; i++) {
        sum += ntohs(buf[i]);
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
    }

    if (len & 1) {
        sum += ((const uint8_t*)data)[len - 1] << 8;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)~sum;
}

int ip_validate_header(const struct ip_header* hdr, size_t total_len) {
    if ((hdr->version_ihl >> 4) != 4) {
        ip_stats.in_hdr_errors++;
        return -1;
    }

    uint8_t ihl = hdr->version_ihl & 0x0F;
    if (ihl < 5 || ihl > 15) {
        ip_stats.in_hdr_errors++;
        return -1;
    }

    uint16_t pkt_len = ntohs(hdr->total_len);
    if (pkt_len < 20 || pkt_len > 65535 || pkt_len != total_len) {
        ip_stats.in_hdr_errors++;
        return -1;
    }

    uint16_t calc = ip_checksum(hdr, ihl * 4);
    if (calc != 0) {
        ip_stats.in_hdr_errors++;
        return -1;
    }

    if (hdr->ttl == 0) {
        ip_stats.in_discards++;
        return -1;
    }

    uint32_t src = ntohl(hdr->src_ip);
    if (src == 0 || src == 0xFFFFFFFF) {
        ip_stats.in_addr_errors++;
        return -1;
    }

    return 0;
}

int ip_is_broadcast(uint32_t ip) {
    if (ip == 0xFFFFFFFF) return 1; // Limited broadcast
    uint32_t net = local_ip & 0xFFFFFF00;
    if (ip == (net | 0xFF)) return 1; // Directed broadcast
    return 0;
}

int ip_is_multicast(uint32_t ip) {
    return (ip & 0xF0000000) == 0xE0000000; // 224.0.0.0/4
}

int ip_is_loopback(uint32_t ip) {
    return (ip & 0xFF000000) == 0x7F000000; // 127.0.0.0/8
}

int ip_is_private(uint32_t ip) {
    uint32_t a = ip & 0xFF000000;
    uint32_t b = ip & 0xFFFF0000;
    uint32_t c = ip & 0xFFFFFF00;

    if (a == 0x0A000000) return 1;          // 10.0.0.0/8
    if (b == 0xAC100000) return 1;          // 172.16.0.0/12
    if (c == 0xC0A80000) return 1;          // 192.168.0.0/16
    return 0;
}

int ip_send_packet(uint32_t dst_ip, uint8_t proto, const void* data, size_t len) {
    if (!data || len == 0) return -1;

    size_t ip_hdr_len = 20;
    size_t pkt_size = ip_hdr_len + len;
    uint8_t* pkt_buf = (uint8_t*)kmalloc(pkt_size);
    if (!pkt_buf) {
        ip_stats.out_discards++;
        return -1;
    }

    struct ip_header* iph = (struct ip_header*)pkt_buf;
    uint8_t* payload = pkt_buf + ip_hdr_len;

    iph->version_ihl = (4 << 4) | 5; // IPv4, IHL=5
    iph->dscp_ecn = 0;
    iph->total_len = htons(pkt_size);
    iph->ident = htons((uint16_t)(timer_get_ticks() & 0xFFFF));
    iph->frag_off = htons(0); // No fragmentation
    iph->ttl = 64;
    iph->protocol = proto;
    iph->src_ip = htonl(local_ip);
    iph->dst_ip = htonl(dst_ip);

    memcpy(payload, data, len);

    iph->checksum = 0;
    iph->checksum = ip_checksum(iph, ip_hdr_len);

    struct ethernet_frame frame;
    memset(&frame, 0, sizeof(frame));

    uint8_t* dest_mac = arp_resolve(dst_ip);
    if (!dest_mac) {
        printf("[IP] ARP resolution failed for %x\n", dst_ip);
        kfree(pkt_buf);
        ip_stats.out_no_routes++;
        return -1;
    }

    memcpy(frame.dest_mac, dest_mac, 6);
    memcpy(frame.src_mac, local_mac, 6);
    frame.ethertype = htons(ETH_PROTO_IP);
    frame.payload = pkt_buf;
    frame.payload_len = pkt_size;

    int ret = ethernet_send_frame(&frame);
    if (ret < 0) {
        ip_stats.out_discards++;
        kfree(pkt_buf);
        return ret;
    }

    kfree(pkt_buf);
    ip_stats.out_requests++;
    return (int)len;
}

void ip_input(struct ethernet_frame* frame) {
    if (!frame || frame->payload_len < 20) {
        ip_stats.in_hdr_errors++;
        return;
    }

    struct ip_header* iph = (struct ip_header*)frame->payload;
    size_t total_len = ntohs(iph->total_len);

    if (ip_validate_header(iph, frame->payload_len) < 0) {
        return;
    }

    ip_stats.in_packets++;
    ip_stats.in_bytes += total_len;

    uint32_t src_ip = ntohl(iph->src_ip);
    uint32_t dst_ip = ntohl(iph->dst_ip);

    arp_cache_add(src_ip, frame->src_mac);

    if (dst_ip != local_ip && !ip_is_broadcast(dst_ip) && !ip_is_multicast(dst_ip)) {
        ip_stats.in_discards++;
        return;
    }

    uint8_t proto = iph->protocol;
    size_t hdr_len = (iph->version_ihl & 0x0F) * 4;
    uint8_t* payload = frame->payload + hdr_len;

    switch (proto) {
        case IP_PROTO_ICMP:
            icmp_handle_packet(iph); // Pass full IP packet
            break;
        case IP_PROTO_TCP:
            // tcp_handle_packet(iph);
            break;
        case IP_PROTO_UDP:
            // udp_handle_packet(iph);
            break;
        default:
            ip_stats.in_unknown_protos++;
            // Send ICMP Protocol Unreachable
            icmp_send_dest_unreachable(src_ip, ICMP_PROTOCOL_UNREACHABLE, iph, payload, frame->payload_len - hdr_len);
            break;
    }

    ip_stats.in_delivers++;
}

// === НОВОЕ: Универсальный вход для драйверов ===
void ip_input_from_driver(uint8_t* frame_data, uint32_t length) {
    if (length < 14) return; // Слишком маленький фрейм

    uint16_t ethertype = (frame_data[12] << 8) | frame_data[13];
    if (ethertype == 0x0800) { // IP
        struct ethernet_frame temp_frame;
        memcpy(temp_frame.dest_mac, frame_data, 6);
        memcpy(temp_frame.src_mac, frame_data + 6, 6);
        temp_frame.ethertype = ethertype;
        temp_frame.payload = frame_data + 14;
        temp_frame.payload_len = length - 14;
        
        ip_input(&temp_frame);
    }
}

