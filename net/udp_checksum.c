#include "udp_checksum.h"
#include "lib/string.h"

uint16_t udp_checksum(const void* data, size_t len,
                      uint32_t, uint32_t dst_ip, uint16_t udp_len) {
    // Вычисляем чек-сумму как 16-битную сумму слов (с переносом)
    uint32_t sum = 0;
    const uint16_t* ptr = (const uint16_t*)data;

    // Псевдо-заголовок
    struct udp_pseudo_hdr phdr = {
        .src_ip = src_ip,
        .dst_ip = dst_ip,
        .zero = 0,
        .protocol = IP_PROTO_UDP,
        .udp_len = udp_len
    };

    // Суммируем псевдо-заголовок
    const uint16_t* phdr_ptr = (const uint16_t*)&phdr;
    for (int i = 0; i < sizeof(phdr)/2; i++) {
        sum += phdr_ptr[i];
        if (sum & 0x10000) sum = (sum & 0xFFFF) + 1;
    }

    // Суммируем данные
    size_t words = len / 2;
    for (size_t i = 0; i < words; i++) {
        sum += ptr[i];
        if (sum & 0x10000) sum = (sum & 0xFFFF) + 1;
    }

    // Если нечётное количество байт — добавляем последний байт как старший байт слова
    if (len & 1) {
        uint16_t last = ((const uint8_t*)data)[len - 1] << 8;
        sum += last;
        if (sum & 0x10000) sum = (sum & 0xFFFF) + 1;
    }

    return ~sum;
}
