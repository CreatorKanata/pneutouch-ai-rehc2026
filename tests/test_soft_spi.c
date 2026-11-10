/* SoftSpi.c bit-bang against a simulated SPI mode 3 slave on a fake Port 8. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* Every P8DO store goes through port_write so the slave sees each edge. */
#define RDWR_REG_H__
#define read_reg32(a)    ((uint32_t)(a))
#define write_reg32(a, d) port_write(&(a), (uint32_t)(d))
#define set_reg32(a, m)  port_write(&(a), (uint32_t)(a) | (uint32_t)(m))
#define clear_reg32(a, m) port_write(&(a), (uint32_t)(a) & ~(uint32_t)(m))
#define set_bit(n, c)    set_reg32(n, c)
#define clear_bit(n, c)  clear_reg32(n, c)
#define get_bit(n, c)    ((read_reg32(n) & (c)) != 0)
#define write_bit(n, c, d) do { clear_bit(n, c); set_bit(n, d); } while (0)
static void port_write(uint32_t *reg, uint32_t value);

#include "mcu.h"
#include "SoftSpi.c"

#define SCK (1U << 5U)
#define MOSI (1U << 4U)
#define MISO (1U << 3U)
#define SPI_PINS (SCK | MOSI)

test_port8_t test_port8;
static uint8_t slave_out[16], slave_in[16];
static unsigned out_bits, in_bits, falls, rises, byte_falls[16], byte_rises[16];
static uint32_t frame; /* P8DO outside SCK/MOSI while a byte is on the wire */
static bool check_frame;
static void port_write(uint32_t *reg, uint32_t value)
{
    uint32_t old = *reg;
    *reg = value;
    if (reg != &test_port8.P8DO) return;
    if (check_frame) assert((value & ~SPI_PINS) == frame); /* CS and other bits untouched */
    if ((old & SCK) && !(value & SCK)) { /* falling edge: slave drives its next bit */
        unsigned bit = out_bits++;
        ++falls;
        if (slave_out[bit/8U] & (0x80U >> (bit%8U))) test_port8.P8DI |= MISO;
        else test_port8.P8DI &= ~MISO;
    } else if (!(old & SCK) && (value & SCK)) { /* rising edge: slave samples MOSI */
        unsigned bit = in_bits++;
        ++rises;
        if (value & MOSI) slave_in[bit/8U] |= (uint8_t)(0x80U >> (bit%8U));
        if (in_bits%8U == 0U) { byte_falls[bit/8U] = falls; byte_rises[bit/8U] = rises; }
    }
}

static void start(const uint8_t *out, unsigned n)
{
    memset(slave_in, 0, sizeof(slave_in)); memset(slave_out, 0, sizeof(slave_out));
    memcpy(slave_out, out, n);
    out_bits = in_bits = falls = rises = 0;
    /* Pattern in the bits SoftSpi must not touch, both CS lines included. */
    test_port8.P8DO = 0xA5A5A5C9U | SCK; test_port8.P8DI = 0xFFFFFFF7U;
    SoftSpiDeviceEnable(SOFT_SPI_DEVICE_FRAM);
    /* FeRAM CS (P82) is active low, RTC CS (P81) active high: both asserted levels low here. */
    assert(!(test_port8.P8DO & (1U << 2U)) && !(test_port8.P8DO & (1U << 1U)));
    assert(test_port8.P8DO & SCK); /* idles high */
    frame = test_port8.P8DO & ~SPI_PINS; check_frame = true;
}

static void finish(unsigned bytes)
{
    check_frame = false;
    assert(test_port8.P8DO & SCK);
    for (unsigned i = 0; i < bytes; ++i) assert(byte_falls[i] == 8U*(i+1U) && byte_rises[i] == 8U*(i+1U));
    assert(falls == 8U*bytes && rises == 8U*bytes);
    SoftSpiDeviceDisable();
    assert((test_port8.P8DO & (1U << 2U)) && !(test_port8.P8DO & (1U << 1U))); /* both deselected */
}

int main(void)
{
    static const uint8_t command[4] = {0x03, 0x01, 0x23, 0x45}, reply[3] = {0xC3, 0x5A, 0x01};
    uint8_t got[8];
    start((const uint8_t[1]){0}, 1);
    SoftSpiWrite(command, 4);
    assert(!memcmp(slave_in, command, 4)); finish(4);
    start(reply, 3);
    SoftSpiRead(got, 3, 0xFF);
    assert(!memcmp(got, reply, 3) && slave_in[0] == 0xFF && slave_in[2] == 0xFF); finish(3);
    start((const uint8_t[5]){0x00, 0x80, 0x7E, 0x81, 0xFF}, 5);
    SoftSpiDuplex(command+1, got, 2, 0x00, 3);
    assert(slave_in[0] == 0x01 && slave_in[1] == 0x23 && !slave_in[2] && !slave_in[4]);
    assert(got[0] == 0x00 && got[1] == 0x80 && got[2] == 0x7E && got[3] == 0x81 && got[4] == 0xFF); finish(5);
    start((const uint8_t[1]){0}, 1);
    check_frame = false;
    SoftSpiDeviceEnable(SOFT_SPI_DEVICE_RTC);
    assert((test_port8.P8DO & (1U << 1U)) && (test_port8.P8DO & (1U << 2U))); /* RTC selected, FeRAM not */
    test_port8.P8MOD0 = test_port8.P8MOD1 = 0xFFFFFFFFU;
    SoftSpiPeripheralInit();
    assert(test_port8.P8MOD0 == 0x05020201U && test_port8.P8MOD1 == 0xFFFF0202U && (test_port8.P8DO & SCK));
    puts("Soft SPI: mode 3 MSB-first write/read/duplex, 8 clocks per byte, SCK idles high, other P8DO bits and CS untouched PASS");
    return 0;
}
