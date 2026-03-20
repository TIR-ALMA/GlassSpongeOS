#include "disk.h"
#include "lib/string.h"
#include "lib/printf.h"

#define ATA_PRIMARY_IO 0x1F0
#define ATA_SECONDARY_IO 0x170

#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07
#define ATA_REG_SECCOUNT1  0x08
#define ATA_REG_LBA3       0x09
#define ATA_REG_LBA4       0x0A
#define ATA_REG_LBA5       0x0B
#define ATA_REG_CONTROL    0x0C
#define ATA_REG_ALTSTATUS  0x0C
#define ATA_REG_DEVADDRESS 0x0D

// Commands
#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xC8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xCA
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY_PACKET   0xA1
#define ATA_CMD_IDENTIFY          0xEC

// Status flags
#define ATA_SR_BSY     0x80
#define ATA_SR_DRDY    0x40
#define ATA_SR_DF      0x20
#define ATA_SR_DSC     0x10
#define ATA_SR_DRQ     0x08
#define ATA_SR_CORR    0x04
#define ATA_SR_IDX     0x02
#define ATA_SR_ERR     0x01

// Device flags
#define ATA_MASTER     0x00
#define ATA_SLAVE      0x10

static int ata_io_base = ATA_PRIMARY_IO;

// Helper functions
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %1, %0" : : "Nd"(port), "a"(val));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void insw(uint16_t port, void *addr, uint32_t count) {
    __asm__ volatile ("rep insw" : "+D"(addr), "+c"(count) : "d"(port) : "memory");
}

static inline void outsw(uint16_t port, const void *addr, uint32_t count) {
    __asm__ volatile ("rep outsw" : "+S"(addr), "+c"(count) : "d"(port) : "memory");
}

static uint8_t ata_status_wait() {
    while (inb(ata_io_base + ATA_REG_STATUS) & ATA_SR_BSY);
    return inb(ata_io_base + ATA_REG_STATUS);
}

static uint8_t ata_read_sector(uint32_t lba, uint8_t *buffer) {
    uint8_t status = ata_status_wait();
    if(status & ATA_SR_ERR) return 0;
    
    // Select device (master) and set LBA mode
    outb(ata_io_base + ATA_REG_HDDEVSEL, 0xE0 | (lba >> 24 & 0x0F));
    
    // Send parameters
    outb(ata_io_base + ATA_REG_FEATURES, 0x00);
    outb(ata_io_base + ATA_REG_SECCOUNT0, 1);
    outb(ata_io_base + ATA_REG_LBA0, (uint8_t)lba);
    outb(ata_io_base + ATA_REG_LBA1, (uint8_t)(lba >> 8));
    outb(ata_io_base + ATA_REG_LBA2, (uint8_t)(lba >> 16));
    
    // Send command
    outb(ata_io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    
    // Wait for DRQ
    while (!((status = inb(ata_io_base + ATA_REG_STATUS)) & ATA_SR_DRQ));
    if(status & ATA_SR_ERR) return 0;
    
    // Read 256 words (512 bytes)
    insw(ata_io_base + ATA_REG_DATA, buffer, 256);
    
    status = ata_status_wait();
    return !(status & ATA_SR_ERR);
}

static uint8_t ata_write_sector(uint32_t lba, const uint8_t *buffer) {
    uint8_t status = ata_status_wait();
    if(status & ATA_SR_ERR) return 0;
    
    // Select device (master) and set LBA mode
    outb(ata_io_base + ATA_REG_HDDEVSEL, 0xE0 | (lba >> 24 & 0x0F));
    
    // Send parameters
    outb(ata_io_base + ATA_REG_FEATURES, 0x00);
    outb(ata_io_base + ATA_REG_SECCOUNT0, 1);
    outb(ata_io_base + ATA_REG_LBA0, (uint8_t)lba);
    outb(ata_io_base + ATA_REG_LBA1, (uint8_t)(lba >> 8));
    outb(ata_io_base + ATA_REG_LBA2, (uint8_t)(lba >> 16));
    
    // Send command
    outb(ata_io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    
    // Wait for DRQ
    while (!((status = inb(ata_io_base + ATA_REG_STATUS)) & ATA_SR_DRQ));
    if(status & ATA_SR_ERR) return 0;
    
    // Write 256 words (512 bytes)
    outsw(ata_io_base + ATA_REG_DATA, buffer, 256);
    
    status = ata_status_wait();
    return !(status & ATA_SR_ERR);
}

void disk_init() {
    // Detect and initialize ATA disk
    uint8_t status = inb(ata_io_base + ATA_REG_STATUS);
    
    if(status == 0xFF) {
        // No device present
        printf("No ATA device detected\n");
        return;
    }
    
    // Try to identify the drive
    outb(ata_io_base + ATA_REG_HDDEVSEL, 0xA0); // Select master
    ata_status_wait();
    
    outb(ata_io_base + ATA_REG_SECCOUNT0, 0);
    outb(ata_io_base + ATA_REG_LBA0, 0);
    outb(ata_io_base + ATA_REG_LBA1, 0);
    outb(ata_io_base + ATA_REG_LBA2, 0);
    
    outb(ata_io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    
    status = inb(ata_io_base + ATA_REG_STATUS);
    if(status == 0) {
        printf("ATA device not present\n");
        return;
    }
    
    // Wait for BSY to clear and DRQ to be set
    while (inb(ata_io_base + ATA_REG_STATUS) & ATA_SR_BSY);
    status = inb(ata_io_base + ATA_REG_STATUS);
    
    if(status & ATA_SR_DRQ) {
        uint16_t identify_data[256];
        insw(ata_io_base + ATA_REG_DATA, identify_data, 256);
        
        // Extract disk information
        uint32_t sectors = identify_data[60] | ((uint32_t)identify_data[61] << 16);
        printf("ATA disk initialized, sectors: %d\n", sectors);
    } else {
        printf("Failed to identify ATA disk\n");
        return;
    }
    
    printf("Disk driver initialized\n");
}

// Public API functions
uint8_t disk_read_sector(uint32_t lba, uint8_t *buffer) {
    return ata_read_sector(lba, buffer);
}

uint8_t disk_write_sector(uint32_t lba, const uint8_t *buffer) {
    return ata_write_sector(lba, buffer);
}

