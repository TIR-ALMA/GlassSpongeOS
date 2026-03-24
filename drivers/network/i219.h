#ifndef _I219_H
#define _I219_H

#include "types.h"

int i219_init(uint8_t bus, uint8_t device);
int i219_transmit(void* packet, uint32_t length);
uint32_t i219_poll(void* buffer);
void i219_interrupt_handler();

#endif
