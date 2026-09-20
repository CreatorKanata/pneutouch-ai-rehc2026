/* Application boundary: use the real reader/state machine with simulated GPIO. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "pressure_app.h"
#include "pneu_config.h"
#include "pressure_model.h"
#include "pressure_display.h"

static uint32_t now, bits;
static unsigned pulses, watchdogs;
static bool ready;
static char output[512];
static unsigned length;
static unsigned predictions, displays;
volatile pneu_model_status_t pneutouch_model;
volatile pneu_display_status_t pneutouch_display;
bool pneu_model_init(void) { pneutouch_model.ready = true; return true; }
pneu_demo_class_t pneu_model_predict(const float values[12], float positive_peak)
{
    assert(values[0] > 199 && values[0] < 201); /* Actual extracted rise time */
    assert(values[10] > .99f && values[10] < 1.01f);
    assert(positive_peak == 200000.0f);
    ++predictions; return PNEU_CLASS_BACK;
}
const char *pneu_class_name(pneu_demo_class_t label) { (void)label; return "BACK"; }
void pneu_display_init(void) {}
void pneu_display_poll(void) {}
void pneu_display_idle(const char *s) { (void)s; }
void pneu_display_result(pneu_demo_class_t label) { assert(label == PNEU_CLASS_BACK); ++displays; }
void board_init(void) {}
void pneu_ai_validation_init(void) {}
bool pneu_ai_validation_poll(void) { return false; }
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
    assert(strstr(output, "PneutouchAi Live; PNEU1,seq,ms,raw; baud=115200") != NULL);
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
    /* Actual simulated GPIO -> acquisition -> 12 features -> chip adapter -> LCD. */
    ready = true; bits = 4000000;
    poll_at(3000);
    for (unsigned i = 0; i < 125; ++i) {
        int u = (int)i-60, delta = 0;
        if (u >= 0) delta = u <= 10 ? 20000*u : u <= 20 ? 20000*(20-u) :
            u <= 30 ? -20000*(u-20) : u <= 40 ? -20000*(40-u) : 0;
        bits = (uint32_t)(4000000+delta);
        poll_at(4000+i*25);
    }
    assert(predictions == 1 && displays == 1);
    puts("Pressure app: UART framing, signed limits, debug status and no fake samples PASS");
    return 0;
}
