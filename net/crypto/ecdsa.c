#include "crypto.h"
#include "sha2.h"
#include "lib/string.h"

// Параметры secp256r1
static const word64 p[4] = {
    0xFFFFFFFFFFFFFFFFULL, 0x00000000FFFFFFFFULL,
    0x0000000000000000ULL, 0xFFFFFFFF00000001ULL
};
static const word64 a[4] = {
    0xFFFFFFFFFFFFFFFFULL, 0x00000000FFFFFFFFULL,
    0x0000000000000000ULL, 0xFFFFFFFF00000000ULL
};
static const word64 b[4] = {
    0x5AC635D8AA3A93E7ULL, 0x53FE4B7C026957D2ULL,
    0x401F477430B5D22BULL, 0x76F812664E9E6659ULL
};
static const word64 Gx[4] = {
    0x6B17D1F2E12C4247ULL, 0xEC34EA4F66C3E7A6ULL,
    0xF1E7D19F1E9E1C3AULL, 0x45A6C3C5D4FB8B5AULL
};
static const word64 Gy[4] = {
    0x4FE342E2FE1A7F9BULL, 0xD72B34A8B5E0E82AULL,
    0x2F6B3D535B5A135EULL, 0x7E5A3B6F7B9C84A6ULL
};
static const word64 n[4] = {
    0xFFFFFFFF00000000ULL, 0x00000000FFFFFFFFULL,
    0xBCE6FAADA7179E84ULL, 0xF3B9CAC2FC632551ULL
};

// Умножение точки на скаляр (double-and-add)
static void ecc_mul(ec_point_t* res, const ec_point_t* P, const word64* k, int bits) {
    ec_point_t Q = {{0},{0},{0}}; // ec_point_t R = *P;
    
    for(int i = bits-1; i >= 0; i--) {
        if((k[i/64] >> (i%64)) & 1) {
            ecc_add(&Q, &Q, &R);
        }
        ecc_double(&R, &R);
    }
    *res = Q;
}

// Генерация ключа
void ecdsa_generate_key(ecdsa_key_t* key) {
    do {
        drbg_generate(key->d, 32);
        // Убедимся, что d < n
        if(ct_memcmp(key->d, (const byte*)n, 32) >= 0) continue;
        break;
    } while(1);
    
    // Вычисляем Q = d * G
    ec_point_t G = {.x = {Gx[0],Gx[1],Gx[2],Gx[3]}, 
                 .y = {Gy[0],Gy[1],Gy[2],Gy[3]}, 
                 .z = {1,0,0,0}};
    ecc_mul(&key->Q, &G, key->d, 256);
}

// Подпись ECDSA
int ecdsa_sign(const ecdsa_key_t* key, const byte* hash, size_t hash_len, byte r[32], byte s[32]) {
    byte k_buf[32];
    word64 k[4], z[4], r_val[4], s_val[4];
    
    do {
        drbg_generate(k_buf, 32);
        // k < n
        if(ct_memcmp(k_buf, (const byte*)n, 32) >= 0) continue;
        
        // Convert k to word64
        for(int i = 0; i < 4; i++) {
            k[i] = ((word64)k_buf[8*i+0] << 56) |
                   ((word64)k_buf[8*i+1] << 48) |
                   ((word64)k_buf[8*i+2] << 40) |
                   ((word64)k_buf[8*i+3] << 32) |
                   ((word64)k_buf[8*i+4] << 24) |
                   ((word64)k_buf[8*i+5] << 16) |
                   ((word64)k_buf[8*i+6] << 8) |
                   ((word64)k_buf[8*i+7]);
        }
        
        // Compute R = k * G
        ec_point_t R;
        ec_point_t G = {.x = {Gx[0],Gx[1],Gx[2],Gx[3]}, 
                     .y = {Gy[0],Gy[1],Gy[2],Gy[3]}, 
                     .z = {1,0,0,0}};
        ecc_mul(&R, &G, k, 256);
        
        // r = R.x mod n
        memcpy(r_val, R.x, 32);
        mod(r_val, r_val, n, 256);
        if(ct_memcmp(r_val, (const word64[]){0}, 32) == 0) continue;
        
        // z = hash (truncated to 256 bits)
        memset(z, 0, 32);
        if(hash_len > 32) hash_len = 32;
        memcpy(z, hash, hash_len);
        
        // s = k^{-1} * (z + r * d) mod n
        word64 rd[4];
        mul_mod(rd, r_val, key->d, n, 256);
        add_mod(s_val, z, rd, n, 256);
        word64 k_inv[4];
        modinv(k_inv, k, n, 256);
        mul_mod(s_val, s_val, k_inv, n, 256);
        if(ct_memcmp(s_val, (const word64[]){0}, 32) == 0) continue;
        
        break;
    } while(1);
    
    // Convert r, s to bytes
    for(int i = 0; i < 4; i++) {
        r[8*i+0] = (byte)(r_val[i] >> 56);
        r[8*i+1] = (byte)(r_val[i] >> 48);
        r[8*i+2] = (byte)(r_val[i] >> 40);
        r[8*i+3] = (byter_val[i] >> 32);
        r[8*i+4] = (byte)(r_val[i] >> 24);
        r[8*i+5] = (byte)(r_val[i] >> 16);
        r[8*i+6] = (byte)(r_val[i] >> 8);
        r[8*i+7] = (byte)r_val[i];
        
        s[8*i+0] = (byte)(s_val >> 56);
        s[8*i+1] = (byte)(s_val[i] >> 48);
        s[8*i+2] = (byte)(s_val[i] >> 40);
        s[8*i+3] = (byte)(s_val[i] >> 32);
        s[8*i+4] = (byte)(s_val[i] >> 24);
        s[8*i+5] = (byte)(s_val[i] >> 16);
        s[8*i+6] = (byte)(s_val[i] >> 8);
        s[8*i+7] = (byte)s_val[i];
    }
    return 0;
}
