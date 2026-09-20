/* Board adapter, based on the supplied ROHM clock/UART and board pin tables.
   CN3-12=P40=SCK; CN3-10=P42=DOUT; CN9 FT2232H channel B=UARTF1. */
#include "board.h"
#include "pneu_config.h"
#include "smpl_common.h"
#include "uartf1.h"
#include "wdt.h"
#include "irq.h"
#include "Lcd.h"
#include "LcdI2cf0.h"
#include "Regulator5VOutput.h"

static volatile uint32_t milliseconds;

void SysTick_Handler(void) { ++milliseconds; }
uint32_t board_millis(void) { return milliseconds; }
uint32_t board_micros(void)
{
    uint32_t before, after, ticks;
    do { before = milliseconds; ticks = SysTick->VAL; after = milliseconds; } while (before != after);
    return before*1000U+(SysTick->LOAD-ticks)/(PNEU_CPU_HZ/1000000U);
}
int board_getc(void) { return uartf1_checkReadReady() ? (int)(uartf1_getc() & 255U) : -1; }
void board_watchdog(void) { wdt_clear(); }

/* SysTick runs from the 48 MHz core even while its interrupt is masked.
   Calls are only a few us, shorter than the 1 ms reload period. */
static void delay_us(uint32_t us)
{
    uint32_t previous = SysTick->VAL;
    uint32_t elapsed = 0;
    const uint32_t period = SysTick->LOAD + 1U;
    const uint32_t target = (PNEU_CPU_HZ / 1000000U) * us;
    while (elapsed < target) {
        uint32_t now = SysTick->VAL;
        elapsed += previous >= now ? previous - now : previous + period - now;
        previous = now;
    }
}

void board_init(void)
{
    __disable_irq();
    /* Hold board power before waiting for the PLL. */
    set_bit(PORT4->P4DO, 1U << 5);
    write_bit(PORT4->P4MOD1, 0xFFU << 8, 0x02U << 8);
    wdt_init(WDT_2S);
    wdt_clear();
    smpl_setLsCrystal32Khz();
    smpl_setHsPll48Mhz(CLK_XSPEN_DIS, CLK_HXSPEN_DIS);
    SystemCoreClock = PNEU_CPU_HZ;

    /* Set output latch low before enabling SCK. P42 is a digital input.
       CN3 already has a board pull-up on P42: unplugged sensor stays busy. */
    clear_bit(PORT4->P4DO, 1U);
    write_bit(PORT4->P4MOD0, 0x00FF00FFU, 0x00010002U);
    /* Release EXT_RESET_B (P72, LCD reset); also select RXDF1/TXDF1. */
    set_bit(PORT7->P7DO, 1U << 2);
    write_bit(PORT7->P7MOD0, 0x00FFFFFFU, 0x00022221U);
    smpl_enablePeripheral(UAF1_PERI);
    write_reg32(UARTF1->UAF0MOD, UARTF_DLAB_DLR);
    write_reg32(UARTF1->UAF0BUF, PNEU_UART_DIVISOR);
    write_reg32(UARTF1->UAF0MOD, UARTF_LG_8BIT | UARTF_STP_1BIT | UARTF_PT_NON);
    write_reg32(UARTF1->UAF0CAJ, UARTF_RMV_ENA | PNEU_UART_ADJUST);
    write_reg32(UARTF1->UAF0IER, 0U); /* Poll TX, no UART interrupt needed. */
    (void)uartf1_getStatus();
    (void)uartf1_getIntCause();
    if (SysTick_Config(PNEU_CPU_HZ / 1000U) != 0U) {
        for (;;) {} /* WDT resets if the configuration is invalid. */
    }
    __enable_irq();
}

bool board_dout_high(void *context)
{
    (void)context;
    return get_bit(PORT4->P4DI, 1U << 2);
}

bool board_sensor_pulse(void *context)
{
    bool bit;
    uint32_t mask;
    (void)context;
    delay_us(PNEU_CLOCK_HALF_US); /* Also satisfies DOUT-ready setup time. */
    mask = __get_PRIMASK();
    __disable_irq();
    set_bit(PORT4->P4DO, 1U);
    delay_us(PNEU_CLOCK_HALF_US);
    bit = board_dout_high(0);
    clear_bit(PORT4->P4DO, 1U);
    __set_PRIMASK(mask); /* Restore caller state only after lowering SCK. */
    return bit;
}

void board_putc(char value)
{
    /* A peripheral fault deliberately lets the watchdog reset the MCU. */
    while (uartf1_checkWriteBusy()) {}
    uartf1_putc((uint8_t)value);
}

static volatile int lcd_result;
static uint32_t lcd_started;
static uint16_t lcd_expected;
static void lcd_complete(uint32_t size, uint8_t error)
{
    lcd_result = !error && size == lcd_expected ? 1 : -1;
}
void board_lcd_init(void)
{
    Regulator5VOutputInit(); Regulator5VOutputOn();
    LcdPeripheralInit(); LcdBacklightOn();
    lcd_result = 1;
}
void board_lcd_start(uint8_t *buffer, uint16_t size)
{
    lcd_result = 0; lcd_expected = size; lcd_started = board_millis();
    if (LcdI2cf0Write(0x7c, buffer, size, lcd_complete) != I2F_R_OK) lcd_result = -1;
}
int board_lcd_status(void)
{
    if (!lcd_result && board_millis()-lcd_started >= 10U) {
        irq_i2cf0_dis();
        /* Stop and disable the interface before relinquishing the TX buffer. */
        clear_bit(I2CF0->I2F0CTL, (1U << 5) | (1U << 7));
        irq_i2cf0_clearIRQ();
        lcd_result = -1;
    }
    return lcd_result;
}
