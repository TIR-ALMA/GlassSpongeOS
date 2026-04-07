#ifndef ARP_H
#define ARP_H

#include "network.h"

#define ARP_CACHE_SIZE 32

struct arp_cache_entry {
    uint32_t ip;
    uint8_t mac[6];
    uint8_t valid;
    uint32_t last_used;
};

extern struct arp_cache_entry arp_cache[ARP_CACHE_SIZE];

void arp_init(void);
int arp_request(uint32_t target_ip);
int arp_reply(uint32_t sender_ip, uint8_t* sender_mac, uint32_t target_ip, uint8_t* target_mac);
uint8_t* arp_resolve(uint32_t ip);
void arp_cache_add(uint32_t ip, uint8_t* mac);
void arp_handle_packet(struct ethernet_frame* frame);

#endif
