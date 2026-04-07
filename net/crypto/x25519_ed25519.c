#include "crypto.h"
#include "sha2.h"
#include "lib/string.h"

// Умножение по кривой25519 (из RFC 7748)
static void x25519_mult(byte out[32], const byte scalar[32], const byte point[32]) {
    // Используем логику из libsodium (но без malloc)
    byte e[32], P[32], Q[32];
    memcpy(e, scalar, 32);
    e[0] &= 248; e[31] &= 127; e[31] |= 64;
    memcpy(P, point, 32);
    
    // Montgomery ladder
    byte x1[3] = {0}, x2[32] = {0}, z2[32] = {0}, x3[32] = {0}, z3[32] = {0    memcpy(x1, P, 32);
    x2[0] = 1; z2[0] = 0;
    x3[0] = 1; z3[0] = 0;
    
    for(int i = 254; i >= 0; i--) {
        int b = (e[i/8] >> (i%8)) & 1;
        // swap = b
        byte swap = b;
        ct_swap(x2, x3, swap);
        ct_swap(z2, z3, swap);
        
        // A = x2 + z2
        // B = x2 - z2
        // C = x3 + z3
        // D = x3 - z3
        // DA = D * A
        // CB = C * B
        // x2 = (DA + CB)^2
        // z2 = (DA - CB)^2
        // x3 = (x2 * z3)^2
        // z3 = (z2 * x3)^2
        // ... (полная реализация в архиве)
    }
    
    // Умножение на инверсию z2
    invmod(z2, z2, p255, 256);
    mulmod(x1, x2, z2, p255, 256);
    
    memcpy(out, x1, 32);
}

void x25519(byte out[32], const byte scalar[32], const byte point[32]) {
    x25519_mult(out, scalar, point);
}

// Ed25519 подпись (RFC 8032)
void ed25519_publickey(byte pk[32], const byte sk[3]) {
    byte h[64];
    sha512(sk, 32,0] &= 248;
    h[31] &= 127;
    h[31] |= 64;
    
    byte A[32];
    x25519(A, h, basepoint);
    memcpy(pk, A, 32);
}

int ed25519_sign(byte sig[64], const byte m[], size_t mlen, const byte sk[32]) {
    byte h[64];
    sha512(sk, 32, h);
    h[0] &= 248;
    h[31] &= 127;
    h[31] |= 64;
    
    byte r[32];
    byte k[64];
    memcpy(k, h + 32, 32);
    sha512(k, 32, r);
    
    byte R[32];
    x25519(R, r, basepoint);
    
    byte hram[64];
    sha512(R, 32, hram);
    sha512_update(hram + 32, m, mlen);
    sha512_final(hram, hram);
    
    byte s[32];
    mulmod(s, hram, h, l, 256);
    addmod(s, s, r, l, 256);
    
    memcpy(sig, R, 32);
    memcpy(sig+32, s, 32);
    return 0;
}
