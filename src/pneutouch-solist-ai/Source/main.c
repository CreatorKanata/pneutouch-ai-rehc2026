/* Phase 0 only: acquire pressure counts and stream versioned CSV, without AI. */
#include "board.h"
#include "hx710b.h"
#include "acquisition.h"
#include "pneu_config.h"

static void text(const char *s) { while (*s) board_putc(*s++); }

/* Small integer formatter avoids printf, heap allocation and semihosting. */
static void number(uint32_t value)
{
    char digits[10];
    unsigned count = 0;
    do { digits[count++] = (char)('0' + value % 10U); value /= 10U; } while (value);
    while (count) board_putc(digits[--count]);
}

static void sample(uint32_t seq, uint32_t ms, int32_t raw)
{
    text("PNEU1,"); number(seq); board_putc(','); number(ms); board_putc(',');
    if (raw < 0) { board_putc('-'); number((uint32_t)(-raw)); }
    else number((uint32_t)raw);
    text("\r\n");
}

int main(void)
{
    const hx710b_t sensor = {0, board_dout_high, board_sensor_pulse, PNEU_HX_PULSES};
    acquisition_t state;
    board_init();
    acquisition_init(&state, sensor, board_millis());
    text("# PneuTouch Phase0; PNEU1,seq,ms,raw; baud=");
    number(PNEU_UART_BAUD); text("; nominal_sps="); number(PNEU_SAMPLE_RATE_HZ);
    text("\r\n");
    for (;;) {
        pressure_sample_t value;
        board_watchdog();
        acquisition_result_t result = acquisition_poll(&state, board_millis(), &value);
        if (result == ACQ_SAMPLE) {
            sample(value.sequence, value.milliseconds, value.raw);
        } else if (result == ACQ_TIMEOUT) {
            text("# ERROR,HX710B_TIMEOUT\r\n");
        }
    }
}
