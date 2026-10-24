/* SW4/SW5 recording without a PC: real app_buttons/app_record/pressure_app and the
   real model_store.c on the in-memory FeRAM; the AI and the LCD are stubbed. */
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
#include "app_record.h"
#include "app_chooser.h"
#include "object_profile.h"
#include "fram_stub.h"

static uint32_t now, bits;
static unsigned pulses, predictions, displays, model_inits, model_tunes;
static bool ready, pool_fails, validating;
static uint8_t button_mask;
static char log_text[32768], idle_text[17], saved_text[17];
static unsigned log_length;
static pneu_model_progress_t progress;
volatile pneu_model_status_t pneutouch_model;
volatile pneu_display_status_t pneutouch_display;
volatile pneu_tune_status_t pneutouch_tune;

void pneu_model_set_progress(pneu_model_progress_t callback) { progress = callback; }
/* Like the real init: trains only from a COMMITTED active slot with >= 4 records. */
bool pneu_model_init(void)
{
    pneu_store_slot_t info;
    assert(progress && pneu_store_init());
    ++model_inits;
    pneutouch_model = (pneu_model_status_t){0};
    pneutouch_model.slot = pneu_store_active();
    memset((char *)pneutouch_model.lcd_line2, ' ', 16); /* blank, as after a failure */
    bool valid = pneu_store_slot_info(pneutouch_model.slot, &info);
    pneutouch_model.object = (uint8_t)pneu_object_of_name(info.name);
    if (!valid || info.state != PNEU_SLOT_COMMITTED || info.count < 4) {
        pneutouch_model.error = PNEU_MODEL_ERR_NO_SLOT;
        return false;
    }
    progress(1, 0, (uint16_t)(info.count*4U));
    pneutouch_model.store_count = info.count; pneutouch_model.selected_count = 3;
    pneutouch_model.ready = true;
    return true;
}
bool pneu_model_tune(void) { ++model_tunes; return true; }
pneu_demo_class_t pneu_model_predict(const float values[12], float peak, float trough)
{
    (void)values; (void)peak; (void)trough;
    ++predictions; return PNEU_CLASS_BACK;
}
bool pneu_model_pool(const float values[12], float peak, float trough, float pool[14])
{
    memcpy(pool, values, 12*sizeof(float)); pool[12] = peak; pool[13] = trough;
    return !pool_fails;
}
const char *pneu_class_name(pneu_demo_class_t label)
{
    return pneu_object_class_name((pneu_object_t)pneutouch_model.object, (uint8_t)label);
}
void pneu_display_init(void) {}
void pneu_display_poll(void) {}
void pneu_display_idle(const char *s)
{
    assert(strlen(s) <= 16); strcpy(idle_text, s);
    if (s[0] == '+' || !strncmp(s, "BAD", 3) || !strncmp(s, "STORE", 5)) strcpy(saved_text, s);
}
void pneu_display_line2(const char *s) { assert(strlen(s) == 16); }
void pneu_display_result(pneu_demo_class_t label) { (void)label; ++displays; }
void board_init(void) {}
void board_buttons_init(void) {}
uint8_t board_buttons(void) { return button_mask; }
void pneu_ai_validation_init(void) {}
bool pneu_ai_validation_poll(void) { return validating; }
uint32_t board_millis(void) { return now; }
void board_watchdog(void) {}
bool board_dout_high(void *context) { (void)context; return !ready || pulses >= 25; }
bool board_sensor_pulse(void *context)
{
    unsigned index = pulses++;
    (void)context;
    return index < 24 ? (bits & (1U << (23U-index))) != 0 : true;
}
void board_putc(char value)
{
    assert(log_length+1 < sizeof(log_text));
    log_text[log_length++] = value; log_text[log_length] = '\0';
}
static void clear_log(void) { log_length = 0; log_text[0] = '\0'; }
static bool logged(const char *line) { return strstr(log_text, line) != NULL; }

static void poll_at(uint32_t milliseconds) { now = milliseconds; pulses = 0; pressure_app_poll(); }
/* Holds mask for ms (polls every 10 ms). */
static void run(uint8_t mask, uint32_t ms)
{
    uint32_t end = now+ms;
    button_mask = mask;
    while (now < end) poll_at(now+10);
}
static void tap(uint8_t mask) { run(mask, 200); run(0, 100); }
static void hold(uint8_t mask) { run(mask, 2100); run(0, 100); }

/* One simulated press-and-release (as in test_pressure_app.c), 25 ms samples. */
static void press_event(void)
{
    unsigned completed = pneutouch_features.completed, i;
    ready = true; button_mask = 0; bits = 4000000;
    for (i = 0; i < 40; ++i) poll_at(now+25); /* settling and baseline */
    for (i = 0; i < 125; ++i) {
        int u = (int)i-60, delta = 0;
        if (u >= 0) delta = u <= 10 ? 20000*u : u <= 20 ? 20000*(20-u) :
            u <= 30 ? -20000*(u-20) : u <= 40 ? -20000*(40-u) : 0;
        bits = (uint32_t)(4000000+delta);
        poll_at(now+25);
    }
    assert(pneutouch_features.completed == completed+1U);
    ready = false;
}

static void fill(uint8_t slot, unsigned count, bool commit)
{
    float pool[PNEU_POOL_COUNT];
    for (unsigned j = 0; j < PNEU_POOL_COUNT; ++j) pool[j] = (float)j+1.0f;
    assert(pneu_store_open(slot, 0, "fix"));
    for (unsigned i = 0; i < count; ++i) assert(pneu_store_put(slot, (uint8_t)(i%4U+1U), pool));
    assert(!commit || pneu_store_commit(slot));
}

/* Slot 0: 8 records (2 per class), slot 1: 141, slot 2 OPEN with 2, slot 3 never written. */
static void fixture(uint8_t active)
{
    memset(fram, 0xff, sizeof(fram));
    assert(pneu_store_init());
    fill(0, 8, true); fill(1, 141, true); fill(2, 2, false);
    assert(pneu_store_use(active));
}

static uint16_t slot_count(uint8_t slot, uint8_t state)
{
    pneu_store_slot_t info;
    assert(pneu_store_slot_info(slot, &info) && info.state == state);
    return info.count;
}

static bool slot_name(uint8_t slot, const char *name)
{
    pneu_store_slot_t info;
    return pneu_store_slot_info(slot, &info) && !strcmp(info.name, name);
}

static void boot(void)
{
    clear_log(); ready = false;
    pressure_app_init();
    run(0, 2100); /* the SLOT notice expires */
}

static void test_gestures(void)
{
    app_buttons_reset(0, 0);
    assert(app_buttons_poll(APP_SW4, 0) == APP_BUTTON_NONE && app_buttons_poll(APP_SW4, 30) == APP_BUTTON_NONE);
    assert(app_buttons_poll(0, 900) == APP_BUTTON_NONE && app_buttons_poll(0, 930) == APP_BUTTON_REC_TOGGLE);
    assert(app_buttons_poll(APP_SW5, 1000) == APP_BUTTON_NONE && app_buttons_poll(APP_SW5, 1030) == APP_BUTTON_NONE);
    assert(app_buttons_poll(0, 2200) == APP_BUTTON_NONE && app_buttons_poll(0, 2230) == APP_BUTTON_NONE); /* 1.2 s */
    assert(app_buttons_poll(APP_SW5, 3000) == APP_BUTTON_NONE && app_buttons_poll(APP_SW5, 3030) == APP_BUTTON_NONE);
    assert(app_buttons_poll(APP_SW5, 4999) == APP_BUTTON_NONE && app_buttons_poll(APP_SW5, 5000) == APP_BUTTON_UNDO);
    assert(app_buttons_poll(APP_SW5, 9000) == APP_BUTTON_NONE); /* once per press */
    assert(app_buttons_poll(0, 9010) == APP_BUTTON_NONE && app_buttons_poll(0, 9040) == APP_BUTTON_NONE);
    assert(app_buttons_poll(APP_SW4, 10000) == APP_BUTTON_NONE && app_buttons_poll(APP_SW4, 10030) == APP_BUTTON_NONE);
    assert(app_buttons_poll(APP_SW4, 12000) == APP_BUTTON_REC_NEW);
    assert(app_buttons_poll(0, 12100) == APP_BUTTON_NONE && app_buttons_poll(0, 12200) == APP_BUTTON_NONE);
    assert(app_buttons_poll(APP_SW5, 13000) == APP_BUTTON_NONE && app_buttons_poll(0, 13010) == APP_BUTTON_NONE);
    assert(app_buttons_poll(0, 13100) == APP_BUTTON_NONE); /* 10 ms glitch: no press */
    assert(app_buttons_poll(APP_SW5, 14000) == APP_BUTTON_NONE && app_buttons_poll(APP_SW5, 14030) == APP_BUTTON_NONE);
    assert(app_buttons_poll(0, 14100) == APP_BUTTON_NONE && app_buttons_poll(0, 14130) == APP_BUTTON_LABEL);
}

/* SW4 short appends to the committed active slot; SW5 labels and undoes; SW2/SW3 refused. */
static void test_append(void)
{
    unsigned inits;
    fixture(0); boot();
    assert(!strcmp(idle_text, "READY DINO      ") && model_inits == 1);
    clear_log(); tap(APP_SW4);
    assert(logged("# PNER1,START,0,8,0\r\n") && !strcmp(idle_text, "REC TAIL   2/  8"));
    assert(app_record.active && slot_count(0, PNEU_SLOT_OPEN) == 8);
    clear_log(); press_event();
    assert(logged("# PNER1,REC,0,TAIL,9\r\n") && logged("# PNEC1,") && displays == 0 && predictions == 1);
    assert(!strcmp(saved_text, "+TAIL 3         ") && slot_count(0, PNEU_SLOT_OPEN) == 9);
    run(0, 1100); assert(!strcmp(idle_text, "REC TAIL   3/  9"));
    tap(APP_SW5); assert(!strcmp(idle_text, "REC BACK   2/  9"));
    clear_log(); hold(APP_SW5);
    assert(!strcmp(idle_text, "UNDO TAIL 2     ") && logged("# PNER1,UNDO,0,TAIL,8\r\n"));
    assert(slot_count(0, PNEU_SLOT_OPEN) == 8);
    run(0, 1000); assert(!strcmp(idle_text, "REC BACK   2/  8"));
    hold(APP_SW5); assert(!strcmp(idle_text, "NOTHING TO UNDO "));
    tap(APP_SW3); assert(!strcmp(idle_text, "STOP REC FIRST  ") && pneu_store_active() == 0);
    run(APP_SW2, 300); assert(!strcmp(idle_text, "STOP REC FIRST  ")); /* no HOLD hint */
    run(APP_SW2, 1900); run(0, 100);
    assert(!strcmp(idle_text, "STOP REC FIRST  ") && model_tunes == 0);
    inits = model_inits; clear_log(); tap(APP_SW4);
    assert(logged("# PNER1,STOP,0,8,0\r\n") && logged("# PNEM3,READY,slot=0,count=8,"));
    assert(!app_record.active && slot_count(0, PNEU_SLOT_COMMITTED) == 8 && model_inits == inits+1);
    assert(!strcmp(idle_text, "S0 DINO    8  k3"));
    run(0, 2000); assert(!strcmp(idle_text, "READY DINO      "));
    tap(APP_SW5); hold(APP_SW5); assert(!strcmp(idle_text, "READY DINO      ")); /* SW5 outside recording */
    tap(APP_SW4); assert(!strcmp(idle_text, "REC BACK   2/  8")); /* label kept */
    tap(APP_SW4); assert(!app_record.active);
}

/* SW4 long records into the first EMPTY slot; stop needs 4 records. */
static void test_new_slot(void)
{
    fixture(0); boot();
    clear_log(); hold(APP_SW4);
    assert(!strcmp(idle_text, "NEW: DINO  (SW5)") && !logged("START") && !app_record.active);
    tap(APP_SW4);
    assert(logged("# PNER1,START,3,0,1\r\n") && pneu_store_active() == 3 && !strcmp(idle_text, "REC TAIL   0/  0"));
    assert(slot_name(3, "DINOBTN") && app_record.object == PNEU_OBJECT_DINO);
    for (unsigned i = 0; i < 4; ++i) tap(APP_SW5);
    assert(!strcmp(idle_text, "REC TAIL   0/  0"));
    press_event(); tap(APP_SW5); press_event();
    tap(APP_SW4); assert(!strcmp(idle_text, "NEED 4 RECORDS  ") && app_record.active);
    fram_drop_writes = true; clear_log(); press_event(); fram_drop_writes = false;
    assert(!strcmp(saved_text, "STORE ERROR     ") && logged("# PNER1,ERROR,STORE\r\n") && app_record.count == 2);
    pool_fails = true; clear_log(); press_event(); pool_fails = false;
    assert(!strcmp(saved_text, "BAD PRESS       ") && logged("# PNER1,ERROR,POOL\r\n"));
    press_event(); press_event();
    run(0, 2100); assert(!strcmp(idle_text, "REC BACK   3/  4"));
    clear_log(); tap(APP_SW4);
    assert(logged("# PNER1,STOP,3,4,4\r\n") && slot_count(3, PNEU_SLOT_COMMITTED) == 4);
    assert(!strcmp(idle_text, "S3 DINO    4  k3") && pneutouch_model.ready);
    clear_log(); hold(APP_SW4);
    assert(!strcmp(idle_text, "NO EMPTY SLOT   ") && logged("# PNER1,ERROR,NO_EMPTY_SLOT\r\n") && !app_record.active);
}

/* SW4 long: object chooser, KRAG slot opened as KRAGBTN, HAT/FACE labels only. */
static void test_chooser(void)
{
    fixture(0); boot();
    clear_log(); hold(APP_SW4);
    assert(!strcmp(idle_text, "NEW: DINO  (SW5)") && app_chooser.active && !app_record.active);
    tap(APP_SW5); assert(!strcmp(idle_text, "NEW: KRAG  (SW5)"));
    tap(APP_SW3); tap(APP_SW2); hold(APP_SW5); /* ignored */
    assert(!strcmp(idle_text, "NEW: KRAG  (SW5)") && pneu_store_active() == 0 && model_tunes == 0 && !logged("PNES1"));
    tap(APP_SW5); tap(APP_SW5); assert(!strcmp(idle_text, "NEW: KRAG  (SW5)"));
    tap(APP_SW4);
    assert(logged("# PNER1,START,3,0,1\r\n") && pneu_store_active() == 3 && !app_chooser.active);
    assert(slot_name(3, "KRAGBTN") && app_record.object == PNEU_OBJECT_KRAG && !strcmp(idle_text, "REC HAT    0/  0"));
    tap(APP_SW5); assert(!strcmp(idle_text, "REC FACE   0/  0"));
    tap(APP_SW5); assert(!strcmp(idle_text, "REC HAT    0/  0"));
    clear_log(); press_event();
    assert(logged("# PNER1,REC,3,HAT,1\r\n") && !strcmp(saved_text, "+HAT  1         "));
    tap(APP_SW5); press_event(); press_event(); tap(APP_SW5); press_event();
    run(0, 1100); assert(!strcmp(idle_text, "REC HAT    2/  4"));
    hold(APP_SW5); assert(!strcmp(idle_text, "UNDO HAT  1     ")); press_event();
    clear_log(); tap(APP_SW4);
    assert(logged("# PNER1,STOP,3,4,4\r\n") && logged(",object=KRAG,sel="));
    assert(pneutouch_model.object == PNEU_OBJECT_KRAG);
    assert(!strcmp(idle_text, "S3 KRAG    4  k3"));
    run(0, 2000); assert(!strcmp(idle_text, "READY KRAG      "));
    tap(APP_SW4); assert(!strcmp(idle_text, "REC HAT    2/  4")); /* append keeps KRAG */
    tap(APP_SW5); tap(APP_SW5); tap(APP_SW5); assert(!strcmp(idle_text, "REC FACE   2/  4"));
    tap(APP_SW4); run(0, 2000); assert(!app_record.active);
    assert(pneu_store_erase(1)); /* an empty slot again */
    clear_log(); hold(APP_SW4); assert(!strcmp(idle_text, "NEW: KRAG  (SW5)")); /* the active slot's object */
    hold(APP_SW4); /* SW4 long cancels */
    assert(!app_chooser.active && !strcmp(idle_text, "READY KRAG      ") && !logged("START"));
    hold(APP_SW4); tap(APP_SW5); assert(!strcmp(idle_text, "NEW: DINO  (SW5)"));
    run(0, 9800); assert(app_chooser.active && !strcmp(idle_text, "NEW: DINO  (SW5)"));
    run(0, 200); /* 10 s after the last press */
    assert(!app_chooser.active && !strcmp(idle_text, "READY KRAG      ") && !logged("START"));
    tap(APP_SW4); tap(APP_SW4); assert(!app_record.active); /* SW4 short works again */
}

/* Power lost while recording: >= 4 records are committed at boot, fewer stay OPEN. */
static void test_boot_open_slot(void)
{
    float pool[PNEU_POOL_COUNT];
    for (unsigned j = 0; j < PNEU_POOL_COUNT; ++j) pool[j] = 2.0f;
    fixture(2); boot();
    assert(!logged("AUTOCOMMIT") && !strcmp(idle_text, "NO MODEL") && slot_count(2, PNEU_SLOT_OPEN) == 2);
    clear_log(); tap(APP_SW4); /* an OPEN active slot continues */
    assert(logged("# PNER1,START,2,2,0\r\n") && !strcmp(idle_text, "REC TAIL   1/  2"));
    for (uint8_t label = 1; label <= 3; ++label) assert(pneu_store_put(2, label, pool));
    clear_log(); pressure_app_init();
    assert(logged("# PNER1,AUTOCOMMIT,2,5\r\n") && logged("# PNEM3,READY,slot=2,count=5,"));
    assert(slot_count(2, PNEU_SLOT_COMMITTED) == 5 && !app_record.active && !strcmp(idle_text, "S2 DINO    5  k3"));
    fram_drop_writes = true; clear_log(); run(0, 2100); tap(APP_SW4); fram_drop_writes = false;
    assert(!strcmp(idle_text, "STORE ERROR     ") && !app_record.active); /* reopen not verified */
}

/* SW4 long while recording removes this session's records. */
static void test_discard(void)
{
    unsigned inits;
    fixture(0); boot();
    tap(APP_SW4); press_event(); press_event(); /* append: committed slot 0, 8 -> 10 */
    inits = model_inits; clear_log(); hold(APP_SW4);
    assert(logged("# PNER1,DISCARD,0,2\r\n") && !app_record.active && slot_count(0, PNEU_SLOT_COMMITTED) == 8);
    assert(model_inits == inits+1 && logged("resumed") && !strcmp(idle_text, "DISCARDED       "));
    run(0, 2000); assert(!strcmp(idle_text, "READY DINO      "));
    hold(APP_SW4); tap(APP_SW4); press_event(); assert(pneu_store_active() == 3); /* fresh: slot 3 */
    inits = model_inits; clear_log(); hold(APP_SW4);
    assert(logged("# PNER1,DISCARD,3,1\r\n") && slot_count(3, PNEU_SLOT_EMPTY) == 0 && pneu_store_active() == 0);
    assert(model_inits == inits+1 && pneutouch_model.ready && pneutouch_model.slot == 0);
    assert(!strcmp(idle_text, "DISCARDED       "));
    fixture(2); boot(); /* OPEN slot with 2: stays OPEN, no retrain */
    tap(APP_SW4); press_event();
    inits = model_inits; clear_log(); hold(APP_SW4);
    assert(logged("# PNER1,DISCARD,2,1\r\n"));
    assert(slot_count(2, PNEU_SLOT_OPEN) == 2 && model_inits == inits && !strcmp(idle_text, "DISCARDED       "));
    run(0, 2000); assert(!strcmp(idle_text, "NO MODEL"));
}

/* PAI1/PST1 MODE 1 ends a recording: commit with >= 4 records, else leave it OPEN. */
static void test_validation(void)
{
    unsigned inits;
    fixture(0); boot();
    tap(APP_SW4); press_event();
    clear_log(); validating = true; run(0, 100);
    assert(logged("# PNER1,STOP,0,9,1\r\n") && !app_record.active && slot_count(0, PNEU_SLOT_COMMITTED) == 9);
    assert(!strcmp(idle_text, "VALIDATION"));
    inits = model_inits; validating = false; run(0, 100);
    assert(model_inits == inits+1 && pneutouch_model.ready && pneutouch_model.store_count == 9);
    run(0, 2000); hold(APP_SW4); tap(APP_SW4); press_event(); press_event(); /* fresh slot 3 with 2 */
    clear_log(); validating = true; run(0, 100);
    assert(logged("# PNER1,ABORT,3,2\r\n") && !app_record.active && slot_count(3, PNEU_SLOT_OPEN) == 2);
    validating = false; run(0, 100);
    assert(!pneutouch_model.ready && pneutouch_model.slot == 3); /* the OPEN slot has no model */
}

int main(void)
{
    test_gestures();
    test_append();
    test_new_slot();
    test_chooser();
    test_boot_open_slot();
    test_discard();
    test_validation();
    puts("App buttons: SW4/SW5 gestures, recording append/new slot/label/undo/stop, SW2/SW3 refused while recording, "
         "discard, validation stop/abort, store errors, boot autocommit, object chooser and KRAG labels PASS");
    return 0;
}
