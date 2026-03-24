#ifndef _INTEL_AX2XX_H
#define _INTEL_AX2XX_H

#include "types.h"

#define MAX_AX2XX_DEVICES 4

// Forward declaration
struct ax2xx_device;

// Public API functions
void ax2xx_init(uint8_t bus, uint8_t device, uint8_t function);
int ax2xx_transmit_packet(void *packet, uint32_t length);
int ax2xx_receive_packet(void *buffer, uint32_t max_len);
void ax_address(uint8_t *mac);
int ax2xx_is_link_up(void);
void ax2xx_scan_pci(void);
void ax2xx_interrupt_handler(void);

// Helper functions
struct ax2xx_device* ax2xx_find_device(void);

#endif /* _INTEL_AX2XX_H */

