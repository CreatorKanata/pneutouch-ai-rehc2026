/* HX710B acquisition and PNEU1 output; no AI or vibration-demo initialization. */
#include "pressure_app.h"
#include "board.h"
#include "acquisition.h"
#include "pneu_config.h"

volatile pneutouch_status_t pneutouch_status;
static acquisition_t acquisition;

static void text(const char *s) { while (*s) board_putc(*s++); }

/* Avoid printf, heap allocation and semihosting. */
static void number(uint32_t value)
{
    char digits[10];
    unsigned count = 0;
    do { digits[count++] = (char)('0' + value % 10U); value /= 10U; } while (value);
    while (count) board_putc(digits[--count]);
}

static void send_sample(const pressure_sample_t *sample)
{
    text("PNEU1,"); number(sample->sequence); board_putc(',');
    number(sample->milliseconds); board_putc(',');
    if (sample->raw < 0) {
        board_putc('-'); number((uint32_t)(-sample->raw));
    } else number((uint32_t)sample->raw);
    text("\r\n");
}

void pressure_app_init(void)
{
    const hx710b_t sensor = {0, board_dout_high, board_sensor_pulse, PNEU_HX_PULSES};
    pneutouch_status = (pneutouch_status_t){PNEU_STARTING, 0, 0, 0, 0};
    board_init();
    acquisition_init(&acquisition, sensor, board_millis());
    text("# PneutouchAi Phase0; PNEU1,seq,ms,raw; baud=");
    number(PNEU_UART_BAUD); text("; nominal_sps="); number(PNEU_SAMPLE_RATE_HZ);
    text("\r\n");
    pneutouch_status.state = PNEU_SETTLING;
}

void pressure_app_poll(void)
{
    pressure_sample_t sample;
    board_watchdog();
    acquisition_result_t result = acquisition_poll(&acquisition, board_millis(), &sample);
    if (result == ACQ_SAMPLE) {
        pneutouch_status.raw = sample.raw;
        pneutouch_status.milliseconds = sample.milliseconds;
        ++pneutouch_status.samples;
        pneutouch_status.state = PNEU_STREAMING;
        send_sample(&sample);
    } else if (result == ACQ_TIMEOUT) {
        ++pneutouch_status.timeouts;
        pneutouch_status.state = PNEU_SENSOR_TIMEOUT;
        text("# ERROR,HX710B_TIMEOUT\r\n");
    } else if (acquisition.mode_selected && acquisition.settling) {
        pneutouch_status.state = PNEU_SETTLING;
    }
}
