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
static char output[1024];
static unsigned length;
static unsigned predictions, displays;
volatile pneu_model_status_t pneutouch_model;
volatile pneu_display_status_t pneutouch_display;
static pneu_model_progress_t progress;
static bool model_fails, board_ready, saw_learn;
static uint8_t model_advice;
static unsigned store_inits;
static char idle_text[17], line2_text[17];
void pneu_model_set_progress(pneu_model_progress_t callback) { progress = callback; }
bool pneu_model_init(void)
{
    static const char selected[17] = "\xff   \xff     \xff    3";
    assert(store_inits == 1 && progress); /* FeRAM ready and progress hook registered first */
    pneutouch_model = (pneu_model_status_t){0};
    pneutouch_model.store_count = 8; pneutouch_model.fisher[0] = 45.0f; pneutouch_model.fisher[4] = 12.75f;
    if (model_fails) { pneutouch_model.error = PNEU_MODEL_ERR_NO_SLOT; memset((char *)pneutouch_model.lcd_line2, ' ', 16); return false; }
    pneutouch_model.selected_count = 3;
    pneutouch_model.selected[1] = 4; pneutouch_model.selected[2] = 10;
    for (unsigned i = 0; i < 17; ++i) pneutouch_model.lcd_line2[i] = selected[i];
    pneutouch_model.training_examples = 176; pneutouch_model.training_steps = 704;
    pneutouch_model.tuned_k = model_advice ? 5 : 0; pneutouch_model.advice = model_advice;
    for (unsigned c = 0; c < 4; ++c) {
        pneutouch_model.class_count[c] = (uint16_t)(37U+c);
        pneutouch_model.recall[c] = model_advice ? (uint8_t)(c == 3 ? 78 : 100) : 0;
    }
    progress(1, 8, 704);
    assert(!strcmp(idle_text, "LEARN1    8/ 704"));
    pneutouch_model.ready = true;
    return true;
}
pneu_demo_class_t pneu_model_predict(const float values[12], float positive_peak, float trough)
{
    assert(values[0] > 199 && values[0] < 201); /* Actual extracted rise time */
    assert(values[10] > .99f && values[10] < 1.01f);
    assert(positive_peak == 200000.0f);
    assert(trough > 0.0f);
    assert(trough == 200000.0f); /* Magnitude of the -200000 triangular release. */
    ++predictions; return PNEU_CLASS_BACK;
}
const char *pneu_class_name(pneu_demo_class_t label) { (void)label; return "BACK"; }
void pneu_display_init(void) {}
void pneu_display_poll(void) {}
void pneu_display_idle(const char *s)
{
    assert(strlen(s) <= 16); strcpy(idle_text, s);
    if (!strncmp(s, "LEARN", 5)) saw_learn = true;
}
void pneu_display_line2(const char *s) { assert(strlen(s) == 16); strcpy(line2_text, s); }
void pneu_display_result(pneu_demo_class_t label) { assert(label == PNEU_CLASS_BACK); ++displays; }
void board_init(void) { board_ready = true; }
void board_store_init(void) { assert(board_ready); ++store_inits; }
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
static void test_tune_progress(void)
{
    char ready[17];
    progress(2, 0, 20); assert(!strcmp(idle_text, "TUNE k4 r1/4    "));
    progress(0, 0, 132); assert(!strcmp(idle_text, "TUNE k4 r1/4  0%"));
    progress(2, 7, 20); assert(!strcmp(idle_text, "TUNE k5 r4/4    "));
    progress(0, 66, 132); assert(!strcmp(idle_text, "TUNE k5 r4/4 50%"));
    progress(1, 304, 528); assert(!strcmp(idle_text, "TUNE k5 r4/4 57%") && strlen(idle_text) == 16);
    progress(1, 528, 528); assert(!strcmp(idle_text, "TUNE k5 r4/4100%"));
    progress(2, 19, 20); assert(!strcmp(idle_text, "TUNE k8 r4/4    "));
    progress(2, 20, 20); assert(!strcmp(idle_text, "TUNE k8 r4/4    ") && strlen(idle_text) == 16);
    progress(1, 8, 704); assert(!strcmp(idle_text, "LEARN1    8/ 704")); /* tuning finished */
    progress(2, 3, 20); progress(1, 8, 704); assert(!strncmp(idle_text, "TUNE ", 5));
    pneu_ready_text(0, ready); assert(!strcmp(ready, "READY"));
    pneu_ready_text(1, ready); assert(!strcmp(ready, "READY MORE TAIL"));
    pneu_ready_text(4, ready); assert(!strcmp(ready, "READY MORE HEAD"));
    pneu_ready_text(5, ready); assert(!strcmp(ready, "READY"));
}

int main(void)
{
    pressure_app_init();
    assert(strstr(output, "PneutouchAi Live; PNEU1,seq,ms,raw; baud=115200") != NULL);
    assert(strstr(output, "; fw=na4-margin\r\n") != NULL);
    assert(saw_learn && !strcmp(idle_text, "READY"));
    assert(!strcmp(line2_text, "\xff   \xff     \xff    3"));
    assert(strstr(output, "# PNEM1,READY,176,704,"));
    assert(strstr(output, "# PNEM2,NONE,0,0,single_instance\r\n"));
    assert(strstr(output, "# PNEM3,READY,slot=0,count=8,err=0,sel=0:4:10,fisher=45000:0:0:0:12750:0:0:0:0:0:0:0:0:0\r\n"));
    assert(strstr(output, "\r\n# PNEM4,k=0,recall=0:0:0:0,counts=37:38:39:40,advice=NONE\r\n"));
    watchdogs = 0; /* the progress callback fed it once during init; count polls only */
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
    /* A board without a loaded slot boots to NO MODEL with a blank line 2. */
    model_fails = true; store_inits = 0; saw_learn = false; length = 0;
    pressure_app_init();
    assert(!strcmp(idle_text, "NO MODEL") && !saw_learn && !strcmp(line2_text, "                "));
    assert(strstr(output, "# PNEM3,ERROR,slot=0,count=8,err=2,sel=,fisher=45000:"));
    assert(strstr(output, "# PNEM4,k=0,recall=0:0:0:0,counts=0:0:0:0,advice=NONE\r\n"));
    /* A tuned slot advising HEAD; a stale TUNE state (failed tune) is cleared by the boot. */
    test_tune_progress();
    model_fails = false; model_advice = 4; store_inits = 0; length = 0;
    pressure_app_init();
    assert(!strcmp(idle_text, "READY MORE HEAD"));
    assert(strstr(output, "# PNEM4,k=5,recall=100:100:100:78,counts=37:38:39:40,advice=HEAD\r\n"));
    puts("Pressure app: UART framing, signed limits, debug status, no fake samples, FeRAM boot banner and LCD PASS");
    return 0;
}
