#ifndef _RTL8822_H
#define _RTL8822_H

#include "types.h"

struct rtl8822_device {
    uint8_t bus;
    uint8_t device;
    uint32_t mmio_base;
    uint8_t mac_address[6];
    int initialized;
};

extern struct rtl8822_device rtl8822_dev;

void rtl8822_init(uint8_t bus, uint8_t device);
int rtl8822_transmit(void* packet, uint32_t length);
uint32_t rtl8822_poll(void* packet);
void rtl8822_interrupt_handler(void);

#endif
