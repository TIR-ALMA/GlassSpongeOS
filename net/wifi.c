#include "wifi.h"
#include "lib/printf.h"
#include "drivers/network/intel_wifi.h"
#include "drivers/network/intel_ax2xx.h"

int wifi_init(void) {
    if(intel_wifi_is_initialized()) {
        intel_wifi_scan();
        return 0;
    }
    if(intel_ax2xx_is_initialized()) {
        intel_ax2xx_scan();
        return 0;
    }
    return -1;
}

int wifi_connect(const char* ssid, const char* password) {
    if(intel_wifi_is_initialized()) {
        return intel_wifi_connect(ssid, password);
    }
    if(intel_ax2xx_is_initialized()) {
        return intel_ax2xx_connect(ssid, password);
    }
    return -1;
}
