#include "arp.h"
#include "network.h"
#include "lib/printf.h"
#include "drivers/timer.h"

struct arp_cache_entry arp_cache[ARP_CACHE_SIZE];

void arp_init(void) {
    for(int i = 0; i < ARP_CACHE_SIZE; i++) {
        arp_cache[i].valid = 0;
    }
}

uint8_t* arp_resolve(uint32_t ip) {
    uint32_t now = timer_get_ticks();
    for(int i = 0; i < ARP_CACHE_SIZE; i++) {
        if(arp_cache[i].valid && arp_cache[i].ip == ip) {
            arp_cache[i].last_used = now;
            return arp_cache[i].mac;
        }
    }
    // Cache miss: send request
    arp_request(ip);
    return NULL;
}

int arp_request(uint32_t target_ip) {
    struct ethernet_frame frame;
    struct arp_header* arp = (struct arp_header*)frame.payload;

    memset(&frame, 0, sizeof(frame));
    memset(arp, 0, sizeof(*arp));

    arp->htype = htons(ARP_HW_ETH);
    arp->ptype = htons(ETH_PROTO_IP);
    arp->hlen = 6;
    arp->plen = 4;
    arp->op = htons(ARP_REQUEST);
    memcpy(arp->sha, net_local_mac, 6);
    *(uint32_t*)arp->spa = htonl(net_local_ip);
    *(uint32_t*)arp->tpa = htonl(target_ip);
    memset(arp->tha, 0, 6); // unknown

    memcpy(frame.dest_mac, "\xff\xff\xff\xff\xff\xff", 6); // broadcast
    memcpy(frame.src_mac, net_local_mac, 6);
    frame.ethertype = htons(ETH_PROTO_ARP);
    frame.payload_len = sizeof(*arp);

    return ethernet_send_frame(&frame);
}

void arp_handle_packet(struct ethernet_frame* frame) {
    struct arp_header* arp = (struct arp_header*)frame->payload;
    uint32_t src_ip = ntohl(*(uint32_t*)arp->spa);
    uint32_t tgt_ip = ntohl(*(uint32_t*)arp->tpa);

    if(ntohs(arp->op) == ARP_REPLY && tgt_ip == net_local_ip) {
        arp_cache_add(src_ip, arp->sha);
        printf("ARP resolved %x -> %02x:%02x:%02x:%02x:%02x:%02x\n",
               src_ip,
               arp->sha[0], arp->sha[1], arp->sha[2],
               arp->sha[3], arp->sha[4], arp->sha[5]);
    }

    if(ntohs(arp->op) == ARP_REQUEST && tgt_ip == net_local_ip) {
        arp_reply(src_ip, arp->sha, net_local_ip, net_local_mac);
    }
}
