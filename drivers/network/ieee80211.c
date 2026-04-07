#include "ieee80211.h"
#include "wifi.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "drivers/timer.h"
#include "mm.h"

struct ieee80211_vif *wifi_vif = NULL;

static struct ieee80211_bss *bss_list[64];
static int bss_count = 0;

// --- Helper functions ---

uint16_t ieee80211_get_duration(struct ieee80211_hdr *hdr, size_t len) {
    // Simplified: for 802.11b/g/n at 1Mbps, 1 byte = 8us + SIFS (10us)
    // In practice, driver handles this; we return dummy value
    return 32 + (len * 8);
}

void ieee80211_send_mgmt_frame(uint8_t *dst, uint8_t subtype, void *data, size_t len) {
    if (!wifi_vif || !wifi_vif->mac_addr) return;

    size_t total_len = sizeof(struct ieee80211_hdr) + len;
    struct ieee80211_hdr *hdr;
    
    hdr = (struct ieee80211_hdr *)kmalloc(total_len);
    if (!hdr) return;

    memset(hdr, 0, sizeof(struct ieee80211_hdr));
    hdr->frame_control = 
        (0 << 2) |                    // Version 0
        (IEEE80211_TYPE_MGMT << 4) | // Type: Management
        (subtype << 8);               // Subtype

    memcpy(hdr->addr1, dst, 6);      // DA
    memcpy(hdr->addr2, wifi_vif->mac_addr, 6); // SA
    if (subtype == IEEE80211_SUBTYPE_BEACON || subtype == IEEE80211_SUBTYPE_PROBE_RESP) {
        memcpy(hdr->addr3, wifi_vif->mac_addr, 6); // BSSID = SA
    } else {
        memcpy(hdr->addr3, wifi_vif->bssid, 6); // BSSID
    }

    hdr->duration_id = htons(ieee80211_get_duration(hdr, len));
    hdr->seq_ctrl = htons((timer_get_ticks() & 0xFFF) << 4);

    if (data && len > 0) {
        memcpy((uint8_t*)hdr + sizeof(struct ieee80211_hdr), data, len);
    }

    // Send via driver (call appropriate send function)
    if (wifi_vif->type == NL80211_IFTYPE_STATION) {
        i8254x_send_80211_frame((uint8_t*)hdr, total_len);
        rtl8168_send_80211_frame((uint8_t*)hdr, total_len);
        i219_send_80211_frame((uint8_t*)hdr, total_len);
        intel_ax2xx_send_80211_frame((uint8_t*)hdr, total_len);
    } else if (wifi_vif->type == NL80211_IFTYPE_AP) {
        // AP mode: send to all stations or specific
        // For now, broadcast
        uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        memcpy(hdr->addr1, broadcast, 6);
        i8254x_send_80211_frame((uint8_t*)hdr, total_len);
    }

    kfree(hdr);
}

int ieee80211_parse_elems(const uint8_t *start, size_t len, struct ieee80211_elem **elems) {
    const uint8_t *pos = start;
    const uint8_t *end = start + len;
    int count = 0;

    while (pos < end && count < 64) {
        if (end - pos < 2)
            break;
        uint8_t id = pos[0];
        uint8_t elen = pos[1];
        if (elen > end - pos - 2)
            break;
        elems[count] = (struct ieee80211_elem *)(pos);
        count++;
        pos += 2 + elen;
    }
    return count;
}

struct ieee80211_elem *ieee80211_find_elem(struct ieee80211_elem **elems, uint8_t id) {
    for (int i = 0; elems[i]; i++) {
        if (elems[i]->id == id)
            return elems[i];
    }
    return NULL;
}

// --- BSS management ---

struct ieee80211_bss *ieee80211_find_bss(uint8_t *bssid) {
    for (int i = 0; i < bss_count; i++) {
        if (memcmp(bss_list[i]->bssid, bssid, 6) == 0)
            return bss_list[i];
    }
    return NULL;
}

void ieee80211_add_bss(uint8_t *bssid, uint8_t *ssid, uint8_t ssid_len,
                       uint16_t capab, uint8_t channel, int8_t rssi, uint64_t tsf,
                       uint32_t freq, uint8_t *rates, uint8_t rates_len,
                       uint8_t *rsn_ie, size_t rsn_len,
                       uint8_t *wpa_ie, size_t wpa_len) {
    struct ieee80211_bss *bss = ieee80211_find_bss(bssid);
    if (bss) {
        // Update existing
        if (ssid_len > 0) memcpy(bss->ssid, ssid, ssid_len);
        bss->ssid_len = ssid_len;
        bss->capab_info = capab;
        bss->channel = channel;
        bss->rssi = rssi;
        bss->last_seen = timer_get_ticks();
        bss->freq = freq;
        if (rates_len <= 16) {
            memcpy(bss->supported_rates, rates, rates_len);
            bss->supported_rates_len = rates_len;
        }
        if (rsn_ie && rsn_len > 0) {
            if (bss->rsn_ie) kfree(bss->rsn_ie);
            bss->rsn_ie = kmalloc(rsn_len);
            memcpy(bss->rsn_ie, rsn_ie, rsn_len);
            bss->rsn_ie_len = rsn_len;
        }
        if (wpa_ie && wpa_len > 0) {
            if (bss->wpa_ie) kfree(bss->wpa_ie);
            bss->wpa_ie = kmalloc(wpa_len);
            memcpy(bss->wpa_ie, wpa_ie, wpa_len);
            bss->wpa_ie_len = wpa_len;
        }
        return;
    }

    // New BSS
    if (bss_count >= 64) {
        printf("BSS list full\n");
        return;
    }

    bss = (struct ieee80211_bss*)kmalloc(sizeof(struct ieee80211_bss));
    if (!bss) return;

    memcpy(bss->bssid, bssid, 6);
    if (ssid_len > 0) memcpy(bss->ssid, ssid, ssid_len);
    bss->ssid_len = ssid_len;
    bss->capab_info = capab;
    bss->channel = channel;
    bss->rssi = rssi;
    bss->last_seen = timer_get_ticks();
    bss->freq = freq;
    if (rates_len <= 16) {
        memcpy(bss->supported_rates, rates, rates_len);
        bss->supported_rates_len = rates_len;
    }
    bss->rsn_ie = NULL;
    bss->rsn_ie_len = 0;
    bss->wpa_ie = NULL;
    bss->wpa_ie_len = 0;

    if (rsn_ie && rsn_len > 0) {
        bss->rsn_ie = kmalloc(rsn_len);
        memcpy(bss->rsn_ie, rsn_ie, rsn_len);
        bss->rsn_ie_len = rsn_len;
    }
    if (wpa_ie && > 0) {
        bss->wpa_ie = kmalloc(wpa_len);
        memcpy(bss->wpa_ie, wpa_ie, wpa_len);
        bss->wpa_ie_len = wpa_len;
    }

    bss_list[bss_count++] = bss;
}

// --- Scan support ---

void ieee80211_scan_start(void) {
    if (!wifi_vif) return;
    wifi_vif->state = IEEE80211_STATE_SCANNING;
    bss_count = 0;

    // Send probe request on each channel (1-14 for 2.4GHz, 36-165 for 5GHz)
    uint8_t channels[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14};
    for (int ch = 0; ch < 14; ch++) {
        wifi_vif->channel = channels[ch];
        wifi_vif->freq = 2407 + 5 * channels[ch]; // 2.4GHz center freq

        // Build probe request
        struct {
            uint16_t capab;
            uint16_t listen_int;
        } __attribute__((packed)) body = {0, 100};

        // SSID element: wildcard (empty)
        uint8_t ssid_elem[2] = {IEEE80211_ELEM_SSID, 0};

        // Supported rates: 1,2,5.5,11 Mbps (802.11b)
        uint8_t rates_elem[8] = {IEEE80211_ELEM_SUPP_RATES, 4, 02, 04, 0B, 16};

        // Assemble payload
        uint8_t payload[32];
        size_t plen = 0;
        memcpy(payload + plen, &body, sizeof(body)); plen += sizeof(body);
        memcpy(payload + plen, ssid_elem, 2); plen += 2;
        memcpy(payload + plen, rates_elem, 8); plen += 8;

        // Send probe request
        uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        ieee80211_send_mgmt_frame(broadcast, IEEE80211_SUBTYPE_PROBE_REQ, payload, plen);

        // Wait100ms per channel
        for (volatile int i = 0; i < 100000; i++);
    }

    wifi_vif->state = IEEE80211_STATE_IDLE;
}

// --- Authentication & Association ---

void ieee80211_authenticate(uint8_t *bssid) {
    if (!wifi_vif) return;
    wifi_vif->state = IEEE80211_STATE_AUTHENTICATING;

    struct {
        uint16_t alg;
        uint16_t transaction;
        uint16_t status;
    } __attribute__((packed)) auth = {0, 1, 0}; // Open system, seq 1

    ieee80211_send_mgmt_frame(bssid, IEEE80211_SUBTYPE_AUTH, &auth, sizeof(auth));
}

void ieee80211_associate(uint8_t *bssid) {
    if (!wifi_vif) return;
    wifi_vif->state = IEEE80211_STATE_ASSOCIATING;

    struct {
        uint16_t capab;
        uint16_t listen_int;
        uint8_t current_ap[6];
    } __attribute__((packed)) assoc = {
        .capab = cpu_to_le16(WLAN_CAPABILITY_ESS),
        .listen_int = cpu_to_le16(100),
        .current_ap = {0}
    };

    // SSID element
    uint8_t ssid_elem[32];
    ssid_elem[0] = IEEE80211_ELEM_SSID;
    ssid_elem[1] = wifi_vif->ssid_len;
    memcpy(ssid_elem + 2, wifi_vif->ssid, wifi_vif->ssid_len);

    // Supported rates
    uint8_t rates_elem[8] = {IEEE80211_ELEM_SUPP_RATES, 4, 02, 04, 0B, 16};

    // Assemble payload
    uint8_t payload[64];
    size_t plen = 0;
    memcpy(payload + plen, &assoc, sizeof(assoc)); plen += sizeof(assoc);
    memcpy(payload + plen, ssid_elem, 2 + wifi_vif->ssid_len); plen += 2 + wifi_vif->ssid_len;
    memcpy(payload + plen, rates_elem, 8); plen += 8;

    ieee80211_send_mgmt_frame(bssid, IEEE80211_SUBTYPE_ASSOC_REQ, payload, plen);
}

// --- Handle incoming frames ---

void ieee80211_rx_mgmt_frame(struct ieee80211_hdr *hdr, size_t len) {
    if (len < sizeof(struct ieee80211_hdr)) return;

    uint8_t subtype = (hdr->frame_control >> 8) & 0xF;
    uint8_t *payload = (uint8_t*)hdr + sizeof(struct ieee80211_hdr);
    size_t plen = len - sizeof(struct ieee80211_hdr);

    switch (subtype) {
        case IEEE80211_SUBTYPE_BEACON:
        case IEEE80211_SUBTYPE_PROBE_RESP: {
            struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt*)hdr;
            struct ieee80211_elem *elems[64];
            int n = ieee80211_parse_elems(payload, plen, elems);

            uint8_t *ssid = NULL;
            uint8_t ssid_len = 0;
            uint8_t *rates = NULL;
            uint8_t rates_len = 0;
            uint8_t *rsn_ie = NULL;
            size_t rsn_len = 0;
            uint8_t *wpa_ie = NULL;
            size_t wpa_len = 0;

            struct ieee80211_elem *e;
            if ((e = ieee80211_find_elem(elems, IEEE80211_ELEM_SSID))) {
                ssid = e->data;
                ssid_len = e->len;
            }
            if ((e = ieee80211_find_elem(elems, IEEE80211_ELEM_SUPP_RATES))) {
                rates = e->data;
                rates_len = e->len;
            }
            if ((e = ieee80211_find_elem(elems, IEEE80211_ELEM_RSN))) {
                rsn_ie = e->data;
                rsn_len = e->len;
            }
            if ((e = ieee80211_find_elem(elems, IEEE80211_ELEM_WPA))) {
                wpa_ie = e->data;
                wpa_len = e->len;
            }

            ieee80211_add_bss(hdr->addr2, ssid, ssid_len,
                              le16_to_cpu(mgmt->beacon.capab_info),
                              wifi_vif->channel, -60, 0, wifi_vif->freq,
                              rates, rates_len, rsn_ie, rsn_len, wpa_ie, wpa_len);
            break;
        }

        case IEEE80211_SUBTYPE_AUTH: {
            struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt*)hdr;
            if (mgmt->auth.auth_transaction == 2) { // Auth response
                if (mgmt->auth.status_code == 0) {
                    printf("Authentication successful\n");
                    ieee80211_associate(hdr->addr2);
                } else {
                    printf("Authentication failed: %d\n", mgmt->auth.status_code);
                    wifi_vif->state = IEEE80211_STATE_DISCONNECTED;
                }
            }
            break;
        }

        case IEEE80211_SUBTYPE_ASSOC_RESP: {
            struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt*)hdr;
            if (mgmt->assoc_resp.status_code == 0) {
                printf("Association successful\n");
                memcpy(wifi_vif->bssid, hdr->addr3, 6);
                wifi_vif->state = IEEE80211_STATE_CONNECTED;
                
                // If WPA2, start 4-way handshake
                if (wifi_vif->security_type == IEEE80211_SECURITY_WPA2_PSK) {
                    wpa_start_4way_handshake(hdr->addr3);
                }
            } else {
                printf("Association failed: %d\n", mgmt->assoc_resp.status_code);
                wifi_vif->state = IEEE80211_STATE_DISCONNECTED;
            }
            break;
        }

        default:
            printf("Unhandled mgmt subtype: %d\n", subtype);
            break;
    }
}
