#include "crypto.h"
#include "lib/string.h"

// CRC32 (standard)
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91
};

uint32_t crypto_crc32(const void* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* p = (const uint8_t*)data;
    for(size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

// Simple RC4 for WEP
void crypto_rc4(uint8_t* key, size_t key_len, uint8_t* data, size_t len) {
    uint8_t s[256];
    for(int i = 0; i < 256; i++) s[i] = i;
    int j = 0;
    for(int i = 0; i < 256; i++) {
        j = (j + s[i] + key[i % key_len]) % 256;
        uint8_t t = s[i]; s[i] = s[j]; s[j] = t;
    }
    int i = 0, k = 0;
    for(size_t n = 0; n < len;++) {
        i = (i + 1) % 256;
        k = (k + s[i]) % 256;
        uint8_t t = s[i]; s[i] = s[k]; s[k] = t;
       ] ^= s[(s[i] + s[k]) % 256];
    }
}
