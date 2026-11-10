/* In-memory MB85RS2MTA for host tests. Include from exactly one file per test. */
#ifndef TEST_FRAM_STUB_H
#define TEST_FRAM_STUB_H
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "board.h"
#define FRAM_SIZE 0x40000U
static uint8_t fram[FRAM_SIZE];
static bool fram_drop_writes; /* simulates a missing chip / broken MOSI */
static int fram_selftest_result;
static unsigned fram_inits;
void board_store_init(void) { ++fram_inits; }
void board_store_read(uint32_t addr, void *dst, unsigned n)
{
    assert(addr+n <= FRAM_SIZE); memcpy(dst, fram+addr, n);
}
void board_store_write(uint32_t addr, const void *src, unsigned n)
{
    assert(addr >= 0x1000U && addr+n <= FRAM_SIZE); /* 0x0000-0x0FFF is legacy config */
    if (!fram_drop_writes) memcpy(fram+addr, src, n);
}
int board_store_selftest(void) { return fram_selftest_result; }
#endif
