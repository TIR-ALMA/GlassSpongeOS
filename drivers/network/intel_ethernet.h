#ifndef _INTEL_ETHERNET_H
#define _INTEL_ETHERNET_H

#include "types.h"

#define INTEL_ETH_MAX_DEVICES 4
#define INTEL_ETH_NUM_RX_DESC 32
#define INTEL_ETH_NUM_TX_DESC 32

// Forward declaration
struct intel_ethernet_device;

// Public API functions
int intel_ethernet_init(void);
int intel_ethernet_send_packet(void *packet, uint32_t length);
uint32_t intel_ethernet_receive_packet(void *buffer, uint32_t max_length);
int intel_ethernet_get_mac_address(uint8_t *mac_addr);
int intel_ethernet_is_link_up(void);
void intel_ethernet_interrupt_handler(void);

// Helper functions
struct intel_ethernet_device* intel_ethernet_find_device(void);

#endif /* _INTEL_ETHERNET_H */

