#include "types.h"
#include "mm.h"
#include "vfs.h"
#include "drivers/network/intel_ax2xx.h"
#include "lib/string.h"
#include "lib/printf.h"

static struct ax2xx_device ax2xx_devices[MAX_AX2XX_DEVICES];
static int ax2xx_device_count = 0;

// PCIe vendor/device IDs for Intel AX series
#define INTEL_VENDOR_ID 0x8086
#define AX200_DEVICE_ID 0x2723
#define AX201_DEVICE_ID 0x2725  
#define AX210_DEVICE_ID 0x2720
#define AX416_DEVICE_ID 0x51f0

// Register definitions
#define AX2XX_CSR_BASE 0x0000
#define AX2XX_FH_BASE  0x4000
#define AX2XX_HBUS_BASE 0x8000
#define AX2XX_UREG_BASE 0xA00000
#define AX2XX_LMAC_FH_BASE 0x400000

// CSR Registers
#define CSR_HW_IF_CONFIG_REG 0x000
#define CSR_INT_COALESCING 0x004
#define CSR_INT_PERIOD 0x008
#define CSR_NMI_START_TIMER 0x00c
#define CSR_RESET 0x020
#define CSR_GP_CNTRL 0x024
#define CSR_HW_REV 0x028
#define CSR_EEPROM_SELECT 0x02c
#define CSR_GIO_CHICKEN_DIE_CF_A 0x048
#define CSR_WFPM_CTRL 0x0a8
#define CSR_DBG_LINK_RECOVERY 0x0a4
#define CSR_FUNC_SCRATCH 0x1a0
#define CSR_CLKO_CTRL 0x1a4
#define CSR_RAND 0x1b0
#define CSR_EEPROM_REG 0x20c
#define CSR_MBOX_SET_REG 0x300
#define CSR_GP_UCODE_REG 0x344
#define CSR_GP_DRIVER_REG 0x348
#define CSR_LED_REG 0x34c
#define CSR_DRAM_INT_TBL_REG 0x3 CSR_MAC_SHADOW_REG_A0 0x360
#define CSR_MAC_SHADOW_REG_A1 0x364
#define CSR_MAC_SHADOW_REG_C0 0x370
#define CSR_MAC_SHADOW_REG_C1 0x374
#define CSR_MAC_SHADOW_REG_C2 0x378
#define CSR_MAC_SHADOW_REG_C3 0x37c
#define CSR_MAC_SHADOW_REG_C4 0x380
#define CSR_MAC_SHADOW_REG_C5 0x384
#define CSR_MAC_SHADOW_REG_C6 0x388
#define CSR_MAC_SHADOW_REG_C7 0x38c
#define CSR_MAC_SHADOW_REG_C8 0x390
#define CSR_MAC_SHADOW_REG_C9 0x394
#define CSR_MAC_SHADOW_REG_D0 0x3a0
#define CSR_MAC_SHADOW_REG_D1 0x3a4
#define CSR_MAC_SHADOW_REG_D2 0x3a8
#define CSR_MAC_SHADOW_REG_D3 0x3ac
#define CSR_PHY_FORMAT_CTRL_REG 0x800
#define CSR_TX_CHICKEN 0x804
#define CSR_RX_CHICKEN 0x80c
#define CSR_RFC_CMD 0x920
#define CSR_RFC_CFG 0x924
#define CSR_RFC_DC_CFG 0x928
#define CSR_RFC_CTL 0x92c
#define CSR_RFC_STS 0x930
#define CSR_RFC_CRC 0x934
#define CSR_RFC_TOP_DBG_ADDR 0x938
#define CSR_RFC_TOP_DBG_DATA 0x93c
#define CSR_RFC_RAM_DBG_ADDR 0x940
#define CSR_RFC_RAM_DBG_DATA 0x944
#define CSR_RFC_RAM_DBG_CTL 0x948
#define CSR_RFC_RAM_DBG_STS 0x94c
#define CSR_RFC_RAM2_CTL 0x950
#define CSR_RFC_RAM2_WADDR 0x954
#define CSR_RFC_RAM2_RADDR 0x958
#define CSR_RFC_RAM2_DATA 0x95c
#define CSR_RFC_RAM3_CTL 0x960
#define CSR_RFC_RAM3_WADDR 0x964
#define CSR_RFC_RAM3_RADDR 0x968
#define CSR_RFC_RAM3_DATA 0x96c
#define CSR_RFC_RAM4_CTL 0x970
#define CSR_RFC_RAM4_WADDR 0x974
#define CSR_RFC_RAM4_RADDR 0x978
#define CSR_RFC_RAM4_DATA 0x97c
#define CSR_RFC_RAM5_CTL 0x980
#define CSR_RFC_RAM5_WADDR 0x984
#define CSR_RFC_RAM5_RADDR 0x988
#define CSR_RFC_RAM5_DATA 0x98c
#define CSR_RFC_RAM6_CTL 0x990
#define CSR_RFC_RAM6_WADDR 0x994
#define CSR_RFC_RAM6_RADDR 0x998
#define CSR_RFC_RAM6_DATA 0x99c
#define CSR_RFC_RAM7_CTL 0x9a0
#define CSR_RFC_RAM7_WADDR 0x9a4
#define CSR_RFC_RAM7_RADDR 0x9a8
#define CSR_RFC_RAM7_DATA 0x9ac
#define CSR_RFC_RAM8_CTL 0x9b0
#define CSR_RFC_RAM8_WADDR 0x9b4
#define CSR_RFC_RAM8_RADDR 0x9b8
#define CSR_RFC_RAM8_DATA 0x9bc
#define CSR_RFC_RAM9_CTL 0x9c0
#define CSR_RFC_RAM9_WADDR 0x9c4
#define CSR_RFC_RAM9_RADDR 0x9c8
#define CSR_RFC_RAM9_DATA 0x9cc
#define CSR_RFC_RAM10_CTL 0x9d0
#define CSR_RFC_RAM10_WADDR 0x9d4
#define CSR_RFC_RAM10_RADDR 0x9d8
#define CSR_RFC_RAM10_DATA 0x9dc
#define CSR_RFC_RAM11_CTL 0x9e0
#define CSR_RFC_RAM11_WADDR 0x9e4
#define CSR_RFC_RAM11_RADDR 0x9e8
#define CSR_RFC_RAM11_DATA 0x9ec
#define CSR_RFC_RAM12_CTL 0x9f0
#define CSR_RFC_RAM12_WADDR 0x9f4
#define CSR_RFC_RAM12_RADDR 0x9f8
#define CSR_RFC_RAM12_DATA 0x9fc

// Command Header Offsets
#define HCMD_ID(x) ((x) & 0xFF)
#define HCMD_SEQ(x) (((x) & 0xFFF) << 12)
#define HCMD_FLAGS(x) (((x) & 0x7) << 24)
#define HCMD_TRIG_TIME_EVT(x) ((x) & 0x1FF)

// Commands
#define WIDE_ID_AND_PAKCET_STATUS 0x00
#define UCODE_ALIVE 0x01
#define REPLY_RX_PHY_NTFY 0x04
#define REPLY_RX_MPDU_NTFY 0x05
#define TX_CMD 0x08
#define REPLY_TX_DONE 0x09
#define MVM_PARK_MCAST_ITERATION_COMPLETE 0x0B
#define ADD_STA_KEY 0x17
#define ADD_STA 0x18
#define REMOVE_STA 0x19
#define TXPATH_FLUSH 0x1C
#define MGMT_MCAST_ITERATION_COMPLETE 0x1E
#define REPLY_SF_CFG_CMD 0x1F
#define BT_COEX_UPDATE_REDUCED_TXP 0x24
#define BT_COEX_CI 0x26
#define MAC_CONTEXT_CMD 0x2E
#define TIME_EVENT_CMD 0x2F
#define BINDx30
#define TIME_QUOTA_CMD 0x31
#define NON_QOS_TX_COUNTER_CMD 0x32
#define STATS_REQUEST 0x33
#define PHY_CONTEXT_CMD 0x34
#define DEBUG_LOG_MSG 0x35
#define SCAN_REQUEST 0x36
#define SCAN_ABORT_CMD 0x37
#define SCAN_COMPLETE_NOTIFY 0x38
#define INIT_EXTENDED 0x39
#define ADD_STA_CMD 0x3A
#define UPDATE_STA_CMD 0x3B
#define REMOVE_STA_CMD 0x3C
#define FW_ERROR_RECOVERY 0x3D
#define SHARED_MEM_CFG_CMD 0x3F
#define TX_CMD_GEN2 0x40
#define ADD_STA_KEY_NOTIF 0x42
#define UPDATE_FT_STATIONS_NOTIF 0x43
#define MFUART_LOAD_NOTIFICATION 0x44
#define RSS_CONFIG_CMD 0x45
#define NVM_ACCESS_CMD 0x46
#define WEP_KEY_IN_TABLE 0x48
#define TDLS_CHANNEL_SWITCH_CMD 0x49
#define TDLS_CONFIG_CMD 0x4A
#define MAC_PM_POWER_TABLE 0x4B
#define TDLS_PEER_UPDATE 0x4C
#define TDLS_PEER_EVENT 0x4D
#define LQ_CMD 0x4E
#define BT_COEX_CI2MEI_CMD 0x4F
#define POWER_TABLE_CMD 0x50
#define PS_QUEUES_INFO 0x51
#define MBSSID_ITERATION_COMPLETE 0x52
#define ADD_COLOR_CMD 0x54
#define REMOVE_COLOR_CMD 0x55
#define HEEP_BINDING_CACHE_CMD 0x56
#define HEEP_AP_BINDING_CACHE_CMD 0x57
#define BT_COEX_UPDATE_SW_BOOST 0x58
#define BT_COEX_UPDATE_BOLT 0x59
#define MAX_CMD 0x60

// Status codes
#define RF_KILL_HW 0x01
#define RF_KILL_SW 0x02
#define RF_KILL_BOTH (RF_KILL_HW | RF_KILL_SW)

// Power management states
#define POWER_STATE_D0 0x00
#define POWER_STATE_D3 0x03

// FIFOs
#define AX2XX_TX_FIFO_CMD 0x0
#define AX2XX_TX_FIFO_HIGH_PRIO 0x1
#define AX2XX_TX_FIFO_LOW_PRIO 0x2
#define AX2XX_TX_FIFO_BEACON 0x3
#define AX2XX_TX_FIFO_MCAST_DATABMC 0x4
#define AX2XX_TX_FIFO_MCAST_CTRL 0x5
#define AX2XX_TX_FIFO_GRP_MCAST_CTRL 0x6
#define AX2XX_TX_FIFO_GRP_MCAST_DATABMC 0x7

// Initialization constants
#define AX2XX_FW_READY_TIMEOUT 5000
#define AX2XX_CMD_QUEUE_SIZE 256
#define AX2XX_RX_QUEUE_SIZE 4096
#define AX2XX_TX_QUEUE_SIZE 4096

// Command queue entry
struct ax2xx_cmd {
    uint16_t len;
    uint8_t id;
    uint8_t flags;
    uint32_t data[];
};

// RX/TX descriptors
struct ax2xx_rx_desc {
    uint32_t uint32_t addr_hi;
    uint16_t len;
    uint16_t flags;
};

struct ax2xx_tx_desc {
    uint32_t addr_lo;
    uint32_t addr_hi;
    uint16_t len;
    uint16_t flags;
    uint32_t rate;
};

// Device structure
struct ax2xx_device {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t base_addr;
    uint32_t irq_line;
    
    // Memory regions
    void *csr_base;
    void *fh_base;
    void *hbus_base;
    void *ureg_base;
    void *lmac_fh_base;
    
    // Queues
    struct ax2xx_cmd *cmd_queue;
    struct ax2xx_rx_desc *rx_ring;
    struct ax2xx_tx_desc *tx_ring;
    uint32_t cmd_head;
    uint32_t cmd_tail;
    uint32_t rx_head;
    uint32_t tx_head;
    
    // Firmware
    uint8_t *firmware;
    uint32_t firmware_size;
    
    // Network interface
    uint8_t mac_addr[6];
    int link_up;
    int initialized;
    
    // Status
    uint32_t rf_kill_status;
    uint8_t power_state;
};

// Helper functions
static inline uint32_t ax2xx_read32(struct ax2xx_device *dev, uint32_t reg) {
    return *(volatile uint32_t*)(dev->csr_base + reg);
}

static inline void ax2xx_write32(struct ax2xx_device *dev, uint32_t reg, uint32_t val) {
    *(volatile uint32_t*)(dev->csr_base + reg) = val;
}

static inline uint16_t ax2xx_read16(struct ax2xx_device *dev, uint32_t reg) {
    return *(volatile uint16_t*)(dev->csr_base + reg);
}

static inline void ax2xx_write16(struct ax2xx_device *dev, uint32_t reg, uint16_t val) {
    *(volatile uint16_t*)(dev->csr_base + reg) = val;
}

static inline uint8_t ax2xx_read8(struct ax2xx_device *dev, uint32_t reg) {
    return *(volatile uint8_t*)(dev->csr_base + reg);
}

static inline void ax2xx_write8(struct ax2xx_device *dev, uint32_t reg, uint8_t val) {
    *(volatile uint8_t*)(dev->csr_base + reg) = val;
}

// PCIe configuration space access
static uint32_t pci_read_config(struct ax2xx_device *dev, uint8_t offset) {
    // This would need to be implemented based on your PCI subsystem
    // For now, assume it's handled by existing kernel PCI functions
    return pci_read_reg(dev->bus, dev->device, dev->function, offset);
}

static void pci_write_config(struct ax2xx_device *dev, uint8_t offset, uint32_t val) {
    pci_write_reg(dev->bus, dev->device, dev->function, offset, val);
}

// Device initialization
static int ax2xx_init_device(struct ax2xx_device *dev) {
    uint32_t reg;
    
    // Reset device
    ax2xx_write32(dev, CSR_RESET, 1);
    for(int i = 0; i < 1000; i++) {
        if(!(ax2xx_read32(dev, CSR_GP_CNTRL)0x1)) {
            break;
        }
    }
    
    // Check hardware revision
    reg = ax2xx_read32(dev, CSR_HW_REV);
    printf("AX2XX HW Rev: 0x%x\n", reg);
    
    // Enable interrupts
    ax2xx_write32(dev, CSR_INT_COALESCING, 0);
    ax2xx_write32(dev, CSR_INT_PERIOD, 0);
    
    // Configure GPIO for LED control
    ax2xx_write32(dev, CSR_LED_REG, 0x00800000);
    
    // Initialize command queue
    dev->cmd_queue = (struct ax2xx_cmd*)get_free_page();
    if(!dev->cmd_queue) {
        printf("Failed to allocate command queue\n");
        return -1;
    }
    
    // Initialize RX ring
    dev->rx_ring = (struct ax2xx_rx_desc*)get_free_page();
    if(!dev->rx_ring) {
        printf("Failed to allocate RX ring\n");
        free_page((paddr_t)dev->cmd_queue);
        return -1;
    }
    
    // Initialize TX ring
    dev->tx_ring = (struct ax2xx_tx_desc*)get_free_page();
    if(!dev->tx_ring) {
        printf("Failed to allocate TX ring\n");
        free_page((paddr_t)dev->cmd_queue);
        free_page((paddr_t)dev->rx_ring);
        return -1;
    }
    
    // Initialize rings
    for(int i = 0; i < AX2XX_CMD_QUEUE_SIZE; i++) {
        dev->rx_ring[i].addr_lo = (uint32_t)get_free_page();
        dev->rx_ring[i].addr_hi = 0;
        dev->rx_ring[i].len = 0;
        dev->rx_ring[i].flags = 0;
    }
    
    dev->cmd_head = 0;
    dev->cmd_tail = 0;
    dev->rx_head = 0;
    dev->tx_head = 0;
    
    // Enable device
    reg = pci_read_config(dev, 0x04); // Command register
    reg |= 0x07; // Bus mastering, memory space, I/O space
    pci_write_config(dev, 0x04, reg);
    
    // Read MAC address
    uint32_t mac_low = ax2xx_read32(dev, CSR_MAC_SHADOW_REG_C0);
    uint32_t mac_mid = ax2xx_read32(dev, CSR_MAC_SHADOW_REG_C1);
    uint32_t mac_high = ax2xx_read32(dev, CSR_MAC_SHADOW_REG_C2);
    
    dev->mac_addr[0] = mac_low & 0xFF;
    dev->mac_addr[1] = (mac_low >> 8) & 0xFF;
    dev->mac_addr[2] = (mac_low >> 16) & 0xFF;
    dev->mac_addr[3] = (mac_low >> 24) & 0xFF;
    dev->mac_addr[4] = mac_mid & 0xFF;
    dev->mac_addr[5] = (mac_mid >> 8) & 0xFF;
    
    printf("AX2XX MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           dev->mac_addr[0], dev->mac_addr[1], dev->mac_addr[2],
           dev->mac_addr[3], dev->mac_addr[4], dev->mac_addr[5]);
    
    return 0;
}

// Firmware loading
static int ax2xx_load_firmware(struct ax2xx_device *dev) {
    // This would typically load firmware from filesystem
    // For now, simulate firmware readiness
    
    // Wait for firmware ready
    uint32_t timeout = AX2XX_FW while(timeout--) {
        uint32_t flag = ax2xx_read32(dev, CSR_GP_DRIVER_REG);
        if(flag & 0x1) { // UCODE READY
            break;
        }
    }
    
    if(timeout == 0) {
        printf("AX2XX: Firmware not ready\n");
        return -1;
    }
    
    printf("AX2XX: Firmware loaded successfully\n");
    return 0;
}

// Command sending
static int ax2xx_send_cmd(struct ax2xx_device *dev, struct ax2xx_cmd *cmd) {
    // Add command to queue
    uint32_t idx = dev->cmd_head %_QUEUE_SIZE;
    memcpy(&dev->cmd_queue[idx], cmd, cmd->len + sizeof(struct ax2xx_cmd));
    dev->cmd_head++;
    
    // Trigger command execution
    ax2xx_write32(dev, CSR_MBOX_SET_REG, 1);
    
    return 0;
}

// Packet transmission
static int ax2xx_transmit(struct ax2xx_device *dev, void *packet, uint32_t length) {
    if(!dev->initialized || !dev->link_up) {
        return -1;
    }
    
    uint32_t idx = dev->tx_head % AX2XX_TX_QUEUE_SIZE;
    struct ax2xx_tx_desc *desc = &dev->tx_ring[idx];
    
    // Setup TX descriptor
    desc->addr_lo = (uint32_t)packet;
    desc->addr_hi = 0;
    desc->len = length;
    desc->flags = 0x0001; // Valid
    desc->rate = 0x00000000; // Auto rate
    
    // Increment head
    dev->tx_head++;
    
    // Notify hardware
    ax2xx_write32(dev, CSR_HBUS_TARG_WRPTR, idx);
    
    return length;
}

// Packet reception
static int ax2xx_receive(struct ax2xx_device *dev, void *buffer, uint32_t max_len) {
    uint32_t idx = dev->rx_head % AX2XX_RX_QUEUE_SIZE;
    struct ax2xx_rx_desc *desc = &dev->rx_ring[idx];
    
    if(desc->flags & 0x0001) { // Descriptor valid
        uint32_t len = desc->len;
        if(len > max_len) len = max_len;
        
        memcpy(buffer, (void*)desc->addr_lo, len);
        
        // Clear descriptor
        desc->flags = 0;
        dev->rx_head++;
        
        return len;
    }
    
    return 0; // No packet available
}

// Interrupt handler
void ax2xx_interrupt_handler(void) {
    for(int i = 0; i < ax2xx_device_count; i++) {
        struct ax2xx_device *dev = &ax2xx_devices[i];
        
        // Read interrupt cause
        uint32_t int_cause = ax2xx_read32(dev, CSR_INT_COALESCING);
        
        if(int_cause & 0x01) { // Command complete
            // Handle command completion
        }
        
        if(int_cause & 0x02) { // RX complete
            // Handle received packets
        }
        
        if(int_cause & 0x04) { // TX complete
            // Handle transmitted packets
        }
        
        // Clear interrupt
        ax2xx_write32(dev, CSR_INT_COALESCING, int_cause);
    }
}

// Device detection and initialization
void ax2xx_init(uint8_t bus, uint8_t device, uint8_t function) {
    if(ax2xx_device_count >= MAX_AX2XX_DEVICES) {
        printf("AX2XX: Too many devices\n");
        return;
    }
    
    struct ax2xx_device *dev = &ax2xx_devices[ax2xx_device_count];
    
    dev->bus = bus;
    dev->device = device;
    dev->function = function;
    
    // Read device info
    dev->vendor_id = pci_read_config(dev, 0x00) & 0xFFFF;
    dev->device_id = (pci_read_config(dev, 0x00) >> 16) & 0xFFFF;
    
    // Verify it's an AX2XX device
    if(dev->vendor_id != INTEL_VENDOR_ID ||
       (dev->device_id != AX200_DEVICE_ID && 
        dev->device_id != AX201_DEVICE_ID &&
        dev->device_id != AX210_DEVICE_ID &&
        dev->device_id != AX416_DEVICE_ID)) {
        return;
    }
    
    printf("Found Intel AX2XX device: %04x:%04x at %d:%d.%d\n", 
           dev->vendor_id, dev->device_id, bus, device, function);
    
    // Get BAR0 (CSR base address)
    uint32_t bar0 = pci_read_config(dev, 0x10);
    dev->base_addr = bar0 & 0xFFFFFFF0; // Mask out flags
    
    // Map memory regions
    dev->csr_base = (void*)dev->base_addr;
    dev->fh_base = (void*)(dev->base_addr + AX2XX_FH_BASE);
    dev->hbus_base = (void*)(dev->base_addr + AX2XX_HBUS_BASE);
    dev->ureg_base = (void*)(dev->base_addr + AX2XX_UREG_BASE);
    dev->lmac_fh_base = (void*)(dev->base_addr + AX2XX_LMAC_FH_BASE);
    
    // Get IRQ line
    dev->irq_line = pci_read_config(dev, 0x3C) & 0xFF;
    
    // Initialize device
    if(ax2xx_init_device(dev) < 0) {
        printf("AX2XX: Device initialization failed\n");
        return;
    }
    
    // Load firmware
    if(ax2xx_load_firmware(dev) < 0) {
        printf("AX2XX: Firmware loading failed\n");
        return;
    }
    
    dev->initialized = 1;
    dev->link_up = 1;
    
    ax2xx_device_count++;
    
    printf("AX2XX: Device initialized successfully\n");
}

// Find AX2XX device
struct ax2xx_device* ax2xx_find_device(void) {
    if(ax2xx_device_count > 0) {
        return &ax2xx_devices[0];
    }
    return NULL;
}

// Public API functions
int ax2xx_transmit_packet(void *packet, uint32_t length) {
    struct ax2xx_device *dev = ax2xx_find_device();
    if(!dev) return -1;
    
    return ax2xx_transmit(dev, packet, length);
}

int ax2xx_receive_packet(void *buffer, uint32_t max_len) {
    struct ax2xx_device *dev = ax2xx_find_device();
    if(!dev) return -1;
    
    return ax2xx_receive(dev, buffer, max_len);
}

void ax2xx_get_mac_address(uint8_t *mac) {
    struct ax2xx_device *dev = ax2xx_find_device();
    if(dev) {
        memcpy(mac, dev->mac_addr, 6);
    }
}

int ax2xx_is_link_up(void) {
    struct ax2xx_device *dev = ax2xx_find_device();
    if(dev) {
        return dev->link_up;
    }
    return 0;
}

// Scan for AX2XX devices on PCIe bus
void ax2xx_scan_pci(void) {
    for(uint8_t bus = 0; bus < 256; bus++) {
        for(uint8_t device = 0; device < 32; device++) {
            for(uint8_t function = 0; function < 8; function++) {
                uint32_t vendor_device = pci_read_reg(bus, device, function, 0x00);
                uint16_t vendor_id = vendor_device & 0xFFFF;
                uint16_t device_id = (vendor_device >> 16) & 0xFFFF;
                
                if(vendor_id == INTEL_VENDOR_ID && 
                   (device_id == AX200_DEVICE_ID || 
                    device_id == AX201_DEVICE_ID ||
                    device_id == AX210_DEVICE_ID ||
                    device_id == AX416_DEVICE_ID)) {
                    
                    printf("Found Intel AX2XX WiFi adapter at %d:%d.%d\n", 
                           bus, device, function);
                    ax2xx_init(bus, device, function);
                }
            }
        }
    }
} 
