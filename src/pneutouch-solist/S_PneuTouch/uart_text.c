#include "uart_text.h"
#include "board.h"
#include <string.h>

void pneu_text(const char *p) { while (*p) board_putc(*p++); }

void pneu_number(uint32_t n)
{
    char s[10]; unsigned i = 0;
    do { s[i++] = (char)('0'+n%10U); n /= 10U; } while (n);
    while (i) board_putc(s[--i]);
}

static void hex_bits(uint32_t n, int top)
{
    static const char digits[] = "0123456789abcdef";
    int i;
    for (i = top; i >= 0; i -= 4) board_putc(digits[(n >> i)&15U]);
}
void pneu_hex16(uint16_t n) { hex_bits(n, 12); }
void pneu_hex32(uint32_t n) { hex_bits(n, 28); }

int pneu_hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c-'0';
    if (c >= 'a' && c <= 'f') return c-'a'+10;
    if (c >= 'A' && c <= 'F') return c-'A'+10;
    return -1;
}

bool pneu_integer(const char *s, uint32_t *n)
{
    uint32_t v = 0;
    if (!*s) return false;
    while (*s) {
        unsigned digit = (unsigned)(*s++-'0');
        if (digit > 9 || v > (UINT32_MAX-digit)/10U) return false;
        v = v*10U+digit;
    }
    *n = v;
    return true;
}

int pneu_parse_f32_hex(const char *p, float *out, unsigned count)
{
    unsigned i, j;
    for (i = 0; i < count; ++i) {
        uint32_t bits = 0;
        for (j = 0; j < 8; ++j) {
            int digit = pneu_hex_digit(*p++);
            if (digit < 0) return 1;
            bits = (bits << 4)|(uint32_t)digit;
        }
        if ((bits & 0x7f800000U) == 0x7f800000U) return 2;
        memcpy(&out[i], &bits, sizeof(bits));
    }
    return 0;
}
