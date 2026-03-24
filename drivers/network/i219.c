#include "i219.h"
#include "mm.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "lib/io.h"
#include "drivers/pci.h"

#define I219_VENDOR_ID 0x8086
#define I219_DEVICE_ID_V 0x15B8
#define I219_DEVICE_ID_LM 0x15B7

#define I219_CTRL      0x0000
#define I219_STATUS    0x0008
#define I219_EERD      0x0014
#define I219_RCTL      0x0100
#define I2      0x0400
#define I219_RDBAL     0x2800
#define I219_RDBAH     0x2#define I219_RDLEN     0x2808
#define I219_RDH       0x2810
#define I219_RDT       0x2818
#define I219_TDBAL     0x3800
#define I219_TDBAH     0x3804
#define I219_TDLEN     0x3808
#define I219_TDH       0x3810
#define I219_TDT       0x3818
#define I219_RXDCTL    0x2828
#define I219_TXDCTL    0x3828
#define I219_IMASK     0x00D8
#define I219_ICR       0x00C0
#define I219_RXCSUM    0x0500
#define I219_MTA       0x5200
#define I219_RA        0x5400
#define I219_MDIC      0x0020

#define R1 << 1)
#define RCTL_SBP    (1 << 2)
#define RCTL_UPE    (1 << 3)
#define RCTL_MPE    (1 << 4)
#define RCTL_LPE    (1 << 5)
#define RCTL_BAM    (1 << 15)
#define RCTL_SECRC  (1 << 26)

#define TCTL_EN     (1 << 1)
#define TCTL_PSP    (1 << 3)
#define TCTL_CT     (0xF << 4)
#define TCTL_COLD   (0x3F << 12)

#define CMD_EOP     (1 << 0)
#define CMD_RS      (1 << 3)
#define CMD_RPS     (1 << 4)

#define STATUS_FD   (1 << 0)
#define STATUS_LU   (1 << 1)
#define STATUS_SPEED_100 (1 << 6)
#define STATUS_SPEED_1000 (1 << 8)

#define RX_DESC_COUNT 32
#define TX_DESC_COUNT 32
#define PACKET_SIZE 1518

struct i219_rx_desc {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed));

struct i219_tx_desc {
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed));

static volatile uint8_t *i219_mmio_base = NULL;
static struct i219_rx_desc *rx_ring = NULL;
static struct i219_tx_desc *tx_ring = NULL;
static uint8_t rx_buffer[RX_DESC_COUNT][PACKET_SIZE] __attribute__((aligned(16)));
static uint8_t tx_buffer[TX_DESC_COUNT][PACKET_SIZE] __attribute__((aligned(16)));
static volatile uint32_t rx_index = 0;
static volatile uint32_t tx_index = 0;
static uint8_t mac_address[6] = {0};

static uint32_t i219_read_reg(uint32_t reg) {
    return *(volatile uint32_t*)(i219_mmio_base + reg);
}

static void i219_write_reg(uint32_t reg, uint32_t value) {
    *(volatile uint32_t*)(i219_mmio_base + reg) = value;
}

static uint32_t i219_read_eeprom_word(uint8_t word) {
    uint32_t eerd = (1 << 31) | (word << 2);
    i219_write_reg(I219_EERD, eerd);
    
    while (!(i219_read_reg(I219_EERD) & (1 << 31)));
    
    return (i219_read_reg(I219_EERD) >> 16) & 0xFFFF;
}

static void i219_read_mac_address() {
    uint32_t low = i219_read_eeprom_word(0);
    uint32_t high = i219_read_eeprom_word(1);
    
    mac_address[0] = low & 0xFF;
    mac_address[1] = (low >> 8) & 0xFF;
    mac_address[2] = (high >> 8) & 0xFF;
    mac_address[3] = (high >> 16) & 0xFF;
    mac_address[4] = (high >> 24) & 0xFF;
    mac_address[5] = 0x00;
}

static void i219_init_rx_descriptors() {
    rx_ring = (struct i219_rx_desc*)get_free_page();
    for (int i = 0; i < RX_DESC_COUNT; i++) {
        rx_ring[i].addr = (uint64_t)rx_buffer[i];
        rx_ring[i].status = 0;
        rx_ring[i].errors = 0;
    }
    
    i219_write_reg(I219_RDBAL, (uint32_t)((uint64_t)rx_ring & 0xFFFFFFFF));
    i219_write_reg(I219_RDBAH, (uint32_t)((uint64_t)rx_ring >> 32));
    i219_write_reg(I219_RDLEN, RX_DESC_COUNT * sizeof(struct i219_rx_desc));
    i219_write_reg(I219_RDH, 0);
    i219_write_reg(I219_RDT, RX_DESC_COUNT - 1);
    
    uint32_t rxdctl = i219_read_reg(I219_RXDCTL);
    rxdctl |= 1; // Enable
    rxdctl &= ~0x3F0; // Clear head writeback frequency
    i219_write_reg(I219_RXDCTL, rxdctl);
}

static void i219_init_tx_descriptors() {
    tx_ring = (struct i219_tx_desc*)get_free_page();
    for (int i = 0; i < TX_DESC_COUNT; i++) {
        tx_ring[i].status = 1;
    }
    
    i219_write_reg(I219_TDBAL, (uint32_t)((uint64_t)tx_ring & 0xFFFFFFFF));
    i219_write_reg(I219_TDBAH, (uint32_t)((uint64_t)tx_ring >> 32));
    i219_write_reg(I219_TDLEN, TX_DESC_COUNT * sizeof(struct i219_tx_desc));
    i219_write_reg(I219_TDH, 0);
    i219_write_reg(I219_TDT, 0);
    
    uint32_t txdctl = i219_read_reg(I219_TXDCTL);
    txdctl |= 1; // Enable
    i219_write_reg(I219_TXDCTL, txdctl);
}

static void i219_setup_mac_address() {
    uint32_t low = (mac_address[3] << 24) | (mac_address[2] << 16) | 
                   (mac_address[1] << 8) | mac_address[0];
    uint32_t high = (mac_address[5] << 8) | mac_address[4];
    
    i219_write_reg(I219_RA, low);
    i219_write_reg(I219_RA + 4, high | 0x80000000); // Enable
}

int i219_init(uint8_t bus, uint8_t device) {
    uint32_t vendor_device = pci_read_reg(bus, device, 0, 0x00);
    uint16_t vendor_id = vendor_device & 0xFFFF;
    uint16_t device_id = vendor_device >> 16;
    
    if (vendor_id != I219_VENDOR_ID || 
        (device_id != I219_DEVICE_ID_V && device_id != I219_DEVICE_ID_LM)) {
        return -1;
    }
    
    printf("Initializing Intel I219 NIC at %d:%d\n", bus, device);
    
    uint32_t bar = pci_read_reg(bus, device, 0, 0x10);
    i219_mmio_base = (volatile uint8_t*)(bar & 0xFFFFFFF0);
    
    // Software reset
    i219_write_reg(I219_CTRL, 0x04000000);
    for (volatile int i = 0; i < 1000; i++);
    
    i219_read_mac_address();
    
    i219_init_rx_descriptors();
    i219_init_tx_descriptors();
    
    i219_setup_mac_address();
    
    // Enable transmitter/receiver
    uint32_t tctl = TCTL_EN | TCTL_PSP | TCTL_CT | TCTL_COLD;
    i219_write_reg(I219_TCTL, tctl);
    
    uint32_t rctl = RCTL_EN | RCTL_SBP | RCTL_UPE | RCTL_MPE | 
                    RCTL_LPE | RCTL_BAM | RCTL_SECRC;
    i219_write_reg(I219_RCTL, rctl);
    
    // Enable interrupts
    i219_write_reg(I219_IMASK, 0x1F6DC);
    i219_write_reg(I219_ICR, 0xFFFFFFFF);
    
    printf("Intel I219 initialized successfully\n");
    return 0;
}

int i219_transmit(void* packet, uint32_t length) {
    if (length > PACKET_SIZE) return -1;
    
    uint32_t next_tx_index = (tx_index + 1) % TX_DESC_COUNT;
    if (!(tx_ring[tx_index].status & 0xFF)) {
        memcpy(tx_buffer[tx_index], packet, length);
        
        tx_ring[tx_index].addr = (uint64_t)tx_buffer[tx_index];
        tx_ring[tx_index].length = length;
        tx_ring[tx_index].cso = 0;
        tx_ring[tx_index].cmd = CMD_EOP | CMD_RS;
        tx_ring[tx_index].status = 0;
        tx_ring[tx_index].css = 0;
        tx_ring[tx_index].special = 0;
        
        i219_write_reg(I219_TDT, next_tx_index);
        tx_index = next_tx_index;
        
        return length;
    }
    
    return -1;
}

uint32_t i219_poll(void* buffer) {
    struct i219_rx_desc* desc = &rx_ring[rx_index];
    if (desc->status & 1) {
        uint32_t length = desc->length;
        if (length > 0 && length <= PACKET_SIZE) {
            memcpy(buffer, rx_buffer[rx_index], length);
            
            desc->status = 0;
            rx_index = (rx_index + 1) % RX_DESC_COUNT;
            i219_write_reg(I219_RDT, rx_index);
            
            return length;
        }
    }
    
    return 0;
}

void i219_interrupt_handler() {
    uint32_t icr = i219_read_reg(I219_ICR);
    if (icr & 0x02) {
        while (1) {
            uint8_t temp_buffer[PACKET_SIZE];
            uint32_t len = i219_poll(temp_buffer);
            if (len == 0) break;
        }
    }
}
