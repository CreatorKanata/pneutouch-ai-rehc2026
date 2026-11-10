#include "crc16.h"

uint16_t pneu_crc16(const void *data, unsigned size)
{
    const uint8_t *p = data;
    uint16_t crc = 0xffffU;
    unsigned i;
    while (size--) {
        crc ^= (uint16_t)(*p++ << 8);
        for (i = 0; i < 8; ++i) crc = (uint16_t)(crc & 0x8000U ? (crc << 1)^0x1021U : crc << 1);
    }
    return crc;
}
