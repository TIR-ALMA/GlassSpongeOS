#include "ethernet.h"
#include "network.h"
#include "drivers/network/i8254x.h"
#include "drivers/network/rtl8168.h"
#include "drivers/network/i219.h"
#include "drivers/network/intel_wifi.h"
#include "drivers/network/intel_ax2xx.h"
#include "drivers/network/intel_ethernet.h"
#include "lib/printf.h"

static int (*driver_send)(struct ethernet_frame*) = NULL;
static void (*driver_rx_handler)(struct ethernet_frame*) = NULL;

int ethernet_init(void) {
    if(i8254x_is_initialized()) {
        driver_send = i8254x_send_frame;
        driver_rx_handler = i8254x_rx_handler;
    } else if(rtl8168_is_initialized()) {
        driver_send = rtl8168_send_frame;
        driver_rx_handler = rtl8168_rx_handler;
    } else if(i219_is_initialized()) {
        driver_send = i219_send_frame;
        driver_rx_handler = i219_rx_handler;
    } else if(intel_wifi_is_initialized()) {
        driver_send = intel_wifi_send_frame;
        driver_rx_handler = intel_wifi_rx_handler;
    } else if(intel_ax2xx_is_initialized()) {
        driver_send = intel_ax2xx_send_frame;
        driver_rx_handler = intel_ax2xx_rx_handler;
    } else if(intel_ethernet_is_initialized()) {
        driver_send = intel_ethernet_send_frame;
        driver_rx_handler = intel_ethernet_rx_handler;
    } else {
        printf("ERROR: No network driver initialized!\n");
        return -1;
    }
    return 0;
}

int ethernet_send_frame(struct ethernet_frame* frame) {
    if(!driver_send) return -1;
    return driver_send(frame);
}

void ethernet_rx_callback(struct ethernet_frame* frame) {
    if(driver_rx_handler) {
        driver_rx_handler(frame);
    }
}
