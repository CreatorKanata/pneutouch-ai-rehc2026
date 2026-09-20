/* HX710B acquisition, bounded features and explicit UART AI validation mode. */
#include "pressure_app.h"
#include "board.h"
#include "acquisition.h"
#include "pneu_config.h"
#include "ai_validation.h"

volatile pneutouch_status_t pneutouch_status;
volatile pneutouch_feature_status_t pneutouch_features;
static acquisition_t acquisition;
static pneu_features_t features;
static pneu_feature_event_t pending_features;
static unsigned pending_part;
static bool pending;
static bool was_validation;

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

/* Two short comment frames, one per acquired sample, preserve existing PNEU1
   readers and avoid a long blocking UART burst. Values use feature units x1000. */
static void send_feature_part(void)
{
    unsigned i, start = pending_part*6U;
    text("# PNEF1,"); number(pending_features.id); board_putc(',');
    number(pending_features.trigger_ms); board_putc(',');
    number(pending_features.end_ms); board_putc(','); number(start);
    for (i = start; i < start+6U; ++i) {
        board_putc(','); number((uint32_t)(pending_features.value[i]*1000.0f+0.5f));
    }
    text("\r\n");
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
        text("# PNEE1,"); number(features.event.id); text(",START,");
        number(features.event.trigger_ms); text("\r\n");
    } else if (result == PNEU_EVENT_COMPLETE) {
        ++pneutouch_features.completed;
        pneutouch_features.last_complete = features.event;
        pending_features = features.event; pending_part = 0; pending = true;
    } else if (result == PNEU_EVENT_INVALID || result == PNEU_EVENT_TIMEOUT) {
        ++pneutouch_features.rejected;
        text("# PNEE1,"); number(features.event.id);
        text(result == PNEU_EVENT_TIMEOUT ? ",TIMEOUT," : ",INVALID,");
        number(sample->milliseconds); text("\r\n");
    } else if (result == PNEU_EVENT_RESET) ++pneutouch_features.resets;
}

void pressure_app_init(void)
{
    const hx710b_t sensor = {0, board_dout_high, board_sensor_pulse, PNEU_HX_PULSES};
    pneutouch_status = (pneutouch_status_t){PNEU_STARTING, 0, 0, 0, 0};
    pneutouch_features = (pneutouch_feature_status_t){0};
    pneu_features_init(&features); pending = false;
    board_init();
    pneu_ai_validation_init(); was_validation = false;
    acquisition_init(&acquisition, sensor, board_millis());
    text("# PneutouchAi Phase0; PNEU1,seq,ms,raw; baud=");
    number(PNEU_UART_BAUD); text("; nominal_sps="); number(PNEU_SAMPLE_RATE_HZ);
    text("\r\n");
    text("# PNEF1; version=1; features=12; units_x1000; classes=tail/back/legs/head; gesture=press_release_1s\r\n");
    pneutouch_status.state = PNEU_SETTLING;
}

void pressure_app_poll(void)
{
    pressure_sample_t sample;
    board_watchdog();
    if (pneu_ai_validation_poll()) { was_validation = true; return; }
    if (was_validation) {
        const hx710b_t sensor = {0, board_dout_high, board_sensor_pulse, PNEU_HX_PULSES};
        acquisition_init(&acquisition, sensor, board_millis());
        pneu_features_reset(&features); pending = false; was_validation = false;
        text("# PneutouchAi live acquisition resumed; sequence restarts\r\n");
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
        text("# ERROR,HX710B_TIMEOUT\r\n");
        pneu_features_reset(&features); pending = false;
        ++pneutouch_features.resets;
    } else if (acquisition.mode_selected && acquisition.settling) {
        pneutouch_status.state = PNEU_SETTLING;
    }
}
