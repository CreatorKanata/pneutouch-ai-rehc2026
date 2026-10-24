/* DT-EBML63Q2557 hardware boundary: clock, watchdog, CN3 GPIO, CN9 UART, FeRAM. */
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
void board_lcd_init(void);
/* Buffer must remain valid until status is nonzero. 0=busy, 1=ACK, -1=fault. */
void board_lcd_start(uint8_t *buffer, uint16_t size);
int board_lcd_status(void);
/* MB85RS2MTA FeRAM on the software SPI (byte addresses, 256 KiB). */
void board_store_init(void);
void board_store_read(uint32_t addr, void *dst, unsigned n);
void board_store_write(uint32_t addr, const void *src, unsigned n);
int board_store_selftest(void); /* 0 = pass; FramTest-style codes otherwise */
/* Push switches SW2..SW5 = P50..P53 (board pull-ups, pressed = low). */
void board_buttons_init(void);
uint8_t board_buttons(void); /* pressed mask: bit 0 = SW2 ... bit 3 = SW5 */
#endif
