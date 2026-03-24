#ifndef _RTL8168_H
#define _RTL8168_H

#include "types.h"

struct rx_desc {
    uint32_t addr_lo;
    uint32_t addr_hi;
    uint32_t size;
    uint32_t status;
};

struct tx_desc {
    uint32_t addr_lo;
    uint32_t addr_hi;
    uint32_t size;
    uint32_t status;
};

// Функции драйвера
int rtl8168_init(uint8_t bus, uint8_t device);
void rtl8168_transmit(void* packet, uint32_t length);
uint32_t rtl8168_poll(void* buffer);
void rtl8168_interrupt_handler(void);
int rtl8168_send_packet(void* packet, uint32_t length);
int rtl8168_receive_packet(void* buffer, uint32_t max_length);
int rtl8168_get_mac_address(uint8_t* mac);
int rtl8168_is_link_up(void);
void find_and_init_rtl8168(void);
void rtl8168_set_multicast_filter(uint32_t hash[2]);
void rtl8168_get_stats(void* stats_buffer);
void rtl8168_set_promiscuous(int enable);
void rtl8168_power_down(void);
void rtl8168_power_up(void);

#endif
