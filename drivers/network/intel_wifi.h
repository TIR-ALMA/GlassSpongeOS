#ifndef _INTEL_WIFI_H
#define _INTEL_WIFI_H

#include "types.h"

// Forward declaration
struct intel_wifi_device;

// Public API functions
int intel_wifi_init(void);
int intel_wifi_transmit(void *packet, uint32_t length);
int intel_wifi_receive(void *buffer, uint32_t max_length);
int intel_wifi_get_mac_addr(uint8_t *mac);
int intel_wifi_is_connected(void);
void intel_wifi_get_stats(uint32_t *tx_packets, uint32_t *rx_packets, uint32_t *errors);
void intel_wifi_cleanup(void);
int intel_wifi_is_initialized(void);

#endif /* _INTEL_WIFI_H */

