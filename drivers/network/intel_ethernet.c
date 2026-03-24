#include "drivers/network/intel_ethernet.h"
#include "mm.h"
#include "lib/string.h"
#include "lib/printf.h"

// i350 (82580) и X710/XL710 используют разные MAC контроллеры
#define INTEL_I350_VENDOR_ID 0x8086
#define INTEL_X710_VENDOR_ID 0x8086

// i350 (82580) Device IDs
#define INTEL_I350_DEVICE_ID_1 0x1521  // i350-AM2
#define INTEL_I350_DEVICE_ID_2 0x1522  // i350-AM4
#define INTEL_I350_DEVICE_ID_3 0x1531  // i350-BT2
#define INTEL_I350_DEVICE_ID_4 0x1533  // i350-BT4

// X710/XL710 Device IDs
#define INTEL_X710_DEVICE_ID_1 0x157B  // X710
#define INTEL_X710_DEVICE_ID_2 0x157C  // XXV710
#define INTEL_XL710_DEVICE_ID_1 0x1583  // XL710-QDA2
#define INTEL_XL710_DEVICE_ID_2 0x1584  // XL710-QDA1

// Common registers offsets
#define E1000_CTRL      0x00000
#define E1000_STATUS    0x00008
#define E1000_EEPROM    0x00014
#define E1000_RCTL      0x00100
#define E1000_TCTL      0x00400
#define E1000_RDBAL     0x02800
#define E1000_RDBAH     0x02804
#define E1000_RDLEN     0x02808
#define E1000_RDH       0x02810
#define E1000_RDT       0x02818
#define E1000_TDBAL     0x03800
#define E1000_TDBAH     0x03804
#define E1000_TDLEN     0x03808
#define E1000_TDH       0x03810
#define E1000_TDT       0x03818
#define E1000_RXDCTL    0x028281000_TXDCTL    0x03828
#define E1000_EICR      0x01580  // Extended Interrupt Cause Read
#define E1000_EIMS      0x01588  // Extended Interrupt Mask Set
#define E1000_MTA       0x05200  // Multicast Table Array
#define E1000_RA        0x05400  // Receive Address Register

// X710/XL710 specific registers
#define E1000_FLEX_MNG_PT_CTL 0x05F18
#define E1000_INIT_CTRL_STATUS 0xB8000
#define E1000_PFPM_APM            0xB8004
#define E1000_FWSM              0x0BB00
#define E1000_HICR              0x08F00

// Register bits
#define E1000_CTRL_SLU     0x00000040  // Set link up
#define E1000_CTRL_RFCE    0x08000000  // RX Flow Control Enable
#define E1000_CTRL_TFCE    0x04000000  // TX Flow Control Enable
#define E1000_RCTL_EN      0x00000002  // Receiver Enable
#define E1000_RCTL_BAM     0x00008000  // Broadcast Accept Mode
#define E1000_RCTL_SZ_2048 0x00000000  // RBUF size: 2048 bytes
#define E1000_RCTL_SECRC   0x04000000  // Strip Ethernet CRC
#define E1000_TCTL_EN      0x00000002  // Transmit Enable
#define E1000_TCTL_PSP     0x00000008  // Pad Short Packets
#define E1000_TCTL_CT      0x00000FF0  // Collision Threshold
#define E1000_TCTL_COLD    0x003FF000  // Collision Distance

// Descriptor status bits
#define E1000_RXD_STAT_DD  0x01        // Descriptor Done
#define E1000_RXD_STAT_EOP 0x02        // End of Packet
#define E1000_TXD_CMD_EOP  0x01        // End of Packet
#define E1000_TXD_CMD_RS   0x08        // Report Status

// Descriptor structures
struct e1000_rx_desc {
    uint64_t buffer_addr;   // Address of the descriptor's data buffer
    uint16_t length;        // Length of data DMAed into data buffer
    uint16_t checksum;      // Packet checksum
    uint8_t  status;        // Descriptor status
    uint8_t  errors;        // Descriptor Errors
    uint16_t special;
};

struct e1000_tx_desc {
    uint64_t buffer_addr;   // Address of the descriptor's data buffer
    uint16_t length;        // Data buffer length
    uint8_t  cso;          // Checksum offset
    uint8_t  cmd;          // Command type
    uint8_t  status;       // Descriptor status
    uint8_t  css;          // Checksum start
    uint16_t special;
};

// Driver state
struct intel_ethernet_device {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint32_t base_address;
    uint8_t mac_addr[6];
    uint8_t irq_line;
    
    // RX descriptors
    struct e1000_rx_desc *rx_descs;
    uint8_t *rx_buffers[INTEL_ETH_NUM_RX_DESC];
    volatile uint32_t rx_index;
    
    // TX descriptors  
    struct e1000_tx_desc *tx_descs;
    uint8_t *tx_buffers[INTEL_ETH_NUM_TX_DESC];
    volatile uint32_t tx_index;
    volatile uint32_t tx_clean_index;
    
    // Flags
    uint8_t initialized;
    uint8_t link_up;
    uint8_t is_x710;  // 1 for X710/XL710, 0 for i350
};

static struct intel_ethernet_device intel_devices[INTEL_ETH_MAX_DEVICES];
static uint8_t num_intel_devices = 0;

// Function prototypes
static uint32_t intel_read_reg(uint32_t base_addr, uint32_t reg);
static void intel_write_reg(uint32_t base_addr, uint32_t reg, uint32_t value);
static int intel_init_device(struct intel_ethernet_device *dev);
static void intel_configure_rx(struct intel_ethernet_device *dev);
static void intel_configure_tx(struct intel_ethernet_device *dev);
static void intel_setup_descriptors(struct intel_ethernet_device *dev);
static int intel_detect_device(uint8_t bus, uint8_t device, uint8_t function);
static void intel_reset_device(struct intel_ethernet_device *dev);

int intel_ethernet_init() {
    printf("Initializing Intel Ethernet driver...\n");
    
    // Scan PCI for Intel network devices
    for(uint8_t bus = 0; bus < 256; bus++) {
        for(uint8_t device = 0; device < 32; device++) {
            for(uint8_t function = 0; function < 8; function++) {
                uint32_t vendor_device = pci_read_reg(bus, device, function, 0x00);
                uint16_t vendor_id = vendor_device & 0xFFFF;
                uint16_t device_id = vendor_device >> 16;
                
                if(vendor_id == INTEL_I350_VENDOR_ID || vendor_id == INTEL_X710_VENDOR_ID) {
                    if(intel_detect_device(bus, device, function)) {
                        if(num_intel_devices < INTEL_ETH_MAX_DEVICES) {
                            struct intel_ethernet_device *dev = &intel_devices[num_intel_devices];
                            
                            dev->bus = bus;
                            dev->device = device;
                            dev->function = function;
                            dev->base_address = pci_read_reg(bus, device, function, 0x10) & ~0x0F; // BAR0
                            
                            // Read IRQ line
                            uint8_t header_type = (pci_read_reg(bus, device, function, 0x0C) >> 16) & 0xFF;
                            if(header_type == 0) {
                                dev->irq_line = (pci_read_reg(bus, device, function, 0x3C) >> 8) & 0xFF;
                            }
                            
                            // Determine if it's X710/XL710 or i350
                            dev->is_x710 = (device_id >= 0x157B && device_id <= 0x1584) ? 1 : 0;
                            
                            if(intel_init_device(dev)) {
                                num_intel_devices++;
                                printf("Intel Ethernet device initialized: %s at %d:%d.%d\n", 
                                       dev->is_x710 ? "X710/XL710" : "i350", 
                                       bus, device, function);
                            }
                        }
                    }
                }
            }
        }
    }
    
    printf("Intel Ethernet driver initialization complete. Found %d devices.\n", num_intel_devices);
    return num_intel_devices > 0 ? 0 : -1;
}

static int intel_detect_device(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t vendor_device = pci_read_reg(bus, device, function, 0x00);
    uint16_t vendor_id = vendor_device & 0xFFFF;
    uint16_t device_id = vendor_device >> 16;
    
    // Check for i350 series
    if(vendor_id == INTEL_I350_VENDOR_ID) {
        switch(device_id) {
            case INTEL_I350_DEVICE_ID_1:
            case INTEL_I350_DEVICE_ID_2:
            case INTEL_I350_DEVICE_ID_3:
            case INTEL_I350_DEVICE_ID_4:
                return 1;
        }
    }
    
    // Check for X710/XL710 series
    if(vendor_id == INTEL_X710_VENDOR_ID) {
        switch(device_id) {
            case INTEL_X710_DEVICE_ID_1:
            case INTEL_X710_DEVICE_ID_2:
            case INTEL_XL710_DEVICE_ID_1:
            case INTEL_XL710_DEVICE_ID_2:
                return 1;
        }
    }
    
    return 0;
}

static int intel_init_device(struct intel_ethernet_device *dev) {
    // Reset device
    intel_reset_device(dev);
    
    // Read MAC address
    uint32_t low_mac = intel_read_reg(dev->base_address, E1000_RA);
    uint32_t high_mac = intel_read_reg(dev->base_address, E1000_RA + 4);
    
    dev->mac_addr[0] = low_mac & 0xFF;
    dev->mac_addr[1] = (low_mac >> 8) & 0xFF;
    dev->mac_addr[2] = (low_mac >> 16) & 0xFF;
    dev->mac_addr[3] = (low_mac >> 24) & 0xFF;
    dev->mac_addr[4] = high_mac & 0xFF;
    dev->mac_addr[5] = (high_mac >> 8) & 0xFF;
    
    printf("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
           dev->mac_addr[0], dev->mac_addr[1], dev->mac_addr[2],
           dev->mac_addr[3], dev->mac_addr[4], dev->mac_addr[5]);
    
    // Setup descriptors
    intel_setup_descriptors(dev);
    
    // Configure RX
    intel_configure_rx(dev);
    
    // Configure TX
    intel_configure_tx(dev);
    
    // Enable interrupts
    intel_write_reg(dev->base_address, E1000_EIMS, 0x1); // Enable all interrupts
    
    // Enable link
    uint32_t ctrl = intel_read_reg(dev->base_address, E1000_CTRL);
    ctrl |= E1000_CTRL_SLU | E1000_CTRL_RFCE | E1000_CTRL_TFCE;
    intel_write_reg(dev->base_address, E1000_CTRL, ctrl);
    
    dev->initialized = 1;
    dev->link_up = 1;
    
    return 1;
}

static void intel_setup_descriptors(struct intel_ethernet_device *dev) {
    // Allocate RX descriptors
    dev->rx_descs = (struct e1000_rx_desc*)get_free_page();
    if(!dev->rx_descs) return;
    
    // Allocate TX descriptors
    dev->tx_descs = (struct e1000_tx_desc*)get_free_page();
    if(!dev->tx_descs) return;
    
    // Allocate RX buffers
    for(int i = 0; i < INTEL_ETH_NUM_RX_DESC; i++) {
        dev->rx_buffers[i] = (uint8_t*)get_free_page();
        if(!dev->rx_buffers[i]) return;
        
        // Initialize RX descriptor
        dev->rx_descs[i].buffer_addr = (uint64_t)dev->rx_buffers[i];
        dev->rx_descs[i].status = 0;
    }
    
    // Allocate TX buffers
    for(int i = 0; i < INTEL_ETH_NUM_TX_DESC; i++) {
        dev->tx_buffers[i] = (uint8_t*)get_free_page();
        if(!dev->tx_buffers[i]) return;
        
        // Initialize TX descriptor
        dev->tx_descs[i].buffer_addr = (uint64_t)dev->tx_buffers[i];
        dev->tx_descs[i].status = 0;
    }
    
    // Setup RX descriptor ring
    intel_write_reg(dev->base_address, E1000_RDBAL, (uint32_t)(uint64_t)dev->rx_descs);
    intel_write_reg(dev->base_address, E1000_RDBAH, (uint32_t)((uint64_t)dev->rx_descs >> 32));
    intel_write_reg(dev->base_address, E1000_RDLEN, INTEL_ETH_NUM_RX_DESC * sizeof(struct e1000_rx_desc));
    intel_write_reg(dev->base_address, E1000_RDH, 0);
    intel_write_reg(dev->base_address, E1000_RDT, INTEL_ETH_NUM_RX_DESC - 1);
    
    // Setup TX descriptor ring
    intel_write_reg(dev->base_address, E1000_TDBAL, (uint32_t)(uint64_t)dev->tx_descs);
    intel_write_reg(dev->base_address, E1000_TDBAH, (uint32_t)((uint64_t)dev->tx_descs >> 32));
    intel_write_reg(dev->base_address, E1000_TDLEN, INTEL_ETH_NUM_TX_DESC * sizeof(struct e1000_tx_desc));
    intel_write_reg(dev->base_address, E1000_TDH, 0);
    intel_write_reg(dev->base_address, E1000_TDT, 0);
    
    dev->rx_index = 0;
    dev->tx_index = 0;
    dev->tx_clean_index = 0;
}

static void intel_configure_rx(struct intel_ethernet_device *dev) {
    // Configure RX control register
    uint32_t rctl = E1000_RCTL_EN |      // Enable receiver
                   E1000_RCTL_BAM |      // Accept broadcast
                   E1000_RCTL_SZ_2048 |  // 2048-byte buffers
                   E1000_RCTL_SECRC;     // Strip CRC
    
    intel_write_reg(dev->base_address, E1000_RCTL, rctl);
    
    // Enable RX descriptor ring
    intel_write_reg(dev->base_address, E1000_RXDCTL, 00000); // Enable RXDCTL
}

static void intel_configure_tx(struct intel_ethernet_device *dev) {
    // Configure TX control register
    uint32_t tctl = E1000_TCTL_EN |      // Enable transmitter
                   E1000_TCTL_PSP |      // Pad short packets
                   (0x10 << 4) |         // Collision threshold (0x10)
                   (0x40 << 12);         // Collision distance (0x40)
    
    intel_write_reg(dev->base_address, E1000_TCTL, tctl);
    
    // Enable TX descriptor ring
    intel_write_reg(dev->base_address, E1000_TXDCTL, 0x2000000); // Enable TXDCTL
}

static void intel_reset_device(struct intel_ethernet_device *dev) {
    // Software reset
    intel_write_reg(dev->base_address, E1000_CTRL, 0x04008002);
    
    // Wait for reset to complete
    for(int i = 0; i < 1000; i++) {
        if(!(intel_read_reg(dev->base_address, E1000_CTRL) & 0x04008002)) {
            break;
        }
    }
}

int intel_ethernet_transmit(struct intel_ethernet_device *dev, void *packet, uint32_t length) {
    if(!dev || !dev->initialized || length > 2048) {
        return -1;
    }
    
    uint32_t next_index = (dev->tx_index + 1) % INTEL_ETH_NUM_TX_DESC;
    
    // Check if there's space in the TX ring
    if(next_index == dev->tx_clean_index) {
        return -1; // Ring full
    }
    
    // Copy packet to TX buffer
    memcpy(dev->tx_buffers[dev->tx_index], packet, length);
    
    // Setup TX descriptor
    dev->tx_descs[dev->tx_index].length = length;
    dev->tx_descs[dev->tx_index].cso = 0;
    dev->tx_descs[dev->tx_index].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS; // End of packet, report status
    dev->tx_descs[dev->tx_index].css = 0;
    dev->tx_descs[dev->tx_index].status = 0;
    
    // Update TX tail pointer
    intel_write_reg(dev->base_address, E1000_TDT, next_index);
    
    dev->tx_index = next_index;
    
    return length;
}

uint32_t intel_ethernet_receive(struct intel_ethernet_device *dev, void *buffer, uint32_t max_length) {
    if(!dev || !dev->initialized) {
        return 0;
    }
    
    uint32_t rx_index = intel_read_reg(dev->base_address, E1000_RDT);
    rx_index = (rx_index + 1) % INTEL_ETH_NUM_RX_DESC;
    
    if(!(dev->rx_descs[rx_index].status & E1000_RXD_STAT_DD)) {
        return 0; // No packet available
    }
    
    uint32_t packet_length = dev->rx_descs[rx_index].length;
    if(packet_length > max_length) {
        packet_length = max_length;
    }
    
    // Copy received packet to buffer
    memcpy(buffer, dev->rx_buffers[rx_index], packet_length);
    
    // Clear descriptor status
    dev->rx_descs[rx_index].status = 0;
    
    // Update RX tail pointer
    intel_write_reg(dev->base_address, E1000_RDT, rx_index);
    
    return packet_length;
}

void intel_ethernet_interrupt_handler() {
    // This would be called from the main interrupt handler when the IRQ fires
    // For now, just acknowledge the interrupt
    intel_read_reg(intel_devices[0].base_address, E1000_EICR);
    
    // Process received packets
    uint8_t packet_buffer[2048];
    uint32_t length;
    
    while((length = intel_ethernet_receive(&intel_devices[0], packet_buffer, sizeof(packet_buffer))) > 0) {
        // Process received packet
        // For now, just print packet length
        printf("Received packet: %d bytes\n", length);
    }
}

// Helper functions
static uint32_t intel_read_reg(uint32_t base_addr, uint32_t reg) {
    return *(volatile uint32_t*)(base_addr + reg);
}

static void intel_write_reg(uint32_t base_addr, uint32_t reg, uint32_t value) {
    *(volatile uint32_t*)(base_addr + reg) = value;
}

// API functions for the kernel
int intel_ethernet_send_packet(void *packet, uint32_t length) {
    if(num_intel_devices > 0) {
        return intel_ethernet_transmit(&intel_devices[0], packet, length);
    }
    return -1;
}

uint32_t intel_ethernet_receive_packet(void *buffer, uint32_t max_length) {
    if(num_intel_devices > 0) {
        return intel_ethernet_receive(&intel_devices[0], buffer, max_length);
    }
    return 0;
}

int intel_ethernet_get_mac_address(uint8_t *mac_addr) {
    if(num_intel_devices > 0) {
        memcpy(mac_addr, intel_devices[0].mac_addr, 6);
        return 0;
    }
    return -1;
}

int intel_ethernet_is_link_up() {
    if(num_intel_devices > 0) {
        return intel_devices[0].link_up;
    }
    return 0;
}
