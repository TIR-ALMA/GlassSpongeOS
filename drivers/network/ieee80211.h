#ifndef IEEE80211_H
#define IEEE8021_H

#include "types.h"
#include "lib/string.h"

// Frame Control bits
#define IEEE80211_FCTL_VERSION_MASK     0x0003
#define IEEE80211_FCTL_TYPE_MASK        0x000C
#define IEEE80211_FCTL_SUBTYPE_MASK     0x00F0
#define IEEE80211_FCTL_TO_DS            0x0100
#define IEEE80211_FCTL_FROM_DS          0x0200
#define IEEE80211_FCTL_MORE_FRAG        0x0400
#define IEEE80211_FCTL_RETRY            0x0800
#define IEEE80211_FCTL_PWR_MGT          0x1000
#define IEEE80211_FCTL_MORE_DATA        0x2000
#define IEEE80211_FCTL_PROTECTED        0x4000
#define IEEE80211_FCTL_ORDER            0x8000

#define IEEE80211_TYPE_MGMT             0x00
#define IEEE80211_TYPE_CTRL             0x01
#define IEEE80211_TYPE_DATA             0x02

#define IEEE80211_SUBTYPE_ASSOC_REQ     0x00
#define IEEE80211_SUBTYPE_ASSOC_RESP    0x01
#define IEEE80211_SUBTYPE_REASSOC_REQ   0x02
#define IEEE80211_SUBTYPE_REASSOC_RESP  0x03
#define IEEE80211_SUBTYPE_PROBE_REQ     0x04
#define IEEE80211_SUBTYPE_PROBE_RESP    0x05
#define IEEE80211_SUBTYPE_BEACON        0x08
#define IEEE80211_SUBTYPE_ATIM          0x09
#define IEEE80211_SUBTYPE_DISASSOC      0x0A
#define IEEE80211_SUBTYPE_AUTH          0x0B
#define IEEE80211_SUB_DEAUTH        0x0C
#define IEEE80211_SUBTYPE_ACTION        0x0D

#define IEEE80211_QOS_CTL_LEN           2
#define IEEE80211_HT_CTL_LEN            4

// Management frame elements
#define IEEE80211_ELEM_SSID             0
#define IEEE80211_ELEM_SUPP_RATES       1
#define IEEE80211_ELEM_FH_PARAM_SET     2
#define IEEE80211_ELEM_DS_PARAM_SET     3
#define IEEE80211_ELEM_CF_PARAM_SET     4
#define IEEE80211_ELEM_TIM              5
#define IEEE80211_ELEM_IBSS_PARAM_SET   6
#define IEEE80211_ELEM_COUNTRY          7
#define IEEE80211_ELEM_POWER_CONSTRAINT 32
#define IEEE80211_ELEM_CHANNEL_SWITCH   37
#define IEEE80211_ELEM_EXT_SUPP_RATES   50
#define IEEE80211_ELEM_RSN              48
#define IEEE80211_ELEM_WPA              221 // Vendor-specific: 00-50-F2-01

// Security
#define WLAN_CIPHER_SUITE_NONE          0x000FAC00
#define WLAN_CIPHER_SUITE_WEP40         0x000FAC01
#define WLAN_CIPHER_SUITE_TKIP          0x000FAC02
#define WLAN_CIPHER_SUITE_CCMP          0x000FAC04
#define WLAN_CIPHER_SUITE_GCMP          0x000FAC08
#define WLAN_CIPHER_SUITE_AES_CMAC      0000FAC06
#define WLAN_CIPHER_SUITE_SMS4          0x000FAC05

#define WLAN_AKM_SUITE_8021X            0x000FAC01
#define WLAN_AKM_SUITE_PSK              0x000FAC02

// Capabilities
#define WLAN_CAPABILITY_ESS             0x0001
#define WLAN_CAPABILITY_IBSS            0x0002
#define WLAN_CAPABILITY_CF_POLLABLE     0x0004
#define WLAN_CAPABILITY_CF_POLLREQ      0x0008
#define WLAN_CAPABILITY_PRIVACY         0x0010
#define WLAN_CAPABILITY_SHORT_PREAMBLE  0x0020
#define WLAN_CAPABILITY_PBCC            0x0040
#define WLAN_CAPABILITY_CHANNEL_AGILITY 0x0080
#define WLAN_CAPABILITY_SPECTRUM_MGMT   0x0100
#define WLAN_CAPABILITY_QOS             0x0200
#define WLAN_CAPABILITY_SHORT_SLOT_TIME 0x0400
#define WLAN_CAPABILITY_DSSS_OFDM       0x2000
#define WLAN_CAPABILITY_DELAYED_BA      0x4000
#define WLAN_CAPABILITY_IMMEDIATE_BA    0x8000

struct ieee80211_hdr {
    uint16_t frame_control;
    uint16_t duration_id;
    uint8_t addr1[6];
    uint8_t addr2[6];
    uint8_t addr3[6];
    uint16_t seq_ctrl;
} __attribute__((packed));

struct ieee80211_qos_hdr {
    struct ieee80211_hdr hdr;
    uint16_t qos_ctl;
} __attribute__((packed));

struct ieee80211_mgmt {
    struct ieee80211_hdr hdr;
    union {
        struct {
            uint16_t auth_alg;
            uint16_t auth_transaction;
            uint16_t status_code;
        } __attribute__((packed)) auth;
        struct {
            uint16_t capab_info;
            uint16_t listen_interval;
            uint8_t current_ap[6];
        } __attribute__((packed)) assoc_req;
        struct {
            uint16_t capab_info;
            uint16_t status_code;
            uint16_t aid;
        __attribute__((packed)) assoc_resp;
        struct {
            uint16_t reason_code;
        } __attribute__((packed)) deauth;
        struct {
            uint16_t reason_code;
        } __attribute__((packed)) disassoc;
        struct {
            uint64_t timestamp;
            uint16_t beacon_int;
            uint16_t capab_info;
        } __attribute__((packed)) beacon;
        struct {
            uint64_t timestamp;
            uint16_t beacon_int;
            uint16_t capab_info;
        } __attribute__((packed)) probe_req;
        struct {
            uint64_t timestamp;
            uint16_t beacon_int;
            uint16_t capab_info;
        } __attribute__((packed)) probe_resp;
    };
} __attribute__((packed));

struct ieee80211_elem {
    uint8_t id;
    uint8_t len;
    uint8_t data[];
} __attribute__((packed));

struct ieee80211_bss {
    uint8_t bssid[6];
    uint8_t ssid[32];
    uint8_t ssid_len;
    uint16_t capab_info;
    uint8_t channel;
    int8_t rssi;
    uint64_t last_seen;
    uint32_t freq; // in MHz
    uint8_t supported_rates[16];
    uint8_t supported_rates_len;
    uint8_t *rsn_ie;
    size_t rsn_ie_len;
    uint8_t *wpa_ie;
    size_t wpa_ie_len;
};

struct ieee80211_vif {
    enum {
        NL80211_IFTYPE_STATION,
        NL80211_IFTYPE_AP,
        NL80211_IFTYPE_MONITOR
    } type;
    
    uint8_t mac_addr[6];
    uint8_t bssid[6];
    uint8_t ssid[32];
    uint8_t ssid_len;
    uint8_t channel;
    uint32_t freq;
    
    // Security
    enum {
        IEEE80211_SECURITY_NONE,
        IEEE80211_SECURITY_WEP,
        IEEE80211_SECURITY_WPA_PSK,
        IEEE80211_SECURITY_WPA2_PSK
    } security_type;
    
    char psk[64]; // ASCII passphrase (max 63 chars)
    uint8_t pmk[32]; // Pairwise Master Key (from PSK)
    uint8_t ptk[80]; // Pairwise Transient Key (TKIP/CCMP)
    uint8_t gtk[32]; // Group Temporal Key
    
    // State machine
    enum {
        IEEE80211_STATE_IDLE,
        IEEE80211_STATE_SCANNING,
        IEEE80211_STATE_AUTHENTICATING,
        IEEE80211_STATE_ASSOCIATING,
        IEEE80211_STATE_CONNECTED,
        IEEE80211_STATE_DISCONNECTED
    } state;
};

extern struct ieee80211_vif *wifi_vif;

// Utility functions
uint16_t ieee80211_get_duration(struct ieee80211_hdr *hdr, size_t len);
void ieee80211_send_mgmt_frame(uint8_t *dst, uint8_t subtype, void *data, size_t len);
int ieee80211_parse_elems(const uint8_t *start, size_t len, struct ieee80211_elem **elems);
struct ieee80211_elem *ieee80211_find_elem(struct ieee80211_elem **elems, uint8_t id);

#endif
