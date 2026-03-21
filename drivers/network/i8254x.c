// drivers/network/i8254x.c
#include "drivers/network/i8254x.h"

// Global variables
uint32_t os_NetIOBaseMem = 0;
uint8_t os_NetIRQ = 0;
uint8_t os_NetMAC[6] = {0};
uint8_t os_eth_rx_buffer[8192] __attribute__((aligned(16)));
uint8_t os_eth_tx_buffer[8192] __attribute__((aligned(16)));

// Array of descriptors
struct i8254x_rx_desc rx_descriptors[32] __attribute__((aligned(16)));
struct i8254x_tx_desc tx_descriptors[32] __attribute__((aligned(16)));

// Packet buffers
uint8_t rx_packet_buffers[32][2048] __attribute__((aligned(16)));
uint8_t tx_packet_buffers[32][2048] __attribute__((aligned(16)));

static uint32_t rx_index = 0;
static uint32_t tx_index = 0;

// Helper functions
static inline void write_reg32(uint32_t addr, uint32_t value) {
    *(volatile uint32_t*)(os_NetIOBaseMem + addr) = value;
}

static inline uint32_t read_reg32(uint32_t addr) {
    return *(volatile uint32_t*)(os_NetIOBaseMem + addr);
}

static inline void write_reg16(uint32_t addr, uint16_t value) {
    *(volatile uint16_t*)(os_NetIOBaseMem + addr) = value;
}

static inline uint16_t read_reg16(uint32_t addr) {
    return *(volatile uint16_t*)(os_NetIOBaseMem + addr);
}

static inline void write_reg8(uint32_t addr, uint8_t value) {
    *(volatile uint8_t*)(os_NetIOBaseMem + addr) = value;
}

static inline uint8_t read_reg8(uint32_t addr) {
    return *(volatile uint8_t*)(os_NetIOBaseMem + addr);
}

void i8254x_init(uint8_t bus, uint8_t slot) {
    // Find device via PCI
    uint8_t function = 0;
    uint32_t pci_addr = (bus << 16) | (slot << 11) | (function << 8);
    
    // Read BAR0 for memory-mapped I/O base address
    uint32_t bar0 = pci_read_reg(bus, slot, function, 0x10); // BAR0 register
    os_NetIOBaseMem = bar0 & 0xFFFFFFF0; // Clear lower 4 bits
    
    // Read IRQ
    os_NetIRQ = pci_read_reg(bus, slot, function, 0x3C) & 0xFF;
    
    // Enable PCI Bus Mastering
    uint32_t command_status = pci_read_reg(bus, slot, function, 0x04);
    command_status |= 0x00000004; // Set Bus Master bit
    pci_write_reg(bus, slot, function, 0x04, command_status);
    
    // Read MAC address from RAL/RAH registers
    uint32_t mac_lo = read_reg32(0x5400); // RAL
    uint32_t mac_hi = read_reg32(0x5404); // RAH
    
    if(mac_lo != 0 || (mac_hi & 0xFFFF) != 0) {
        os_NetMAC[0] = mac_lo & 0xFF;
        os_NetMAC[1] = (mac_lo >> 8) & 0xFF;
        os_NetMAC[2] = (mac_lo >> 16) & 0xFF;
        os_NetMAC[3] = (mac_lo >> 24) & 0xFF;
        os_NetMAC[4] = mac_hi & 0xFF;
        os_NetMAC[5] = (mac_hi >> 8) & 0xFF;
    } else {
        // Try reading from EEPROM if RAL/RAH not valid
        // This is a simplified approach - in reality you'd need to implement EEPROM reading
        os_NetMAC[0] = 0x00;
        os_NetMAC[1] = 0x11;
        os_NetMAC[2] = 0x22;
        os_NetMAC[3] = 0x33;
        os_NetMAC[4] = 0x44;
        os_NetMAC[5] = 0x55;
    }
    
    // Reset the device
    i8254x_reset();
    
    printf("Intel 8254x NIC initialized\n");
    printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", 
           os_NetMAC[0], os_NetMAC[1], os_NetMAC[2], 
           os_NetMAC[3], os_NetMAC[4], os_NetMAC[5]);
    printf("Memory base: 0x%x\n", os_NetIOBaseMem);
    printf("IRQ: %d\n", os_NetIRQ);
}

void i8254x_reset(void) {
    // Stop transmitter and receiver
    uint32_t ctrl = read_reg32(I8254X_REG_CTRL);
    ctrl &= ~I8254X_TCTL_EN;
    write_reg32(I8254X_REG_TCTL, ctrl);
    
    ctrl &= ~I8254X_RCTL_EN;
    write_reg32(I8254X_REG_RCTL, ctrl);
    
    // Disable all interrupts
    write_reg32(I8254X_REG_IMC, 0xFFFFFFFF);
    
    // Clear any pending interrupts
    read_reg32(I8254X_REG_ICR);
    
    // Disable interrupt throttling
    write_reg32(I8254X_REG_ITR, 0);
    
    // Configure PBA (Packet Buffer Allocation)
    write_reg32(I8254X_REG_PBA, 0x0030); // 48KB for RX, 16KB for TX
    
    // Configure TXCW (Transmit Configuration Word)
    write_reg32(I8254X_REG_TXCW, 0x80008060);
    
    // Reset control register settings
    ctrl = read_reg32(I8254X_REG_CTRL);
    ctrl &= ~(I8254X_CTRL_LRST | I8254X_CTRL_RST | I8254X_CTRL_VME | I8254X_CTRL_ILOS);
    ctrl |= I8254X_CTRL_SLU | I8254X_CTRL_ASDE;
    write_reg32(I8254X_REG_CTRL, ctrl);
    
    // Initialize receive descriptors
    for(int i = 0; i < 32; i++) {
        rx_descriptors[i].addr = (uint64_t)rx_packet_buffers[i];
        rx_descriptors[i].status = 0;
        rx_descriptors[i].errors = 0;
        rx_descriptors[i].length = 0;
        rx_descriptors[i].checksum = 0;
        rx_descriptors[i].special = 0;
    }
    
    // Setup RX ring
    write_reg32(I8254X_REG_RDBAL, (uint32_t)(uint64_t)rx_descriptors);
    write_reg32(I8254X_REG_RDBAH, (uint32_t)((uint64_t)rx_descriptors >> 32));
    write_reg32(I8254X_REG_RDLEN, 32 * sizeof(struct i8254x_rx_desc));
    write_reg32(I8254X 0);
    write_reg32(I8254X_REG_RDT, 31); // Tail points to last descriptor initially
    
    // Initialize transmit descriptors
    for(int i = 0; i < 32; i++) {
        tx_descriptors[i].addr = (uint64_t)tx_packet_buffers[i];
        tx_descriptors[i].status = I8254X_TXDESC_DD; // Mark as done initially
        tx_descriptors[i].cmd = 0;
        tx_descriptors[i].length = 0;
        tx_descriptors[i].cso = 0;
        tx_descriptors[i].css = 0;
        tx_descriptors[i].special = 0;
    }
    
    // Setup TX ring
    write_reg32(I8254X_REG_TDBAL, (uint32_t)(uint64_t)tx_descriptors);
    write_reg32(I8254X_REG_TDBAH, (uint32_t)((uint64_t)tx_descriptors >> 32));
    write_reg32(I8254X_REG_TDLEN, 32 * sizeof(struct i8254x_tx_desc));
    write_reg32(I8254X_REG_TDH, 0);
    write_reg32(I8254X_REG_TDT, 0);
    
    // Configure transmit control
    write_reg32(I8254X_REG_TCTL, I8254X_TCTL_EN | I8254X_TCTL_PSP);
    write_reg32(I8254X_REG_TIPG, 0x0060200A); // IPGT 10, IPGR1 8, IPGR2 6
    
    // Configure receive control
    write_reg32(I8254X_REG_RCTL, 
                I8254X_RCTL_EN | 
                I8254X_RCTL_SBP | 
                I8254X_RCTL_UPE | 
                I8254X_RCTL_MPE | 
                I8254X_RCTL_LPE | 
                I8254X_RCTL_BAM | 
                I8254X_RCTL_SECRC);
    
    // Clear delay timers
    write_reg32(I8254X_REG_RDTR, 0);
    write_reg32(I8254X_REG_RADV, 0);
    write_reg32(I8254X_REG_RSRPD, 0);
    
    // Enable interrupts
    write_reg32(I8254X_REG_IMS, I8254X_ICR_RXDMT0 | I8254X_ICR_RXT0 | I8254X_ICR_RXO);
    
    rx_index = 0;
    tx_index = 0;
}

void i8254x_transmit(void* packet, uint32_t length) {
    if(length > 2047) return; // Packet too large
    
    // Wait until current descriptor is available
    while(!(tx_descriptors[tx_index].status & I8254X_TXDESC_DD)) {
        // In a real system, you might want to yield here
    }
    
    // Copy packet to TX buffer
    memcpy((void*)tx_descriptors[tx_index].addr, packet, length);
    
    // Setup descriptor
    tx_descriptors[tx_index].length = length;
    tx_descriptors[tx_index].cmd = I8254X_TXDESC_EOP | I8254X_TXDESC_IFCS | I8254X_TXDESC_RS;
    tx_descriptors[tx_index].status = 0; // Clear done bit
    
    // Update tail pointer to trigger transmission
    uint32_t next_index = (tx_index + 1) % 32;
    write_reg32(I8254X_REG_TDT, next_index);
    
    // Move to next descriptor
    tx_index = next_index;
}

uint32_t i8254x_poll(void* packet) {
    uint32_t current_rdt = read_reg32(I8254X_REG_RDT);
    uint32_t next_index = (current_rdt + 1) % 32;
    
    // Check if a packet is available
    if(rx_descriptors[next_index].status & I8254X_RXDESC_DD) {
        uint32_t packet_len = rx_descriptors[next_index].length;
        
        if(packet_len > 0 && packet_len <= 2047) {
            // Copy packet to destination
            memcpy(packet, (void*)rx_descriptors[next_index].addr, packet_len);
            
            // Mark descriptor as available by clearing DD bit
            rx_descriptors[next_index].status = 0;
            rx_descriptors[next_index].errors = 0;
            rx_descriptors[next_index].length = 0;
            
            // Update head pointer
            write_reg32(I8254X_REG_RDH, current_rdt);
            write_reg32(I8254X_REG_RDT, next_index);
            
            return packet_len;
        }
    }
    
    return 0; // No packet available
}

uint32_t i8254x_ack_int(void) {
    uint32_t icr = read_reg32(I8254X_REG_ICR);
    return icr;
}

// Interrupt handler for the NIC
void i8254x_interrupt_handler(void) {
    uint32_t icr = i8254x_ack_int();
    
    if(icr & (I8254X_ICR_RXT0 | I8254X_ICR_RXDMT0)) {
        // Received packet interrupt
        // In a real system, you might want to signal higher-level networking code
    }
    
    if(icr & I8254X_ICR_TXDW) {
        // Transmission complete interrupt
        // In a real system, you might want to clean up transmitted packets
    }
    
    if(icr & I8254X_ICR_LSC) {
        // Link status change interrupt
        uint32_t status = read_reg32(I8254X_REG_STATUS);
        if(status & I8254X_STATUS_LU) {
            printf("Network link up\n");
        } else {
            printf("Network link down\n");
        }
    }
}

