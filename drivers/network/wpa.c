#include "wpa.h"
#include "ieee80211.h"
#include "lib/printf.h"
#include "mm.h"

void wpa_derive_pmk(const char *passphrase, const uint8_t *ssid, size_t ssid_len, uint8_t *pmk) {
    // PBKDF2-HMAC-SHA1(passphrase, ssid, 4096)
    uint8_t salt[32];
    memcpy(salt, ssid, ssid_len);
    if (ssid_len < 32) memset(salt + ssid_len, 0, 32 - ssid_len);

    uint8_t key[32];
    pbkdf2_hmac_sha1((const uint8_t*)passphrase, strlen(passphrase),
                     salt, 32, key, 32);

    memcpy(pmk, key, 32);
}

void wpa_calc_ptk(const uint8_t *anonce, const uint8_t *snonce,
                  const uint8_t *aa, const uint8_t *spa,
                  const uint8_t *pmk, uint8_t *ptk, size_t ptk_len) {
    // PTK = PRF-X(PMK, "Pairwise key expansion", Min(AA,SPA) || Max(AA,SPA) || Min(ANonce,SNonce) || Max(ANonce,SNonce))
    uint8_t buf[100];
    size_t len = 0;

    // Determine min/max AA/SPA
    if (memcmp(aa, spa, 6) < 0) {
        memcpy(buf, aa, 6);
        memcpy(buf + 6, spa, 6);
    } else {
        memcpy(buf, spa, 6);
        memcpy(buf + 6, aa, 6);
    }
    len = 12;

    // Min/Max ANonce/SNonce
    if (memcmp(anonce, snonce, 32) < 0) {
        memcpy(buf + len, anonce, 32);
        memcpy(buf + len + 32, snonce, 32);
    } else {
        memcpy(buf + len, snonce, 32);
        memcpy(buf + len + 32, anonce, 32);
    }
    len += 64;

    // PRF-512
    uint8_t key[64];
    prf_512(pmk, 32, (uint8_t*)"Pairwise key expansion", 22, buf, len, key);

    memcpy(ptk, key, ptk_len);
}

void wpa_start_4way_handshake(uint8_t *bssid) {
    if (!wifi_vif) return;

    // Generate SNonce
    uint8_t snonce[32];
    for (int i = 0; i < 32; i++) snonce[i] = (uint8_t)timer_get_ticks() ^ i;

    // Build EAPOL-Key frame (Message 1: ANonce from AP)
    // In real impl, we wait for AP's Msg1, then send Msg2
    // Here we simulate: assume AP sent ANonce in beacon? No — we need real RX.

    // Instead: when we receive EAPOL-Key (Msg1), we respond with Msg2
    // So this function is called after receiving Msg1
    printf("WPA2 4-way handshake started\n");
}
