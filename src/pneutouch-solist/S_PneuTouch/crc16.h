/* CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF), shared by UART lines and FeRAM. */
#ifndef PNEU_CRC16_H
#define PNEU_CRC16_H
#include <stdint.h>
uint16_t pneu_crc16(const void *data, unsigned size);
#endif
