#include "types.h"
#include "mm.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "drivers/network/rtl8168.h"
#include "pci.h"

#define RTL8168_VENDOR_ID 0x10EC
#define RTL8168_DEVICE_ID_1 0x8168  // RTL8111/8168B/C/D
#define RTL8168_DEVICE_ID_2 0x8169  // RTL8111E
#define RTL8168_DEVICE_ID_3 0x8176  // RTL8111G
#define RTL8168_DEVICE_ID_4 0x8178  // RTL8111H
#define RTL8168_DEVICE_ID_5 0x8136  // RTL8101E

// Register definitions
#define RTL8168_TX_DESC_START_LO    0x20
#define RTL8168_TX_DESC_START_HI    0x24
#define RTL8168_RX_DESC_START_LO    0x28
#define RTL8168_RX_DESC_START_HI    0x2C
#define RTL8168_COMMAND             0x37
#define RTL8168_INTERRUPT_STATUS    0x3E
#define RTL8168_INTERRUPT_MASK      0x3C
#define RTL8168_RX_CONFIG           0x44
#define RTL8168_TX_CONFIG           0x40
#define RTL8168_CONFIG_1            0x52
#define RTL8168_CONFIG_3            0x53
#define RTL8168_CONFIG_5            0x55
#define RTL8168_MAR0                0x48  // Multicast Address
#define RTL8168_MAR4                0x4C
#define RTL8168_PHY_ACCESS          0x80
#define RTL8168_PHY_STATUS          0x82
#define RTL8168_PHY_DATA            0x84
#define RTL8168_TIMER_INT           0x50
#define RTL8168_CPLUS_CMD           0x78
#define RTL8168_RMS                 0xDA
#define RTL8168_MAC_ADDR            0x00
#define RTL8168_CHIP_VERSION        0x38

// Command register bits
#define CMD_RST         0x10
#define CMD_RX_ENABLE   0x08
#define CMD_TX_ENABLE   0x04
#define CMD_RX_FIFO     0x02
#define CMD_TX_FIFO     0x01

// Interrupt bits
#define ISR_ROK         0x01    // Receive OK
#define ISR_RER         0x02    // Receive Error
#define ISR_TOK         0x04    // Transmit OK
#define ISR_TER         0x08    // Transmit Error
#define ISR_RX_OVERFLOW 0x10    // RX FIFO Overflow
#define ISR_LINK_CHANGE 0x20    // Link Status Change
#define ISR_FIFO_ERROR  0x40    // FIFO overflow
#define ISR_SYS_ERROR   0x80    // System Error
#define ISR_TDU         0x100   // TX DMA Underrun
#define ISR_TIMEOUT     0x200   // Timeout
#define ISR_SWINT       0x400   // Software interrupt

// TX/RX Descriptor Status
#define DESC_OWN        0x80000000
#define DESC_EOR        0x40000000
#define DESC_FS         0x20000000
#define DESC_LS         0x10000000

// RX Config bits
#define RX_CONFIG_AB    0x00000002    // Accept Broadcast
#define RX_CONFIG_AM    0x00000004    // Accept Multicast
#define RX_CONFIG_APM   0x00000008    // Accept Physical Match
#define RX_CONFIG_AAP   0x00000010    // Accept All Packets

#define NUM_RX_DESC     32
#define NUM_TX_DESC     32
#define RX_BUFFER_SIZE  2048

static struct rtl8168_private {
    uint8_t bus;
    uint8_t device;
    uint8_t irq;
    uint32_t io_base;
    uint32_t mmio_base;
    
    // RX descriptors and buffers
    struct rx_desc rx_ring[NUM_RX_DESC] __attribute__((aligned(256)));
    uint8_t *rx_buffers[NUM_RX_DESC];
    volatile int rx_cur;
    
    // TX descriptors and buffers
    struct tx_desc tx_ring[NUM_TX_DESC] __attribute__((aligned(256)));
    uint8_t *tx_buffers[NUM_TX_DESC];
    volatile int tx_cur;
    volatile int tx_next_to_send;
    
    uint8_t mac_addr[6];
    int link_up;
    int initialized;
} rtl8168_dev;

static inline uint8_t rtl8168_read_byte(uint32_t addr) {
    return inb(rtl8168_dev.io_base + addr);
}

static inline uint16_t rtl8168_read_word(uint32_t addr) {
    return inw(rtl8168_dev.io_base + addr);
}

static inline uint32_t rtl8168_read_dword(uint32_t addr) {
    return inl(rtl8168_dev.io_base + addr);
}

static inline void rtl8168_write_byte(uint32_t addr, uint8_t value) {
    outb(rtl8168_dev.io_base + addr, value);
}

static inline void rtl8168_write_word(uint32_t addr, uint16_t value) {
    outw(rtl8168_dev.io_base + addr, value);
}

static inline void rtl8168_write_dword(uint32_t addr, uint32_t value) {
    outl(rtl8168_dev.io_base + addr, value);
}

static inline uint32_t rtl8168_read_phy(uint8_t reg) {
    rtl8168_write_word(RTL8168_PHY_ACCESS, (reg << 16) | 1);
    while(rtl8168_read_word(RTL8168_PHY_ACCESS) & 1) {
        // Wait for completion
    }
    return rtl8168_read_word(RTL8168_PHY_STATUS);
}

static inline void rtl8168_write_phy(uint8_t reg, uint16_t value) {
    rtl8168_write_word(RTL8168_PHY_STATUS, value);
    rtl8168_write_word(RTL8168_PHY_ACCESS, (reg << 16) | 2 | 1);
    while(rtl8168_read_word(RTL8168_PHY_ACCESS) & 1) {
        // Wait for completion
    }
}

static void rtl8168_reset(void) {
    rtl8168_write_byte(RTL8168_COMMAND, CMD_RST);
    
    // Wait for reset to complete
    int timeout = 100000;
    while(timeout-- && (rtl8168_read_byte(RTL8168_COMMAND) & CMD_RST)) {
        asm volatile("pause");
    }
    
    if(timeout <= 0) {
        printf("RTL8168: Reset failed!\n");
        return;
    }
    
    // Small delay after reset
    for(int i = 0; i < 10000; i++) {
        asm volatile("nop");
    }
}

static void rtl8168_init_rx_descriptors(void) {
    for(int i = 0; i < NUM_RX_DESC; i++) {
        rtl8168_dev.rx_buffers[i] = (uint8_t*)get_free_page();
        if(!rtl8168_dev.rx_buffers[i]) {
            printf("RTL8168: Failed to allocate RX buffer %d\n", i);
            return;
        }
        
        rtl8168_dev.rx_ring[i].addr_lo = (uint32_t)(uintptr_t)rtl8168_dev.rx_buffers[i];
        rtl8168_dev.rx_ring[i].addr_hi = 0; // 32-bit addressing
        rtl8168_dev.rx_ring[i].size = RX_BUFFER_SIZE - 1;
        rtl8168_dev.rx_ring[i].status = DESC_OWN;
        
        if(i == NUM_RX_DESC - 1) {
            rtl8168_dev.rx_ring[i].status |= DESC_EOR;
        }
    }
    
    // Set RX ring base address
    rtl8168_write_dword(RTL8168_RX_DESC_START_LO, (uint32_t)(uintptr_t)rtl8168_dev.rx_ring);
    rtl8168_write_dword(RTL8168_RX_DESC_START_HI, 0);
    
    rtl8168_dev.rx void rtl8168_init_tx_descriptors(void) {
    for(int i = 0; i < NUM_TX_DESC; i++) {
        rtl8168_dev.tx_buffers[i] = (uint8_t*)get_free_page();
        if(!rtl8168_dev.tx_buffers[i]) {
            printf("RTL8168: Failed to allocate TX buffer %d\n", i);
            return;
        }
        
        rtl8168_dev.tx_ring[i].addr_lo = (uint32_t)(uintptr_t)rtl8168_dev.tx_buffers[i];
        rtl8168_dev.tx_ring[i].addr_hi = 0; // 32-bit addressing
        rtl8168_dev.tx_ring[i].size = 0;
        rtl8168_dev.tx_ring[i].status = 0;
        
        if(i == NUM_TX_DESC - 1) {
            rtl8168_dev.tx_ring[i].status |= DESC_EOR;
        }
    }
    
    // Set TX ring base address
    rtl8168_write_dword(RTL8168_TX_DESC_START_LO, (uint32_t)(uintptr_t)rtl8168_dev.tx_ring);
    rtl8168_write_dword(RTL8168_TX_DESC_START_HI, 0);
    
    rtl8168_dev.tx_cur = 0;
    rtl8168_dev.tx_next_to_send = 0;
}

static void rtl8168_init_mac_address(void) {
    // Read MAC address from registers
    uint32_t mac_low = rtl8168_read_dword(RTL8168_MAC_ADDR);
    uint32_t mac_high = rtl8168_read_dword(RTL8168_MAC_ADDR + 4);
    
    rtl8168_dev.mac_addr[0] = mac_low & 0xFF;
    rtl8168_dev.mac_addr[1] = (mac_low >> 8) & 0xFF;
    rtl8168_dev.mac_addr[2] = (mac_low >> 16) & 0xFF;
    rtl8168_dev.mac_addr[3] = (mac_low >> 24) & 0xFF;
    rtl8168_dev.mac_addr[4] = mac_high & 0xFF;
    rtl8168_dev.mac_addr[5] = (mac_high >> 8) & 0xFF;
    
    printf("RTL8168: MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
           rtl8168_dev.mac_addr[0], rtl8168_dev.mac_addr[1],
           rtl8168_dev.mac_addr[2], rtl8168_dev.mac_addr[3],
           rtl8168_dev.mac_addr[4], rtl8168_dev.mac_addr[5]);
}

static void rtl8168_enable_interrupts(void) {
    // Enable all relevant interrupts
    rtl8168_write_word(RTL8168_INTERRUPT_MASK, 0xFFFF);
}

static void rtl8168_configure(void) {
    // Disable interrupts during configuration
    rtl8168_write_word(RTL8168_INTERRUPT_MASK, 0x0000);
    
    // Configure RX settings
    rtl8168_write_dword(RTL8168_RX_CONFIG, 
                        RX_CONFIG_AB | RX_CONFIG_AM | RX_CONFIG_APM | RX_CONFIG_AAP |
                        (7 << 8)); // 256 byte burst size
    
    // Configure TX settings
    rtl8168_write_dword(RTL8168_TX_CONFIG, 0x03000700); // Normal priority
    
    // Set maximum receive size
    rtl8168_write_word(RTL8168_RMS, RX_BUFFER_SIZE - 1);
    
    // Enable C+ command features
    rtl8168_write_word(RTL8168_CPLUS_CMD, 0x0000); // Disable C+ mode initially
    
    // Enable multicast
    rtl8168_write_dword(RTL8168_MAR0, 0xFFFFFFFF);
    rtl8168_write_dword(RTL8168_MAR4, 0xFFFFFFFF);
    
    // Additional configuration
    uint8_t cfg1 = rtl8168_read_byte(RTL8168_CONFIG_1);
    cfg1 |= 0x0A; // LED config
    rtl8168_write_byte(RTL8168_CONFIG_1, cfg1);
    
    // Clear interrupt status
    rtl8168_write_word(RTL8168_INTERRUPT_STATUS, 0xFFFF);
    
    // Re-enable interrupts
    rtl8168_enable_interrupts();
}

int rtl8168_init(uint8_t bus, uint8_t device) {
    printf("RTL8168: Initializing device at %d:%d\n", bus, device);
    
    // Read PCI configuration
    uint32_t bar0 = pci_read_reg(bus, device, 0, 0x10); // BAR0
    uint32_t bar1 = pci_read_reg(bus, device, 0, 0x14); // BAR1
    
    // Use I/O space if available, otherwise memory mapped
    if(bar0 & 1) {
        rtl8168_dev.io_base = bar0 & 0xFFFFFFFC;
        printf("RTL8168: Using I/O space at 0x%X\n", rtl8168_dev.io_base);
    } else {
        rtl8168_dev.mmio_base = bar0 & 0xFFFFFFF0;
        printf("RTL8168: Using MMIO at 0x%X\n", rtl8168_dev.mmio_base);
        // For MMIO, need to convert to physical address if required
        rtl8168_dev.io_base = (bar0 & 0xFFFFFFF0);
    }
    
    // Read IRQ
    rtl8168_dev.irq = pci_read_reg(bus, device, 0, 0x3C) & 0xFF;
    printf("RTL8168: IRQ: %d\n", rtl8168_dev.irq);
    
    // Store device info
    rtl8168_dev.bus = bus;
    rtl8168_dev.device = device;
    rtl8168_dev.initialized = 0;
    
    // Reset the device
    rtl8168_reset();
    
    // Initialize descriptors
    rtl8168_init_rx_descriptors();
    rtl8168_init_tx_descriptors();
    
    // Configure MAC address
    rtl8168_init_mac_address();
    
    // Configure the device
    rtl8168_configure();
    
    // Enable RX and TX
    uint8_t cmd = rtl8168_read_byte(RTL8168_COMMAND);
    cmd |= CMD_RX_ENABLE | CMD_TX_ENABLE | CMD_RX_FIFO | CMD_TX_FIFO;
    rtl8168_write_byte(RTL8168_COMMAND, cmd);
    
    // Check link status
    uint32_t phy_status = rtl8168_read_phy(0x01); // Basic status register
    rtl81_up = (phy_status & 0x04) ? 1 : 0;
    printf("RTL8168: Link status: %s\n", rtl8168_dev.link_up ? "UP" : "DOWN");
    
    rtl8168_dev.initialized = 1;
    printf("RTL8168: Initialization complete\n");
    return 0;
}

void rtl8168_transmit(void* packet, uint32_t length) {
    if(!rtl8168_dev.initialized) {
        printf("RTL8168: Not initialized\n");
        return;
    }
    
    if(length > RX_BUFFER_SIZE) {
        printf("RTL8168: Packet too large: %d\n", length);
        return;
    }
    
    int cur = rtl8168_dev.tx_cur;
    
    // Wait if descriptor is still owned by hardware
    int timeout = 10000;
    while((rtl8168_dev.tx_ring[cur].status & DESC_OWN) && timeout--) {
        if(timeout <= 0) {
            printf("RTL8168: TX timeout waiting for descriptor\n");
            return;
        }
    }
    
    // Copy packet to TX buffer
    memcpy(rtl8168_dev.tx_buffers[cur], packet, length);
    
    // Setup TX descriptor
    rtl8168_dev.tx_ring[cur].size = (length & 0x3FFF) | (DESC_FS | DESC_LS);
    rtl8168_dev.tx_ring[cur].status = DESC_OWN;
    
    // Increment counter
    rtl8168_dev.tx_cur = (cur + 1) % NUM_TX_DESC;
    
    // Tell controller to check for new descriptors
    rtl8168_write_byte(RTL8168_TX_CONFIG, rtl8168_read_byte(RTL8168_TX_CONFIG) | 0x40);
}

uint32_t rtl8168_poll(void* buffer) {
    if(!rtl8168_dev.initialized) {
        return 0;
    }
    
    uint32_t packets_received = 0;
    
    while(1) {
        int cur = rtl8168_dev.rx_cur;
        uint32_t status = rtl8168_dev.rx_ring[cur].status;
        
        if(status & DESC_OWN) {
            // Hardware still owns this descriptor
            break;
        }
        
        if(status & DESC_FS && status & DESC_LS) {
            // Valid packet (first and last segment)
            uint32_t packet_len = (status >> 16) & 0x3FFF;
            if(packet_len > 0 && packet_len <= RX_BUFFER_SIZE) {
                memcpy(buffer, rtl8168_dev.rx_buffers[cur], packet_len);
                
                // Reset descriptor for reuse
                rtl8168_dev.rx_ring[cur].status = DESC_OWN;
                packets_received = packet_len;
                
                // Move to next descriptor
                rtl8168_dev.rx_cur = (cur + 1) % NUM_RX_DESC;
                
                // Break after receiving one packet
                break;
            }
        }
        
        // Reset descriptor and move to next
        rtl8168_dev.rx_ring[cur].status = DESC_OWN;
        rtl8168_dev.rx_cur = (cur + 1) % NUM_RX_DESC;
    }
    
    return packets_received;
}

void rtl8168_interrupt_handler(void) {
    if(!rtl8168_dev.initialized) {
        return;
    }
    
    uint16_t status = rtl8168_read_word(RTL8168_INTERRUPT_STATUS);
    
    if(status == 0xFFFF || status == 0x0000) {
        // Device not present or other error
        return;
    }
    
    // Acknowledge interrupts
    rtl8168_write_word(RTL8168_INTERRUPT_STATUS, status);
    
    if(status & ISR_ROK) {
        // Received packet(s) - handled in polling
    }
    
    if(status & ISR_TOK) {
        // Transmission completed
    }
    
    if(status & ISR_LINK_CHANGE) {
        // Check link status via PHY
        uint32_t phy_status = rtl8168_read_phy(0x01); // PHY basic status
        rtl8168_dev.link_up = (phy_status & 0x04) ? 1 : 0;
        printf("RTL8168: Link status changed, up=%d\n", rtl8168_dev.link_up);
    }
    
    if(status & (ISR_RER | ISR_TER | ISR_RX_OVERFLOW | ISR_FIFO_ERROR | ISR_SYS_ERROR | ISR_TDU)) {
        printf("RTL8168: Error interrupt 0x%X\n", status);
        // Could implement recovery here
    }
    
    if(status & ISR_TIMEOUT) {
        // Timer interrupt - clear it
    }
}

// Export functions for network system calls
int rtl8168_send_packet(void* packet, uint32_t length) {
    if(!rtl8168_dev.initialized) {
        return -1;
    }
    
    rtl8168_transmit(packet, length);
    return length;
}

int rtl8168_receive_packet(void* buffer, uint32_t max_length) {
    if(!rtl8168_dev.initialized) {
        return -1;
    }
    
    uint32_t received = rtl8168_poll(buffer);
    if(received > max_length) {
        return -1; // Buffer too small
    }
    return received;
}

int rtl8168_get_mac_address(uint8_t* mac) {
    if(!rtl8168_dev.initialized || !mac) {
        return -1;
    }
    
    memcpy(mac, rtl8168_dev.mac_addr, 6);
    return 0;
}

int rtl8168_is_link_up(void) {
    if(!rtl8168_dev.initialized) {
        return 0;
    }
    
    return rtl8168_dev.link_up;
}

// Function to detect and initialize RTL8168 device
void find_and_init_rtl8168(void) {
    // Search through PCI devices for RTL8168 NIC
    for(uint8_t bus = 0; bus < 256; bus++) {
        for(uint8_t device = 0; device < 32; device++) {
            uint32_t vendor_device = pci_read_reg(bus, device, 0, 0x00);
            uint16_t vendor_id = vendor_device & 0xFFFF;
            uint16_t device_id = vendor_device >> 16;
            
            // Check if it's a Realtek RTL8168 device
            if(vendor_id == RTL8168_VENDOR_ID && 
               (device_id == RTL8168_DEVICE_ID_1 || device_id == RTL8168_DEVICE_ID_2 ||
                device_id == RTL8168_DEVICE_ID_3 || device_id == RTL8168_DEVICE_ID_4 ||
                device_id == RTL8168_DEVICE_ID_5)) {
                
                printf("Found RTL8168/8111 NIC at %d:%d, Device ID: 0x%04X\n", bus, device, device_id);
                
                int result = rtl8168_init(bus, device);
                if(result == 0) {
                    printf("RTL8168: Successfully initialized\n");
                    return;
                } else {
                    printf("RTL8168: Failed to initialize\n");
                }
            }
        }
    }
    printf("RTL8168: No compatible device found\n");
}

// Additional helper functions
void rtl8168_set_multicast_filter(uint32_t hash[2]) {
    if(!rtl8168_dev.initialized) {
        return;
    }
    
    // Set multicast hash filter
    rtl8168_write_dword(RTL8168_MAR0, hash[0]);
    rtl8168_write_dword(RTL8168_MAR4, hash[1]);
}

void rtl8168_set_promiscuous(int enable) {
    if(!rtl8168_dev.initialized) {
        return;
    }
    
    uint32_t rx_config = rtl8168_read_dword(RTL8168_RX_CONFIG);
    if(enable) {
        rx_config |= RX_CONFIG_AAP; // Accept all packets
    } else {
        rx_config &= ~RX_CONFIG_AAP; // Don't accept all packets
    }
    rtl8168_write_dword(RTL8168_RX_CONFIG, rx_config);
}

void rtl8168_power_down(void) {
    if(!rtl8168_dev.initialized) {
        return;
    }
    
    // Disable RX/TX
    uint8_t cmd = rtl8168_read_byte(RTL8168_COMMAND);
    cmd &= ~(CMD_RX_ENABLE | CMD_TX_ENABLE);
    rtl8168_write_byte(RTL8168_COMMAND, cmd);
    
    // Power down PHY
    uint16_t phy_data = rtl8168_read_phy(0x00); // Control register
    phy_data |= 0x0800; // Set power-down bit
    rtl8168_write_phy(0x00, phy_data);
}

void rtl8168_power_up(void) {
    if(!rtl8168_dev.initialized) {
        return;
    }
    
    // Power up PHY
    uint16_t phy_data = rtl8168_read_phy(0x00); // Control register
    phy_data &= ~0x0800; // Clear power-down bit
    rtl8168_write_phy(0x00, phy_data);
    
    // Small delay for PHY to wake up
    for(int i = 0; i < 10000; i++) {
        asm volatile("nop");
    }
    
    // Re-enable RX/TX
    uint8_t cmd = rtl8168_read_byte(RTL8168_COMMAND);
    cmd |= (CMD_RX_ENABLE | CMD_TX_ENABLE);
    rtl8168_write_byte(RTL8168_COMMAND, cmd);
}
