#ifndef WIFI_H
#define WIFI_H

#include "ieee80211.h"

int wifi_init(void);
int wifi_scan_networks(void);
int wifi_connect(const char *ssid, const char *password, const char *security);
int wifi_disconnect(void);
int wifi_set_channel(uint8_t channel);
int wifi_get_rssi(uint8_t *bssid);
void wifi_rx_frame(uint8_t *frame, size_t len);

// Driver callbacks (to be implemented per hardware)
void i8254x_send_80211_frame(uint8_t *frame, size_t len);
void rtl8168_send_80211_frame(uint8_t *frame, size_t len);
void i219_send_80211_frame(uint8_t *frame, size_t len);
void intel_ax2xx_send_80211_frame(uint8_t *frame, size_t len);

#endif
