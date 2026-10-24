/* HX710B acquisition, bounded features and explicit UART AI validation mode. */
#include "pressure_app.h"
#include "board.h"
#include "acquisition.h"
#include "pneu_config.h"
#include "ai_validation.h"
#include "pressure_model.h"
#include "pressure_display.h"
#include "uart_text.h"
#include "firmware_tag.h"
#include "app_buttons.h"
#include "app_record.h"
#include "app_chooser.h"
#include "object_profile.h"
#include <string.h>

volatile pneutouch_status_t pneutouch_status;
volatile pneutouch_feature_status_t pneutouch_features;
static acquisition_t acquisition;
static pneu_features_t features;
static pneu_feature_event_t pending_features;
static unsigned pending_part;
static bool pending;
static bool was_validation;
static bool lcd_ready_reported, lcd_failed_reported;
static uint32_t lcd_updates_reported;
static uint8_t tune_phase;             /* PNEU_PHASE_TUNE (screening) or PNEU_PHASE_FINAL */
static uint16_t tune_done, tune_total; /* last count of that phase */
static bool tuning;
static bool notice_active; /* an LCD notice is up until notice_until */
static uint32_t notice_until;
static uint8_t detect_object; /* pneu_object_t whose thresholds features uses */

/* UART text goes through uart_text.c: no printf, heap allocation or semihosting. */
#define FISHER_PRINT_LIMIT 4.0e9f

static void send_sample(const pressure_sample_t *sample)
{
    pneu_text("PNEU1,"); pneu_number(sample->sequence); board_putc(',');
    pneu_number(sample->milliseconds); board_putc(',');
    if (sample->raw < 0) {
        board_putc('-'); pneu_number((uint32_t)(-sample->raw));
    } else pneu_number((uint32_t)sample->raw);
    pneu_text("\r\n");
}

/* Two short comment frames, one per acquired sample, preserve existing PNEU1
   readers and avoid a long blocking UART burst. Values use feature units x1000. */
static void send_feature_part(void)
{
    unsigned i, start = pending_part*6U;
    pneu_text("# PNEF1,"); pneu_number(pending_features.id); board_putc(',');
    pneu_number(pending_features.trigger_ms); board_putc(',');
    pneu_number(pending_features.end_ms); board_putc(','); pneu_number(start);
    for (i = start; i < start+6U; ++i) {
        board_putc(','); pneu_number((uint32_t)(pending_features.value[i]*1000.0f+0.5f));
    }
    pneu_text("\r\n");
    if (++pending_part == 2) pending = false;
}

static void show_notice(const char *text, uint32_t ms)
{
    pneu_display_idle(text);
    notice_active = true; notice_until = board_millis()+ms;
}

/* "# PNEC1" with the prediction of the model in use (also while recording). */
static pneu_demo_class_t send_prediction(void)
{
    unsigned i;
    static const char hex[] = "0123456789abcdef";
    pneu_demo_class_t label = pneu_model_predict(features.event.value, features.event.peak,
                                                 features.event.trough);
    pneu_text("# PNEC1,"); pneu_number(features.event.id); board_putc(',');
    pneu_number(board_millis()); board_putc(','); pneu_text(pneu_class_name(label));
    board_putc(','); pneu_number(pneutouch_model.inference_us);
    for (i = 0; i < 4; ++i) {
        int shift; board_putc(',');
        for (shift = 12; shift >= 0; shift -= 4)
            board_putc(hex[(pneutouch_model.scores[i] >> shift)&15U]);
    }
    pneu_text("\r\n");
    return label;
}

/* While recording the LCD shows the saved count ("+HEAD 39") instead of the result. */
static void complete_event(void)
{
    char notice[17];
    pneu_demo_class_t label = send_prediction();
    if (!app_record.active) { pneu_display_result(label); return; }
    if (app_record_event(features.event.value, features.event.peak, features.event.trough, notice))
        show_notice(notice, APP_BRIEF_MS);
    else show_notice(notice, APP_NOTICE_MS);
}

static void process_features(const pressure_sample_t *sample)
{
    uint32_t before = board_millis(), elapsed;
    pneu_event_result_t result = pneu_features_feed(&features, sample->sequence,
                                                   sample->milliseconds, sample->raw);
    elapsed = board_millis()-before;
    if (elapsed > pneutouch_features.max_compute_ms) pneutouch_features.max_compute_ms = elapsed;
    pneutouch_features.last_result = result;
    if (result == PNEU_EVENT_START) {
        ++pneutouch_features.started;
        pneu_text("# PNEE1,"); pneu_number(features.event.id); pneu_text(",START,");
        pneu_number(features.event.trigger_ms); pneu_text("\r\n");
    } else if (result == PNEU_EVENT_COMPLETE) {
        ++pneutouch_features.completed;
        pneutouch_features.last_complete = features.event;
        pending_features = features.event; pending_part = 0; pending = true;
        complete_event();
    } else if (result == PNEU_EVENT_INVALID || result == PNEU_EVENT_TIMEOUT) {
        ++pneutouch_features.rejected;
        pneu_text("# PNEE1,"); pneu_number(features.event.id);
        pneu_text(result == PNEU_EVENT_TIMEOUT ? ",TIMEOUT," : ",INVALID,");
        pneu_number(sample->milliseconds); pneu_text("\r\n");
    } else if (result == PNEU_EVENT_RESET) ++pneutouch_features.resets;
}

static void show_line2(void)
{
    char line[17];
    unsigned i;
    for (i = 0; i < sizeof(line); ++i) line[i] = pneutouch_model.lcd_line2[i];
    line[16] = '\0';
    pneu_display_line2(line);
}

/* "TUNE 37/198     " (screening) or "FINAL 3/6       " (finals, 2 digits each from 10). */
static void tune_text(char line[17])
{
    unsigned width = tune_total >= 10U ? 2U : 1U;
    if (tune_phase == PNEU_PHASE_FINAL) {
        memcpy(line, "FINAL           ", 17);
        app_digits(line+6, width, tune_done); line[6+width] = '/';
        app_digits(line+7+width, width, tune_total);
        return;
    }
    memcpy(line, "TUNE   /        ", 17);
    app_digits(line+4, 3, tune_done); app_digits(line+8, 3, tune_total);
}

/* The tune line, plus the percent of the training or statistics pass that runs
   inside it ("TUNE 37/198  57%"); columns 12..15 stay blank on the tune callbacks. */
static void tune_line(char line[17], bool with_percent, uint16_t done, uint16_t total)
{
    tune_text(line);
    if (!with_percent) return;
    app_digits(line+12, 3, total ? (uint32_t)done*100U/total : 0U); line[15] = '%';
}

/* Training blocks for seconds: keep the LCD state machine and watchdog going.
   Line 1 "LEARN<phase> done/total" (or the TUNE/FINAL line while tuning); line 2 as
   soon as the selection is known. */
static void show_progress(uint8_t phase, uint16_t done, uint16_t total)
{
    char line[17] = "LEARN0          ";
    if (phase == PNEU_PHASE_TUNE || phase == PNEU_PHASE_FINAL) {
        tune_phase = phase; tune_done = done; tune_total = total;
        tuning = phase == PNEU_PHASE_TUNE || done < total; /* ends with the last final */
        tune_line(line, false, done, total);
    } else if (tuning) tune_line(line, true, done, total);
    else {
        line[5] = (char)('0'+phase%10U);
        app_digits(line+7, 4, done); line[11] = '/'; app_digits(line+12, 4, total);
    }
    if (phase == PNEU_PHASE_TRAIN && done == 0U) show_line2();
    pneu_display_idle(line);
    pneu_display_poll();
    board_watchdog();
}

static const char *object_tag(void) { return pneu_object((pneu_object_t)pneutouch_model.object)->tag; }

/* Advice zone of the active slot's object, NONE for 0. */
static const char *zone_name(uint8_t label)
{
    return label ? pneu_object_class_name((pneu_object_t)pneutouch_model.object, label) : "NONE";
}

/* LCD line 1 when nothing else is shown: the chooser, the REC line while recording, else
   "READY DINO      ", NO MODEL or AI ERROR. The TUNE advice goes to UART only (# PNEM4). */
static void show_idle(void)
{
    char text[17];
    if (app_chooser.active) app_chooser_line(text);
    else if (app_record.active) app_record_line(text);
    else if (pneutouch_model.ready) { memcpy(text, "READY           ", 17); memcpy(text+6, object_tag(), 4); }
    else { pneu_display_idle(pneutouch_model.error == PNEU_MODEL_ERR_NO_SLOT ? "NO MODEL" : "AI ERROR"); return; }
    pneu_display_idle(text);
}

/* Thresholds of the pressed object (recording slot's, else the model's); true = changed. */
static bool follow_object(void)
{
    uint8_t object = app_record.active ? app_record.object : pneutouch_model.object;
    bool changed = object != detect_object;
    detect_object = object;
    pneu_features_set_detection(&features, pneu_object_detect((pneu_object_t)object));
    return changed;
}

/* Success shows "S2 DINO  176  k4" for 2 s, then the idle text (poll_buttons). */
static bool train_model(void)
{
    char text[17];
    bool ready;
    tuning = false; /* a failed PST1 TUNE never sent its last phase 2 */
    notice_active = false;
    ready = pneu_model_init();
    (void)follow_object(); /* also after a failure (DINO without a readable slot) */
    show_line2(); /* blank after a failure */
    if (!ready) { show_idle(); return false; }
    app_slot_notice(text, pneutouch_model.slot, pneutouch_model.object, pneutouch_model.store_count,
                    pneutouch_model.selected_count);
    show_notice(text, APP_NOTICE_MS);
    return true;
}

static void send_list(const volatile uint8_t *items, unsigned count)
{
    unsigned i;
    for (i = 0; i < count; ++i) { if (i) board_putc(':'); pneu_number(items[i]); }
}

/* Tuning of the active slot and the records per class of the last init. */
static void send_tune_banner(void)
{
    unsigned i;
    pneu_text("# PNEM4,k="); pneu_number(pneutouch_model.tuned_k);
    pneu_text(",mask="); pneu_hex16(pneutouch_model.mask);
    pneu_text(",recall="); send_list(pneutouch_model.recall, 4);
    pneu_text(",counts=");
    for (i = 0; i < 4U; ++i) { if (i) board_putc(':'); pneu_number(pneutouch_model.class_count[i]); }
    pneu_text(",advice="); pneu_text(zone_name(pneutouch_model.advice)); pneu_text("\r\n");
}

static void send_model_banner(void)
{
    unsigned i;
    pneu_text(pneutouch_model.ready ? "# PNEM1,READY," : "# PNEM1,ERROR,");
    pneu_number(pneutouch_model.training_examples); board_putc(',');
    pneu_number(pneutouch_model.training_steps); board_putc(',');
    pneu_number(pneutouch_model.training_us); pneu_text("\r\n");
    pneu_text("# PNEM2,NONE,0,0,single_instance\r\n"); /* kept for older log readers */
    pneu_text(pneutouch_model.ready ? "# PNEM3,READY,slot=" : "# PNEM3,ERROR,slot=");
    pneu_number(pneutouch_model.slot); pneu_text(",count="); pneu_number(pneutouch_model.store_count);
    pneu_text(",err="); pneu_number(pneutouch_model.error);
    pneu_text(",object="); pneu_text(object_tag()); pneu_text(",sel=");
    send_list(pneutouch_model.selected, pneutouch_model.selected_count);
    pneu_text(",fisher=");
    for (i = 0; i < 14U; ++i) {
        float scaled = pneutouch_model.fisher[i]*1000.0f+0.5f; /* x1000 */
        if (i) board_putc(':');
        pneu_number(scaled >= FISHER_PRINT_LIMIT ? (uint32_t)FISHER_PRINT_LIMIT : scaled > 0.0f ? (uint32_t)scaled : 0U);
    }
    pneu_text("\r\n");
    send_tune_banner();
    app_detect_banner(&features.detect, detect_object);
}

/* After PAI1/PST1 (same chip instance), a TUNE or a slot switch: retrain from the
   (maybe new) active slot and restart acquisition, which the blocking work starved. */
static void resume_live(void)
{
    const hx710b_t sensor = {0, board_dout_high, board_sensor_pulse, PNEU_HX_PULSES};
    (void)train_model();
    acquisition_init(&acquisition, sensor, board_millis());
    pneu_features_reset(&features); pending = false;
    send_model_banner();
    pneu_text("# PneutouchAi live acquisition resumed; sequence restarts\r\n");
}

/* SW4/SW5 (app_record.c or, while it is up, the chooser decides); stop commits and retrains. */
static void record_button(app_button_event_t event)
{
    char line[17];
    bool was_recording = app_record.active;
    switch (app_chooser.active ? app_chooser_button(event, line) : app_record_button(event, line)) {
    case APP_REC_IDLE: notice_active = false; show_idle(); break;
    case APP_REC_NOTICE: show_notice(line, APP_NOTICE_MS); break;
    case APP_REC_BRIEF: show_notice(line, APP_BRIEF_MS); break;
    case APP_REC_RETRAIN: resume_live(); break;
    case APP_REC_DISCARD: resume_live(); show_notice(line, APP_NOTICE_MS); break;
    default: break;
    }
    if (!was_recording && app_record.active && follow_object()) app_detect_banner(&features.detect, detect_object);
}

/* SW2 TUNE and SW3 slot switch need a committed slot: refused while recording. */
static void slot_button(app_button_event_t event)
{
    char line[17];
    if (app_record.active) { show_notice("STOP REC FIRST  ", APP_NOTICE_MS); return; }
    if (event == APP_BUTTON_TUNE) { app_run_tune(); resume_live(); }
    else if (app_next_slot(line)) { pneu_display_idle(line); resume_live(); }
    else show_notice(line, APP_NOTICE_MS);
}

/* Normal (non-validation) path only. Only TUNE and the retraining block. */
static void poll_buttons(void)
{
    uint32_t now = board_millis();
    app_button_event_t event = app_buttons_poll(board_buttons(), now);
    if (app_chooser.active) { record_button(event); return; } /* SW2/SW3 ignored, timeout */
    switch (event) {
    case APP_BUTTON_HOLD:
        if (!app_record.active) { notice_active = false; pneu_display_idle("HOLD 2s: TUNE   "); }
        break;
    case APP_BUTTON_CANCEL: if (!app_record.active) show_idle(); break;
    case APP_BUTTON_TUNE: case APP_BUTTON_NEXT: slot_button(event); break;
    case APP_BUTTON_NONE:
        if (notice_active && (int32_t)(now-notice_until) >= 0) { notice_active = false; show_idle(); }
        break;
    default: record_button(event); break;
    }
}

void pressure_app_init(void)
{
    const hx710b_t sensor = {0, board_dout_high, board_sensor_pulse, PNEU_HX_PULSES};
    pneutouch_status = (pneutouch_status_t){PNEU_STARTING, 0, 0, 0, 0};
    pneutouch_features = (pneutouch_feature_status_t){0};
    pneu_features_init(&features); pending = false;
    board_init();
    board_store_init();
    pneu_ai_validation_init(); was_validation = false;
    pneu_display_init();
    lcd_ready_reported = lcd_failed_reported = false; lcd_updates_reported = 0;
    pneu_model_set_progress(show_progress); /* also used by PAI1 exit and PST1 TRAIN */
    app_record_boot(); /* commits a slot left OPEN by a power loss while recording */
    (void)train_model();
    app_buttons_reset(board_buttons(), board_millis()); /* held at boot: ignored until released */
    acquisition_init(&acquisition, sensor, board_millis());
    pneu_text("# PneutouchAi Live; PNEU1,seq,ms,raw; baud=");
    pneu_number(PNEU_UART_BAUD); pneu_text("; nominal_sps="); pneu_number(PNEU_SAMPLE_RATE_HZ);
    pneu_text("; fw="); pneu_text(PNEU_FIRMWARE_TAG); pneu_text("\r\n");
    pneu_text("# PNEF1; version=1; features=12; units_x1000; classes=tail/back/legs/head; gesture=press_release_1s; "
              "objects=dino:tail/back/legs/head,krag:hat/face\r\n");
    send_model_banner();
    pneutouch_status.state = PNEU_SETTLING;
}

void pressure_app_poll(void)
{
    pressure_sample_t sample;
    board_watchdog();
    pneu_display_poll();
    if (pneutouch_display.ready && !lcd_ready_reported) {
        pneu_text("# PNEL1,READY\r\n"); lcd_ready_reported = true;
    }
    if (pneutouch_display.failed && !lcd_failed_reported) {
        pneu_text("# PNEL1,ERROR,I2C\r\n"); lcd_failed_reported = true;
    }
    if (pneutouch_display.updates != lcd_updates_reported) {
        lcd_updates_reported = pneutouch_display.updates;
        pneu_text("# PNEL1,"); pneu_text(pneu_class_name(pneutouch_display.label)); board_putc(',');
        pneu_number(pneutouch_display.label ? pneutouch_display.shown_ms : pneutouch_display.cleared_ms);
        pneu_text("\r\n");
    }
    if (pneu_ai_validation_poll()) {
        if (!was_validation) { app_record_abort(); pneu_display_idle("VALIDATION"); }
        app_buttons_reset(board_buttons(), board_millis()); notice_active = false;
        was_validation = true; return;
    }
    if (was_validation) { resume_live(); was_validation = false; }
    poll_buttons();
    acquisition_result_t result = acquisition_poll(&acquisition, board_millis(), &sample);
    if (result == ACQ_SAMPLE) {
        pneutouch_status.raw = sample.raw;
        pneutouch_status.milliseconds = sample.milliseconds;
        ++pneutouch_status.samples;
        pneutouch_status.state = PNEU_STREAMING;
        send_sample(&sample);
        if (pending) send_feature_part();
        process_features(&sample);
    } else if (result == ACQ_TIMEOUT) {
        ++pneutouch_status.timeouts;
        pneutouch_status.state = PNEU_SENSOR_TIMEOUT;
        pneu_text("# ERROR,HX710B_TIMEOUT\r\n");
        pneu_features_reset(&features); pending = false;
        ++pneutouch_features.resets;
    } else if (acquisition.mode_selected && acquisition.settling) {
        pneutouch_status.state = PNEU_SETTLING;
    }
}
