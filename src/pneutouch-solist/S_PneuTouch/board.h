/* DT-EBML63Q2557 hardware boundary: clock, watchdog, CN3 GPIO, CN9 UART. */
#ifndef PNEU_BOARD_H
#define PNEU_BOARD_H
#include <stdbool.h>
#include <stdint.h>
void board_init(void);
uint32_t board_millis(void);
uint32_t board_micros(void);
int board_getc(void); /* -1 when no UART byte is available */
void board_watchdog(void);
bool board_dout_high(void *context);
bool board_sensor_pulse(void *context);
void board_putc(char value);
#endif
