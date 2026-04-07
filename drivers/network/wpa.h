#ifndef WPA_H
#define WPA_H

#include "ieee80211.h"
#include "crypto/aes.h"
#include "crypto/sha1.h"
#include "crypto/hmac.h"

void wpa_derive_pmk(const char *passphrase, const uint8_t *ssid, size_t ssid_len, uint8_t *pmk);
void wpa_calc_ptk(const uint8_t *anonce, const uint8_t *snonce,
                  const uint8_t *aa, const uint8_t *spa,
                  const uint8_t *pmk, uint8_t *ptk, size_t ptk_len);
void wpa_start_4way_handshake(uint8_t *bssid);

#endif
