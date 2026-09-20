/* Application boundary: use the real reader/state machine with simulated GPIO. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "pressure_app.h"
#include "pneu_config.h"

static uint32_t now, bits;
static unsigned pulses, watchdogs;
static bool ready;
static char output[512];
static unsigned length;
void board_init(void) {}
uint32_t board_millis(void) { return now; }
void board_watchdog(void) { ++watchdogs; }
bool board_dout_high(void *context)
{
    (void)context;
    return !ready || pulses >= 25;
}
bool board_sensor_pulse(void *context)
{
    (void)context;
    unsigned index = pulses++;
    return index < 24 ? (bits & (1U << (23U - index))) != 0 : true;
}
void board_putc(char value)
{
    assert(length + 1 < sizeof(output));
    output[length++] = value;
    output[length] = '\0';
}
static void poll_at(uint32_t milliseconds)
{
    now = milliseconds;
    pulses = 0;
    length = 0;
    output[0] = '\0';
    pressure_app_poll();
}
int main(void)
{
    pressure_app_init();
    assert(strstr(output, "PneutouchAi Phase0; PNEU1,seq,ms,raw; baud=115200") != NULL);
    assert(pneutouch_status.state == PNEU_SETTLING);
    poll_at(PNEU_SENSOR_TIMEOUT_MS);
    assert(strcmp(output, "# ERROR,HX710B_TIMEOUT\r\n") == 0);
    assert(pneutouch_status.samples == 0 && pneutouch_status.timeouts == 1);
    assert(pneutouch_status.state == PNEU_SENSOR_TIMEOUT);
    ready = true;
    bits = 0x800000;
    poll_at(1100);
    assert(length == 0 && pneutouch_status.state == PNEU_SETTLING);
    poll_at(1100 + PNEU_SETTLE_MS - 1);
    assert(length == 0);
    poll_at(1100 + PNEU_SETTLE_MS);
    char expected[80];
    snprintf(expected, sizeof(expected), "PNEU1,0,%lu,-8388608\r\n",
             (unsigned long)(1100 + PNEU_SETTLE_MS));
    assert(strcmp(output, expected) == 0);
    assert(pneutouch_status.raw == -8388608 && pneutouch_status.samples == 1);
    assert(pneutouch_status.state == PNEU_STREAMING);
    bits = 0x7FFFFF;
    poll_at(1125 + PNEU_SETTLE_MS);
    snprintf(expected, sizeof(expected), "PNEU1,1,%lu,8388607\r\n",
             (unsigned long)(1125 + PNEU_SETTLE_MS));
    assert(strcmp(output, expected) == 0);
    ready = false;
    poll_at(1125 + PNEU_SETTLE_MS + PNEU_SENSOR_TIMEOUT_MS);
    assert(pneutouch_status.state == PNEU_SENSOR_TIMEOUT);
    assert(pneutouch_status.samples == 2 && pneutouch_status.raw == 8388607);
    assert(watchdogs == 6);
    puts("Pressure app: UART framing, signed limits, debug status and no fake samples PASS");
    return 0;
}
