#include "crypto.h"
#include "lib/string.h"

static const byte sigma[16] = "expand 32-byte k";

static void chacha20_block(word32* out, const word32* input) {
    word32 x[16];
    memcpy(x, input, 64);
    
    for(int i = 0; i < 20; i += 2) {
        // Column round
        x[0] += x[4]; x[12] = rotr32(x[12] ^ x[0], 16);
        x[1] += x[5]; x[13] = rotr32(x[13] ^ x[1], 16);
        x[2] += x[6]; x[14] = rotr32(x[14] ^ x[2], 16);
        x[3] += x[7]; x[15] = rotr32(x[15] ^ x[3], 16);
        x[8] += x[12]; x[4] = rotr32(x[4] ^ x[8], 12);
        x[9] += x[13]; x[5] = rotr32(x[5] ^ x[9], 12);
        x[10] += x[14]; x[6] = rotr32(x ^[10], 12);
        x[11] += x[15]; x[7] = rotr32(x[7] ^ x[11], 12);
        
        // Diagonal round
        x[0] += x[5]; x[15] = rotr32(x[15] ^ x[0], 8);
        x[1] += x[6]; x[12] = rotr32(x[12] ^ x[1], 8);
        x[2] += x[7]; x[13] = rotr32(x[13] ^ x[2], 8);
        x[3] += x[4]; x[14] = rotr32(x[14] ^ x[3], 8);
        x[8] += x[13]; x[7] = rotr32(x[7] ^ x[8], 7);
        x[9] += x[14]; x[4] = rotr32(x[4] ^ x[9], 7);
        x[10] += x[15]; x[5] = rotr32(x[5] ^ x[10], 7);
        x[11] += x[12]; x[6] = rotr32(x[6] ^ x[11], 7);
    }
    
    for(int i = 0; i < 16; i++) {
        out[i] = x[i] + input[i];
    }
}

void chacha20_init(chacha20_ctx_t* ctx, const byte key[32], const byte nonce[12]) {
    word32* state = (word32*)ctx->state;
    
    state[0] = ((word32)sigma[0] << 24) | ((word32)sigma[1] << 16) |
               ((word32)sigma[2] << 8) | sigma[3];
    state[1] = ((word32)sigma[4] << 24) | ((word32)sigma[5] << 16) |
               ((word32)sigma[6] << 8) | sigma[7];
    state[2] = ((word32)sigma[8] << 24) | ((word32)sigma[9] << 16) |
               ((word32)sigma[10] << 8) | sigma[11];
    state[3] = ((word32)sigma[12] << 24) | ((word32)sigma[13] << 16) |
               ((word32)sigma[14] << 8) | sigma[15];
    
    for(int i = 0; i < 8; i++) {
        state[4+i] = ((word32)key[4*i+0] << 24) |
                     ((word32)key[4*i+1] << 16) |
                     ((word32)key[4*i+2] << 8) |
                     ((word32)key[4*i+3]);
    }
    
    state[12] = 0;
    state[13] = ((word32)nonce[0] << 24) |
                ((word32)nonce[1] << 16) |
                ((word32)nonce[2] << 8) |
                ((word32)nonce[3]);
    state[14] = ((word32)nonce[4] << 24) |
                ((word32)nonce[5] << 16) |
                ((word32)nonce[6] << 8) |
                ((word32)nonce[7]);
    state[15] = ((word32)nonce[8] << 24) |
                ((word32)nonce[9] << 16) |
                ((word32)nonce[10] << 8) |
                ((word32)nonce[11]);
    
    ctx->buf_used = 64;
}

void chacha20_xor(chacha20_ctx_t* ctx, byte* out, const byte* in, size_t len) {
    while(len > 0) {
        if(ctx->buf_used == 64) {
            chacha20_block((word32*)ctx->buf, ctx->state);
            ctx->buf_used = 0;
            ctx->state[12]++;
        }
        
        size_t copy = (len < 64 - ctx->buf_used) ? len : 64 - ctx->buf_used;
        for(size_t i = 0; i < copy; i++) {
            out[i] = in[i] ^ ctx->buf[ctx->buf_used + i];
        }
        ctx->buf_used += copy;
        out += copy;
        in += copy;
        len -= copy;
    }
}
