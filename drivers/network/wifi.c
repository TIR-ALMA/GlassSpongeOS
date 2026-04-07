#include "wifi.h"
#include "ieee80211.h"
#include "lib/printf.h"
#include "mm.h"

int wifi_init(void) {
    if (wifi_vif) return 0;

    wifi_vif = (struct ieee80211_vif*)kmalloc(sizeof(struct ieee80211_vif));
    if (!wifi_vif) return -1;

    memset(wifi_vif, 0, sizeof(struct ieee80211_vif));
    wifi_vif->type = NL80211_IFTYPE_STATION;
    wifi_vif->state = IEEE80211_STATE_IDLE;
    wifi_vif->channel = 1;
    wifi_vif->freq = 2412; // Channel 1

    // Get MAC from driver (example)
    uint8_t mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    memcpy(wifi_vif->mac_addr, mac, 6);

    printf("Wi-Fi initialized (MAC: %02x:%02x:%02x:%02x:%02x:%02x)\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return 0;
}

int wifi_scan_networks(void) {
    if (!wifi_vif) return -1;
    ieee80211_scan_start();
    return bss_count;
}

int wifi_connect(const char *ssid, const char *password, const char *security) {
    if (!wifi_vif || !ssid) return -1;

    // Set SSID
    wifi_vif->ssid_len = strlen(ssid);
    if (wifi_vif->ssid_len > 32) wifi_vif->ssid_len = 32;
    memcpy(wifi_vif->ssid, ssid, wifi_vif->ssid_len);

    // Set security
    if (strcmp(security, "open") == 0) {
        wifi_vif->security_type = IEEE80211_SECURITY_NONE;
    } else if (strcmp(security, "wep") == 0) {
        wifi_vif->security_type = IEEE80211_SECURITY_WEP;
        if (password) strncpy(wifi_vif->psk, password, 63);
    } else if (strcmp(security, "wpa-psk") == 0 || strcmp(security, "wpa2-psk") == 0) {
        wifi_vif->security_type = IEEE80211_SECURITY_WPA2_PSK;
        if (password) strncpy(wifi_vif->psk, password, 63);
        // Derive PMK
        wpa_derive_pmk(password, wifi_vif->ssid, wifi_vif->ssid_len, wifi_vif->pmk);
    } else {
        return -1;
    }

    // Find BSS
    struct ieee80211_bss *bss = NULL;
    for (int i = 0; i < bss_count; i++) {
        if (bss_list[i]->ssid_len == wifi_vif->ssid_len &&
            memcmp(bss_list[i]->ssid, wifi_vif->ssid, wifi_vif->ssid_len) == 0) {
            bss = bss_list[i];
            break;
        }
    }

    if (!bss) {
        printf("SSID '%s' not found in scan results\n", ssid);
        return -1;
    }

    // Start auth/assoc
    ieee80211_authenticate(bss->bssid);
    return 0;
}

int wifi_disconnect(void) {
    if (!wifi_vif) return -1;
    wifi_vif->state = IEEE80211_STATE_DISCONNECTED;
    memset(wifi_vif->bssid, 0, 6);
    return 0;
}

void wifi_rx_frame(uint8_t *frame, size_t len) {
    if (len < 2) return;

    struct ieee80211_hdr *hdr = (struct ieee80211_hdr*)frame;
    uint16_t fc = le16_to_cpu(hdr->frame_control);
    uint8_t type = (fc >> 2) & 0x3;
    uint8_t subtype = (fc >> 4) & 0xF;

    if (type == IEEE80211_TYPE_MGMT) {
        ieee80211_rx_mgmt_frame(hdr, len);
    } else if (type == IEEE80211_TYPE_DATA) {
        // Data frame: decrypt if protected, pass to network stack
        if (fc & IEEE80211_FCTL_PROTECTED) {
            // Decrypt (WEP/TKIP/CCMP) — simplified
            printf("Encrypted data frame received\n");
        }
        // Pass to IP stack
        network_handle_frame((struct ethernet_frame*)frame);
    }
}
