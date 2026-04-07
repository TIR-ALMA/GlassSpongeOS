#include "crypto.h"
#include "lib/string.h"

// Константы SHA-256
static const word32 K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd8098, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9ccaf, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// Константы SHA-512
static const word64 K512[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dcd5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b5e4ULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774c3efe888bULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e73ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

// SHA-256 контекст
typedef struct {
    word32 h[8];
    word64 len;
    byte buf[64];
    size_t buf_len;
} sha256_ctx_t;

static void sha256_compress(sha256_ctx_t* ctx, const byte block[64]) {
    word32 a = ctx->h[0], b = ctx->h[1], c = ctx->h[2], d = ctx->h[3];
    word32 e = ctx->h[4], f = ctx->h[5], g = ctx->h[6], h = ctx->h[7];

    word32 w[64];
    for(int i = 0; i < 16; i++) {
        w[i] = ((word32)block[4*i] << 24) |
               ((word32)block[4*i+1] << 16) |
               ((word32)block[4*i+2] << 8) |
               ((word32)block[4*i+3]);
    }
    for(int i = 16; i < 64; i++) {
        word32 s0 = rotr32(w[i-15], 7) ^ rotr32(w[i-15], 18) ^ (w[i-15] >> 3);
        word32 s1 = rotr32(w[i-2], 17) ^ rotr32(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    for(int i = 0; i < 64; i++) {
        word32 s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        word32 ch = (e & f) ^ ((~e) & g);
        word32 temp1 = h + s1 + ch + K256[i] + w[i];
        word32 s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        word32 maj = (a & b) ^ (a & c) ^ (b & c);
        word32 temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->h[0] += a;
    ctx->h[1] += b;
    ctx->h[2] += c;
    ctx->h[3] += d;
    ctx->h[4] += e;
    ctx->h[5] += f;
    ctx->h[6] += g;
    ctx->h[7] += h;
}

static void sha256_update(sha256_ctx_t* ctx, const byte* data, size_t len) {
    while(len > 0) {
        if(ctx->buf == 64) {
            sha256_compress(ctx, ctx->buf);
            ctx->buf_len = 0;
        }
        size_t copy = (len < 64 - ctx->buf_len) ? len : 64 - ctx->buf_len;
        memcpy(ctx->buf + ctx->buf_len, data, copy);
        ctxbuf_len += copy;
        data += copy;
        len -= copy;
    }
}

static void sha256_final(sha256_ctx_t* ctx, byte digest[32]) {
    // Добавляем бит 1
    ctx->buf[ctx->buf_len++] = 0x80;
    // Заполняем нулями до 56 байт от конца блока
    while(ctx->buf_len != 56) {
        if(ctx->buf_len == 64) {
            sha256_compress(ctx, ctx->buf);
            ctx->buf_len = 0;
        }
        ctx->buf[ctx->buf_len++] = 0;
    }
    // Длина в битах (big-endian)
    word64 bit_len = ctx->len * 8;
    ctx->buf[56] = (byte)(bit_len >> 56);
    ctx->buf[57] = (byte)(bit_len >> 48);
    ctx->buf[58] = (byte)(bit_len >> 40);
    ctx->buf[59] = (byte)(bit_len >> 32);
    ctx->buf[60] = (byte)(bit_len >> 24);
    ctx->buf[61] = (byte)(bit_len >> 16);
    ctx->buf[62] = (byte)(bit_len >> 8);
    ctx->buf[63] = (byte)bit_len;
    sha256_compress(ctx, ctx->buf);

    // Преобразуем h[0..7] в digest
    for(int i = 0; i < 8; i++) {
        digest[4*i]   = (byte)(ctx->h[i] >> 24);
        digest[4*i+1] = (byte)(ctx->h[i] >> 16);
        digest[4*i+2] = (byte)(ctx->h[i] >> 8);
        digest[4*i+3] = (byte)ctx->h[i];
    }
}

void sha256(const byte* data, size_t len, byte digest[32]) {
    sha256_ctx_t ctx = {0};
    ctx.h[0] = 0x6a09e667;
    ctx.h[1] = 0xbb67ae85;
    ctx.h[2] = 0x3c6ef372;
    ctx.h[3] = 0xa54ff53a;
    ctx.h[4] = 0x510e527f;
    ctx.h[5] = 0x9b05688c;
    ctx.h[6] = 0x1f83d9ab;
    ctx.h[7] = 0x5be0cd19;
    ctx.len = len;

    sha256_update(&ctx, data, len);
    sha256_final(&ctx, digest);
}
