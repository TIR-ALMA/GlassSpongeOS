#include "crypto.h"
#include "chacha20.h"

static chacha20_ctx_t drbg_ctx;
static byte drbg_key[32];
static byte drbg_v[64];
static int drbg_inited = 0;

void drbg_reseed(const byte* seed, size_t seed_len) {
    byte input[128];
    memset(input, 0, 128);
    memcpy(input, drbg_v, 64);
    memcpy(input+64, seed, seed_len);
    
    chacha20_ctx_t tmp;
    chacha20_init(&tmp, drbg_key, (const byte*)"DRBG");
    chacha20_xor(&tmp, drbg_key, input, 32);
    chacha20_xor(&tmp, drbg_v, input+32, 64);
    
    drbg_inited = 1;
}

void drbg_generate(byte* out, size_t len) {
    if(!drbg_inited) {
        byte seed[32];
        drbg_reseed(seed, 32);
    }
    
    while(len > 0) {
        size_t chunk = (len > 64) ? 64 : len;
        chacha20_xor(&drbg_ctx, out, drbg_ctx.buf + drbg_ctx.buf_used, chunk);
        drbg_ctx.buf_used += chunk;
        out += chunk;
        len -= chunk;
        
        if(drbg_ctx.buf_used == 64) {
            chacha20_block((word32*)drbg_ctx.buf, drbg_ctx.state);
            drbg_ctx.buf_used = 0;
            drbg_ctx.state[12]++;
        }
    }
}
