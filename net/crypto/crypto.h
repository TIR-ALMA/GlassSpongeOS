#ifndef CRYPTO_H
#define CRYPTO_H

#include "types.h"

// --- Типы данных ---
typedef uint8_t byte;
typedef uint32_t word32;
typedef uint64_t word64;

// Хэш-алгоритмы
void sha256(const byte* data, size_t len, byte digest[32]);
void sha384(const byte* data, size_t len, byte digest[48]);
void sha512(const byte* data, size_t len, byte digest[64]);

// AES
typedef struct {
    word32 rk[60];   // Round keys (max for AES-256: 14 rounds → 15*4=60 words)
    int rounds;
} aes_key_t;

int aes_setkey_enc(aes_key_t* ctx, const byte* key, size_t key_len);
int aes_encrypt_ecb(const aes_key_t* ctx, const byte in[16], byte out[16]);
int aes_decrypt_ecb(const aes_key_t* ctx, const byte in[16], byte out[16]);

// AES-CBC
int aes_encrypt_cbc(const aes_key_t* ctx, const byte* iv, const byte* in, byte* out, size_t len);
int aes_decrypt_cbc(const aes_key_t* ctx, const byte* iv, const byte* in, byte* out, size_t len);

// AES-CTR
int aes_encrypt_ctr(const aes_key_t* ctx, const byte* nonce, byte* in_out, size_t len);

// AES-GCM
typedef struct {
    aes_key_t aes;
    byte H[16];      // Hash subkey
    byte Y0[16];     // Initial counter block
    byte X[16];      // Running hash state
    word64 aad_len;
    word64 text_len;
} gcm_context_t;

int gcm_init(gcm_context_t* ctx, const byte* key, size_t key_len);
int gcm_encrypt(gcm_context_t* ctx, const byte* iv, size_t iv_len,
                const byte* aad, size_t aad_len,
                const byte* in, byte* out, size_t len,
                byte* tag, size_t tag_len);
int gcm_decrypt(gcm_context_t* ctx, const byte* iv, size_t iv_len,
                const byte* aad, size_t aad_len,
                const byte* in, byte* out, size_t len,
                const byte* tag, size_t tag_len);

// RSA
typedef struct {
    word64 n[4];   // 2048-bit modulus (4×64 = 256 bytes)
    word64 e[1];   // public exponent (usually 65537)
    word64 d[4];   // private exponent
    word64 p[2];   // prime p
    word64 q[2];   // prime q
    word64 dp[2];  // d mod (p-1)
    word64 dq[2];  // d mod (q-1)
    word64 qinv[2]; // (q^-1) mod p
} rsa_key_t;

int rsa_public_encrypt(const rsa_key_t* key, const byte* in, size_t in_len, byte* out, size_t* out_len);
int rsa_private_decrypt(const rsa_key_t* key, const byte* in, size_t in_len, byte* out, size_t* out_len);
int rsa_sign_pkcs1_v1_5(const rsa_key_t* key, const byte* hash, size_t hash_len, byte* sig);
int rsa_verify_pkcs1_v1_5(const rsa_key_t* key, const byte* hash, size_t hash_len, const byte* sig);

// ECDSA (secp256r1)
typedef struct {
    word64 x[4];   // 256-bit field element
    word64 y[4];
    word64 z[4];   // projective coord
} ec_point_t;

typedef struct {
    word64 d[4];   // private key
    ec_point_t Q;  // public key
} ecdsa_key_t;

int ecdsa_sign(const ecdsa_key_t* key, const byte* hash, size_t hash_len, byte r[32], byte s[32]);
int ecdsa_verify(const ecdsa_key_t* key, const byte* hash, size_t hash_len, const byte r[32], const byte s[32]);

// HMAC
void hmac_sha256(const byte* key, size_t key_len, const byte* data, size_t data_len, byte mac[32]);

// PBKDF2
void pbkdf2_hmac_sha256(const byte* password, size_t pass_len,
                        const byte* salt, size_t salt_len,
                        uint32_t iterations,
                        byte* out, size_t out_len);

// ChaCha20-Poly1305
typedef struct {
    word32 state[16];
    byte buf[64];
    size_t buf_used;
} chacha20_ctx_t;

void chacha20_init(chacha20_ctx_t* ctx, const byte key[32], const byte nonce[12]);
void chacha20_xor(chacha20_ctx_t* ctx, byte* out, const byte* in, size_t len);

typedef struct {
    chacha20_ctx_t cipher;
    byte poly_key[32];
    byte tag[16];
    word64 aad_len;
    word64 text_len;
} chacha20poly1305_ctx_t;

int chacha20poly1305_encrypt(chacha20poly1305_ctx_t* ctx,
                             const byte* nonce, const byte* aad, size_t aad_len,
                             const byte* in, byte* out, size_t len,
                             byte* tag);
int chacha20poly1305_decrypt(chacha20poly1305_ctx_t* ctx,
                             const byte* nonce, const byte* aad, size_t aad_len,
                             const byte* in, byte* out, size_t len,
                             const byte* tag);

// X25519 / Ed25519
void x25519(byte out[32], const byte scalar[32], const byte point[32]);
void ed25519_publickey(byte pk[32], const byte sk[32]);
int ed25519_sign(byte sig[64], const byte m[], size_t mlen, const byte sk[32]);
int ed25519_verify(const byte sig[64], const byte m[], size_t mlen, const byte pk[32]);

// Base64
size_t base64_encode(const byte* src, size_t len, char* dst);
size_t base64_decode(const char* src, byte* dst);

// CSRNG (DRBG)
void drbg_reseed(const byte* seed, size_t seed_len);
void drbg_generate(byte* out, size_t len);

// Утилиты
void memzero(void* p, size_t n);
int ct_memcmp(const void* a, const void* b, size_t n);  // constant-time

#endif
