/* Application boundary: use the real reader/state machine with simulated GPIO. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "pressure_app.h"
#include "pneu_config.h"
#include "pressure_model.h"
#include "pressure_display.h"
#include "model_store.h"
#include "model_tune.h"
#include "app_buttons.h"
#include "object_profile.h"

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
static char idle_text[17], line2_text[17], idle_at_init[17];
static unsigned model_inits, model_tunes;
static bool validating, use_fails;
static uint8_t active_slot, used_slot = 255;
volatile pneu_tune_status_t pneutouch_tune;
void pneu_model_set_progress(pneu_model_progress_t callback) { progress = callback; }
bool pneu_model_init(void)
{
    static const char selected[17] = "\xff   \xff     \xff   k3";
    assert(store_inits == 1 && progress); /* FeRAM ready and progress hook registered first */
    ++model_inits; strcpy(idle_at_init, idle_text);
    pneu_store_slot_t info;
    pneutouch_model = (pneu_model_status_t){0};
    pneutouch_model.slot = active_slot;
    (void)pneu_store_slot_info(active_slot, &info);
    pneutouch_model.object = (uint8_t)pneu_object_of_name(info.name);
    pneutouch_model.store_count = 8; pneutouch_model.fisher[0] = 45.0f; pneutouch_model.fisher[4] = 12.75f;
    if (model_fails) { pneutouch_model.error = PNEU_MODEL_ERR_NO_SLOT; memset((char *)pneutouch_model.lcd_line2, ' ', 16); return false; }
    pneutouch_model.selected_count = 3;
    pneutouch_model.selected[1] = 4; pneutouch_model.selected[2] = 10;
    for (unsigned i = 0; i < 17; ++i) pneutouch_model.lcd_line2[i] = selected[i];
    pneutouch_model.training_examples = 176; pneutouch_model.training_steps = 704;
    pneutouch_model.tuned_k = model_advice ? 3 : 0; pneutouch_model.advice = model_advice;
    pneutouch_model.mask = 0x0411; /* features 0, 4, 10 */
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
static uint8_t button_mask;
void board_init(void) { board_ready = true; }
void board_buttons_init(void) {}
uint8_t board_buttons(void) { return button_mask; }
void board_store_init(void) { assert(board_ready); ++store_inits; }
void pneu_ai_validation_init(void) {}
bool pneu_ai_validation_poll(void) { return validating; }
bool pneu_model_tune(void)
{
    ++model_tunes;
    progress(2, 0, 198); assert(!strcmp(idle_text, "TUNE  0/198     "));
    pneutouch_tune = (pneu_tune_status_t){0};
    pneutouch_tune.done = true; pneutouch_tune.count = 8; pneutouch_tune.mask = 0x0015;
    pneutouch_tune.best_k = 3; pneutouch_tune.correct = 7; pneutouch_tune.champion_mask = 0x2411;
    pneutouch_tune.champion_correct = 5; pneutouch_tune.finalist_mask[0] = 0x0015;
    pneutouch_tune.finalist_screen[0] = 16; pneutouch_tune.finalist_correct[0] = 7;
    pneutouch_tune.candidates = 99; pneutouch_tune.screened = 8;
    for (unsigned t = 0; t < 4; ++t) { pneutouch_tune.confusion[t][t] = 2; pneutouch_tune.recall[t] = 100; }
    pneutouch_tune.elapsed_us = 61000000U;
    return true;
}
/* Store: only slot 1 is committed (141 records, a KRAG slot); PST1 commands are not exercised here. */
uint8_t pneu_store_active(void) { return active_slot; }
bool pneu_store_use(uint8_t slot) { if (use_fails) return false; used_slot = active_slot = slot; return true; }
bool pneu_store_slot_info(uint8_t slot, pneu_store_slot_t *info)
{
    *info = (pneu_store_slot_t){0};
    info->valid = true;
    if (slot == 1) { info->state = PNEU_SLOT_COMMITTED; info->count = 141; strcpy(info->name, "KRAG1024"); }
    return true;
}
bool pneu_store_open(uint8_t slot, uint8_t k, const char *name) { (void)slot; (void)k; (void)name; return false; }
bool pneu_store_put(uint8_t slot, uint8_t label, const float pool[PNEU_POOL_COUNT]) { (void)slot; (void)label; (void)pool; return false; }
bool pneu_store_commit(uint8_t slot) { (void)slot; return false; }
bool pneu_store_erase(uint8_t slot) { (void)slot; return false; }
bool pneu_store_init(void) { return true; }
bool pneu_store_reopen(uint8_t slot) { (void)slot; return false; }
bool pneu_store_undo(uint8_t slot) { (void)slot; return false; }
bool pneu_store_truncate(uint8_t slot, uint16_t count) { (void)slot; (void)count; return false; }
bool pneu_model_pool(const float values[12], float peak, float trough, float pool[14])
{
    (void)values; (void)peak; (void)trough; (void)pool; return false;
}
bool pneu_store_get(uint8_t slot, uint16_t index, uint8_t *label, float pool[PNEU_POOL_COUNT])
{
    (void)slot; (void)index; (void)label; (void)pool; return false;
}
int board_store_selftest(void) { return 0; }
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
    progress(2, 0, 198); assert(!strcmp(idle_text, "TUNE  0/198     "));
    progress(0, 0, 176); assert(!strcmp(idle_text, "TUNE  0/198   0%"));
    progress(1, 91, 160); assert(!strcmp(idle_text, "TUNE  0/198  56%") && strlen(idle_text) == 16);
    progress(2, 37, 198); assert(!strcmp(idle_text, "TUNE 37/198     "));
    progress(1, 160, 160); assert(!strcmp(idle_text, "TUNE 37/198 100%"));
    progress(2, 198, 198); assert(!strcmp(idle_text, "TUNE198/198     ")); /* finals follow */
    progress(3, 0, 6); assert(!strcmp(idle_text, "FINAL 0/6       "));
    progress(1, 304, 528); assert(!strcmp(idle_text, "FINAL 0/6    57%"));
    progress(3, 3, 6); progress(0, 66, 132); assert(!strcmp(idle_text, "FINAL 3/6    50%"));
    progress(3, 10, 12); assert(!strcmp(idle_text, "FINAL 10/12     "));
    progress(1, 1, 2); assert(!strcmp(idle_text, "FINAL 10/12  50%"));
    progress(3, 6, 6); assert(!strcmp(idle_text, "FINAL 6/6       ") && strlen(idle_text) == 16);
    progress(1, 8, 704); assert(!strcmp(idle_text, "LEARN1    8/ 704")); /* tuning finished */
    progress(2, 3, 198); progress(1, 8, 704); assert(!strncmp(idle_text, "TUNE ", 5));
}

/* Polls every 10 ms over [from, to) with the given button mask. */
static void hold_buttons(uint8_t mask, uint32_t from, uint32_t to)
{
    button_mask = mask;
    for (uint32_t t = from; t < to; t += 10) poll_at(t);
}

static void test_sw2(uint32_t t)
{
    unsigned inits = model_inits;
    hold_buttons(1, t, t+10); hold_buttons(0, t+10, t+100); /* 10 ms glitch: no press */
    assert(!strcmp(idle_text, "READY DINO      ") && model_tunes == 0);
    hold_buttons(1, t+100, t+600); /* accepted 30 ms after the edge */
    assert(!strcmp(idle_text, "HOLD 2s: TUNE   "));
    hold_buttons(0, t+600, t+700);
    assert(!strcmp(idle_text, "READY DINO      ") && model_tunes == 0 && model_inits == inits);
    t += 1000;
    hold_buttons(1, t, t+2000);
    assert(!strcmp(idle_text, "HOLD 2s: TUNE   ") && model_tunes == 0);
    poll_at(t+2000); /* 2000 ms after press-down */
    assert(model_tunes == 1 && model_inits == inits+1);
    assert(strstr(output, "# PNET1,START\r\n# PNET1,1,0,8,0015,3,7,2411,5,0015=16=7,99,8,100:100:100:100,"
                          "2:2:2:2,2:0:0:0:0:2:0:0:0:0:2:0:0:0:0:2,0,61000000\r\n"));
    assert(strstr(output, "# PNEM3,READY,slot=0,count=8,err=0,object=DINO,sel=") &&
           strstr(output, "# PneutouchAi live acquisition resumed; sequence restarts\r\n"));
    assert(!strcmp(idle_text, "S0 DINO    8  k3"));
    hold_buttons(1, t+2010, t+3000); hold_buttons(0, t+3000, t+3990); /* fired once, release is silent */
    assert(model_tunes == 1 && !strcmp(idle_text, "S0 DINO    8  k3"));
    poll_at(t+4000); assert(!strcmp(idle_text, "READY DINO      "));
}

static void test_sw3(uint32_t t)
{
    unsigned inits = model_inits;
    hold_buttons(2, t, t+200); hold_buttons(0, t+200, t+230);
    assert(used_slot == 255);
    poll_at(t+230); /* released 200 ms after press-down */
    assert(used_slot == 1 && model_inits == inits+1 && !strcmp(idle_at_init, "S1 KRAG  141    "));
    assert(strstr(output, "# PNES1,slot=1,count=141\r\n") && strstr(output, "resumed"));
    assert(strstr(output, "# PNEM3,READY,slot=1,count=8,err=0,object=KRAG,sel=0:4:10,"));
    assert(strstr(output, ",advice=NONE\r\n") && !strcmp(idle_text, "S1 KRAG    8  k3"));
    assert(strstr(output, "# PNEM5,detect=7000:5000:6000:6000,object=KRAG\r\n"));
    poll_at(t+2230); assert(!strcmp(idle_text, "READY KRAG      "));
    t += 3000;
    hold_buttons(2, t, t+200); hold_buttons(0, t+200, t+230); poll_at(t+230);
    assert(model_inits == inits+1 && !strcmp(idle_text, "SLOT 1 ONLY     "));
    assert(strstr(output, "# PNES1,ERROR,NO_OTHER_SLOT\r\n"));
    poll_at(t+2229); assert(!strcmp(idle_text, "SLOT 1 ONLY     "));
    poll_at(t+2230); assert(!strcmp(idle_text, "READY KRAG      "));
    t += 3000;
    hold_buttons(2, t, t+1200); hold_buttons(0, t+1200, t+1300); /* long press: nothing */
    assert(!strcmp(idle_text, "READY KRAG      ") && model_inits == inits+1);
    t += 2000; active_slot = 0; use_fails = true;
    hold_buttons(2, t, t+100); hold_buttons(0, t+100, t+130); poll_at(t+130);
    assert(!strcmp(idle_text, "STORE ERROR") && strstr(output, "# PNES1,ERROR,STORE\r\n"));
    assert(model_inits == inits+1 && active_slot == 0);
    poll_at(t+2130); assert(!strcmp(idle_text, "READY KRAG      ")); /* still the slot 1 model */
    active_slot = 1; use_fails = false;
}

static void test_validation_buttons(uint32_t t)
{
    unsigned inits = model_inits;
    validating = true;
    hold_buttons(1, t, t+3000);
    assert(!strcmp(idle_text, "VALIDATION") && model_tunes == 1);
    validating = false; /* MODE 0 with SW2 still held: retrain, no TUNE */
    hold_buttons(1, t+3000, t+6000);
    assert(model_inits == inits+1 && model_tunes == 1 && !strcmp(idle_text, "READY KRAG      "));
    hold_buttons(0, t+6000, t+6100);
    assert(!strcmp(idle_text, "READY KRAG      ") && model_tunes == 1);
}

static void test_buttons(void)
{
    char notice[17];
    model_advice = 0; active_slot = 0; store_inits = 0;
    pressure_app_init(); ready = false;
    poll_at(now+2000); assert(!strcmp(idle_text, "READY DINO      "));
    test_sw2(now+100);
    test_sw3(now+100);
    test_validation_buttons(now+100);
    store_inits = 0; length = 0; pressure_app_init(); /* boot with the KRAG slot 1 active */
    assert(strstr(output, "object=KRAG,sel=") && strstr(output, "# PNEM5,detect=7000:5000:6000:6000,object=KRAG\r\n"));
    app_slot_notice(notice, 2, PNEU_OBJECT_DINO, 176, 4); assert(!strcmp(notice, "S2 DINO  176  k4"));
    app_slot_notice(notice, 2, PNEU_OBJECT_KRAG, 176, 4); assert(!strcmp(notice, "S2 KRAG  176  k4"));
    app_slot_notice(notice, 3, PNEU_OBJECT_KRAG, 1000, 12); assert(!strcmp(notice, "S3 KRAG 1000  12"));
    app_slot_notice(notice, 0, PNEU_OBJECT_DINO, 4, 2); assert(!strcmp(notice, "S0 DINO    4  k2"));
    for (unsigned i = 0; i < 16; ++i) assert(notice[i] == "S0 DINO    4  k2"[i]); /* columns 0..15 */
    assert(strlen(notice) == 16);
}

int main(void)
{
    pressure_app_init();
    assert(strstr(output, "PneutouchAi Live; PNEU1,seq,ms,raw; baud=115200") != NULL);
    assert(strstr(output, "; fw=ss6-thresh\r\n") != NULL);
    assert(saw_learn && !strcmp(idle_text, "S0 DINO    8  k3")); /* 2 s notice, then READY */
    assert(!strcmp(line2_text, "\xff   \xff     \xff   k3"));
    assert(strstr(output, "# PNEM1,READY,176,704,"));
    assert(strstr(output, "# PNEM2,NONE,0,0,single_instance\r\n"));
    assert(strstr(output, "; gesture=press_release_1s; objects=dino:tail/back/legs/head,krag:hat/face\r\n"));
    assert(strstr(output, "# PNEM3,READY,slot=0,count=8,err=0,object=DINO,sel=0:4:10,"
                          "fisher=45000:0:0:0:12750:0:0:0:0:0:0:0:0:0\r\n"));
    assert(strstr(output, "\r\n# PNEM4,k=0,mask=0411,recall=0:0:0:0,counts=37:38:39:40,advice=NONE\r\n"
                          "# PNEM5,detect=30000:25000:25000:30000,object=DINO\r\n"));
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
    assert(length == 0 && !strcmp(idle_text, "S0 DINO    8  k3"));
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
    assert(!strcmp(idle_text, "READY DINO      ")); /* the notice expired at 2000 ms */
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
    assert(strstr(output, "# PNEM3,ERROR,slot=0,count=8,err=2,object=DINO,sel=,fisher=45000:"));
    assert(strstr(output, "# PNEM4,k=0,mask=0000,recall=0:0:0:0,counts=0:0:0:0,advice=NONE\r\n"));
    assert(strstr(output, "# PNEM5,detect=30000:25000:25000:30000,object=DINO\r\n")); /* failed init: DINO */
    /* A tuned slot advising HEAD; a stale TUNE state (failed tune) is cleared by the boot. */
    test_tune_progress();
    model_fails = false; model_advice = 4; store_inits = 0; length = 0;
    pressure_app_init();
    assert(strstr(output, "# PNEM4,k=3,mask=0411,recall=100:100:100:78,counts=37:38:39:40,advice=HEAD\r\n"));
    poll_at(now+1999); assert(!strcmp(idle_text, "S0 DINO    8  k3"));
    poll_at(now+1); assert(!strcmp(idle_text, "READY DINO      ")); /* advice is UART-only (PNEM4) */
    test_buttons();
    puts("Pressure app: UART framing, signed limits, debug status, no fake samples, FeRAM boot banner and LCD PASS");
    return 0;
}
