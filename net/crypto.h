#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>
#include <stddef.h>

uint32_t crypto_crc32(const void* data, size_t len);
void crypto_rc4(uint8_t* key, size_t key_len, uint8_t* data, size_t len);

#endif

