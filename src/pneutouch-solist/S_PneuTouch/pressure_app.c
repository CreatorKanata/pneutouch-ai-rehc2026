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
#include "model_tune.h"
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
static uint16_t tune_done; /* trainings finished (phase 2) */
static bool tuning;

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
            pneu_display_result(label);
        }
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

/* Right-aligned decimal in width columns. */
static void digits(char *out, unsigned width, uint32_t value)
{
    unsigned i = width;
    do { out[--i] = (char)('0'+value%10U); value /= 10U; } while (value && i);
    while (i) out[--i] = ' ';
}

/* "TUNE k5 r3/4    " (phase 2) or "TUNE k5 r3/4 57%" (percent of the current phase):
   candidate k and round (fold+1) of the training that runs next. */
static void tune_line(char line[17], bool with_percent, uint16_t done, uint16_t total)
{
    memcpy(line, "TUNE k  r /     ", 17);
    digits(line+6, 1, PNEU_TUNE_K_MIN+tune_done/PNEU_TUNE_FOLDS);
    digits(line+9, 1, tune_done%PNEU_TUNE_FOLDS+1U); digits(line+11, 1, PNEU_TUNE_FOLDS);
    if (!with_percent) return;
    digits(line+12, 3, total ? (uint32_t)done*100U/total : 0U); line[15] = '%';
}

/* Training blocks for seconds: keep the LCD state machine and watchdog going.
   Line 1 "LEARN<phase> done/total" (or the TUNE line while tuning); line 2 as
   soon as the selection is known. */
static void show_progress(uint8_t phase, uint16_t done, uint16_t total)
{
    char line[17] = "LEARN0          ";
    if (phase == PNEU_PHASE_TUNE) {
        tuning = done < total;
        tune_done = tuning ? done : (uint16_t)(done-1U); /* the end shows the last training */
        tune_line(line, false, done, total);
    } else if (tuning) tune_line(line, true, done, total);
    else {
        line[5] = (char)('0'+phase%10U);
        digits(line+7, 4, done); line[11] = '/'; digits(line+12, 4, total);
    }
    if (phase == PNEU_PHASE_TRAIN && done == 0U) show_line2();
    pneu_display_idle(line);
    pneu_display_poll();
    board_watchdog();
}

static const char *const zone_names[] = {"NONE", "TAIL", "BACK", "LEGS", "HEAD"};

static const char *zone_name(uint8_t label) { return zone_names[label <= 4U ? label : 0U]; }

void pneu_ready_text(uint8_t advice, char out[17])
{
    memcpy(out, "READY", 6);
    if (advice < 1U || advice > 4U) return;
    memcpy(out+5, " MORE ", 6);
    memcpy(out+11, zone_name(advice), 5); /* 4 letters + NUL: 15 characters */
}

static bool train_model(void)
{
    char text[17];
    bool ready;
    tuning = false; /* a failed PST1 TUNE never sent its last phase 2 */
    ready = pneu_model_init();
    show_line2(); /* blank after a failure */
    pneu_ready_text(pneutouch_model.advice, text);
    pneu_display_idle(ready ? text :
                      pneutouch_model.error == PNEU_MODEL_ERR_NO_SLOT ? "NO MODEL" : "AI ERROR");
    return ready;
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
    pneu_text(",err="); pneu_number(pneutouch_model.error); pneu_text(",sel=");
    send_list(pneutouch_model.selected, pneutouch_model.selected_count);
    pneu_text(",fisher=");
    for (i = 0; i < 14U; ++i) {
        float scaled = pneutouch_model.fisher[i]*1000.0f+0.5f; /* x1000 */
        if (i) board_putc(':');
        pneu_number(scaled >= FISHER_PRINT_LIMIT ? (uint32_t)FISHER_PRINT_LIMIT : scaled > 0.0f ? (uint32_t)scaled : 0U);
    }
    pneu_text("\r\n");
    send_tune_banner();
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
    (void)train_model();
    acquisition_init(&acquisition, sensor, board_millis());
    pneu_text("# PneutouchAi Live; PNEU1,seq,ms,raw; baud=");
    pneu_number(PNEU_UART_BAUD); pneu_text("; nominal_sps="); pneu_number(PNEU_SAMPLE_RATE_HZ);
    pneu_text("; fw="); pneu_text(PNEU_FIRMWARE_TAG); pneu_text("\r\n");
    pneu_text("# PNEF1; version=1; features=12; units_x1000; classes=tail/back/legs/head; gesture=press_release_1s\r\n");
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
        if (!was_validation) pneu_display_idle("VALIDATION");
        was_validation = true; return;
    }
    if (was_validation) {
        const hx710b_t sensor = {0, board_dout_high, board_sensor_pulse, PNEU_HX_PULSES};
        /* PAI1 uses the same chip instance. Retrain from the (maybe new) active slot. */
        (void)train_model();
        acquisition_init(&acquisition, sensor, board_millis());
        pneu_features_reset(&features); pending = false; was_validation = false;
        pneu_text("# PneutouchAi live acquisition resumed; sequence restarts\r\n");
    }
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
