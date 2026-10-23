/* Host stand-in for the ML63Q25x7 register map: only the Port 8 block SoftSpi.c uses.
   The test that includes SoftSpi.c defines test_port8 and the register access macros. */
#ifndef TEST_MCU_H
#define TEST_MCU_H
#include <stdint.h>
typedef struct { uint32_t P8DI, P8DO, P8MOD0, P8MOD1; } test_port8_t;
extern test_port8_t test_port8;
#define PORT8 (&test_port8)
#endif
