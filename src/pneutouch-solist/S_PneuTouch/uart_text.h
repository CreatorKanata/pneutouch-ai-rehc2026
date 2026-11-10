/* Tiny UART text helpers shared by the line protocols (no printf). */
#ifndef PNEU_UART_TEXT_H
#define PNEU_UART_TEXT_H
#include <stdbool.h>
#include <stdint.h>
void pneu_text(const char *p);
void pneu_number(uint32_t n);
void pneu_hex16(uint16_t n);
void pneu_hex32(uint32_t n);
int pneu_hex_digit(char c);          /* -1 when not a hex digit */
bool pneu_integer(const char *s, uint32_t *n); /* decimal, no sign, overflow-checked */
/* count float32 values, 8 hex digits each, high nibble first.
   0 ok, 1 not hex, 2 non-finite (stops at the first problem). */
int pneu_parse_f32_hex(const char *p, float *out, unsigned count);
#endif
