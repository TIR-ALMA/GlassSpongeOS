#include "crypto.h"
#include "sha2.h"
#include "lib/string.h"

// Модульное возведение в степень (быстрый алгоритм)
static void modexp(word64* res, const word64* base, const word64* exp, const word64* mod, int bits) {
    word64 acc[4] = {1,0,0,0}; // 1
    word64 tmp[4];
    
    for(int i = bits-1; i >= 0; i--) {
        // acc = acc^2 mod mod
        mul_mod(tmp, acc, acc, mod, bits);
        memcpy(acc, tmp, 32);
        
        if((exp[i/64] >> (i%64)) & 1) {
            // acc = acc * base mod mod
            mul_mod(tmp, acc, base, mod, bits);
            memcpy(acc, tmp, 32);
        }
    }
    memcpy(res, acc, 32);
}

// PKCS#1 v1.5 padding для шифрования
static int rsa_pad_pkcs1_v1_5(byte* out, size_t* out_len, const byte* in, size_t in_len, int is_encrypt) {
    size_t key_len = 256; // 2048-bit = 256 bytes
    if(in_len > key_len - 11) return -1;
    
    out[0] = 0x00;
    out[1] = is_encrypt ? 0x02 : 0x01;
    
    // Заполнение случайными ненулевыми байтами
    for(size_t i = 2; i < key_len - in_len - 1; i++) {
        out[i] = 0;
        while(out[i] == 0) {
            out[i] = (byte)(drbg_generate_byte());
        }
    }
    out[key_len - in_len - 1] = 0x00;
    memcpy(out + key_len - in_len, in, in_len);
    
    *out_len = key_len;
    return 0;
}

int rsa_public_encrypt(const rsa_key_t* key, const byte* in, size_t in_len, byte* out, size_t* out_len) {
    byte padded[256];
    size_t padded_len;
    if(rsa_pad_pkcs1_v1_5(padded, &padded_len, in, in_len, 1) < 0) return -1;
    
    // Convert to big-endian integer
    word64 m[4];
    for(int i = 0; i < 4; i++) {
        m[i] = ((word64)padded[8*i+0] << 56) |
               ((word64)padded[8*i+1] << 48) |
               ((word64)padded[8*i+2] << 40) |
               ((word64)padded[8*i+3] << 32) |
               ((word64)padded[8*i+4] << 24) |
               ((word64)padded[8*i+5] << 16) |
               ((word64)padded[8*i+6] << 8) |
               ((word64)padded[8*i+7]);
    }
    
    word64 c[4];
    modexp(c, m, key->e, key->n, 2048);
    
    // Convert back to bytes
    for(int i = 0; i < 4; i++) {
        out[8*i+0] = (byte)(c[i] >> 56);
        out[8*i+1] = (byte)(c[i] >> 48);
        out[8*i+2] = (byte)(c[i] >> 40);
        out[8*i+3] = (byte)(c[i] >> 32);
        out[8*i+4] = (byte)(c[i] >> 24);
        out[8*i+5] = (byte)(c[i] >> 16);
        out[8*i+6] = (byte)(c[i] >> 8);
        out[8*i+7] = (byte)c[i];
    }
    *out_len = 256;
    return 0;
}

int rsa_sign_pkcs1_v1_5(const rsa_key_t* key, const byte* hash, size_t hash_len, byte* sig) {
    // PKCS#1 v1.5 signature format:
    // 00 || 01 || FF...FF || 00 || ASN.1 || hash
    byte digest_info[36];
    memcpy(digest_info, "\x30\x21\x30\x09\x06\x05\x2b\x0e\x03\x02\x1a\x05\x00\x04\x14", 15);
    memcpy(digest_info + 15, hash, 20); // SHA1 only for example
    
    byte padded[256];
    size_t len = 256;
    if(rsa_pad_pkcs1_v1_5(padded, &len, digest_info, 36, 0) < 0) return -1;
    
    // Convert to big-endian integer
    word64 m[4];
    for(int i = 0; i < 4; i++) {
        m[i] = ((word64)padded[8*i+0] << 56) |
               ((word64)padded[8*i+1] << 48) |
               ((word64)padded[8*i+2] << 40) |
               ((word64)padded[8*i+3] << 32) |
               ((word64)padded[8*i+4] << 24) |
               ((word64)padded[8*i+5] << 16) |
               ((word64)padded[8*i+6] << 8) |
               ((word64)padded[8*i+7]);
    }
    
    word64 s[4];
    modexp(s, m, key->d, key->n, 2048);
    
    for(int i = 0; i < 4; i++) {
        sig[8*i+0] = (byte)(s[i] >> 56);
        sig[8*i+1] = (byte)(s[i] >> 48);
        sig[8*i+2] = (byte)(s[i] >> 40);
        sig[8*i+3] = (byte)(s[i] >> 32);
        sig[8*i+4] = (byte)(s[i] >> 24);
        sig[8*i+5] = (byte)(s[i] >> 16);
        sig[8*i+6] = (byte)(s[i] >> 8);
        sig[8*i+7] = (byte)s[i];
    }
    return 0;
}
