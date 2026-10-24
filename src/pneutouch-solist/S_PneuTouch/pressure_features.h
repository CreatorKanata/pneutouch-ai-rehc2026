/* Pressure feature v1: bounded, causal, label-free extraction at 40 SPS. */
#ifndef PNEU_PRESSURE_FEATURES_H
#define PNEU_PRESSURE_FEATURES_H
#include <stdbool.h>
#include <stdint.h>

#define PNEU_FEATURE_COUNT 12U
#define PNEU_HISTORY_COUNT 224U
#define PNEU_BASELINE_COUNT 80U

/* Keep this order identical to the PC validation/export code. */
typedef enum {
    PNEU_F_RISE_MS, PNEU_F_DECAY_MS, PNEU_F_WIDTH_MS, PNEU_F_AREA_WIDTH_MS,
    PNEU_F_SLOPE_NORM_S, PNEU_F_ASYMMETRY, PNEU_F_RELEASE_RISE_MS,
    PNEU_F_RECOVERY_MS, PNEU_F_RELEASE_WIDTH_MS, PNEU_F_RELEASE_AREA_WIDTH_MS,
    PNEU_F_TROUGH_PEAK_RATIO, PNEU_F_AREA_RATIO
} pneu_feature_index_t;

/* Demo labels are NOT inputs to the event detector. */
typedef enum {
    PNEU_CLASS_UNKNOWN = 0, PNEU_CLASS_TAIL = 1, PNEU_CLASS_BACK = 2,
    PNEU_CLASS_LEGS = 3, PNEU_CLASS_HEAD = 4
} pneu_demo_class_t;

typedef enum {
    PNEU_EVENT_NONE, PNEU_EVENT_START, PNEU_EVENT_COMPLETE,
    PNEU_EVENT_INVALID, PNEU_EVENT_TIMEOUT, PNEU_EVENT_RESET
} pneu_event_result_t;

/* Detection thresholds in counts relative to the baseline (docs/model-store.md, 物体プロファイル):
   trigger starts an event (and marks the release), quiet tracks the baseline and ends it,
   min_peak/min_trough reject weak events at extraction. */
typedef struct { float trigger, quiet, min_peak, min_trough; } pneu_detect_t;
extern const pneu_detect_t PNEU_DETECT_DINO; /* 30000, 25000, 25000, 30000 */
extern const pneu_detect_t PNEU_DETECT_KRAG; /*  7000,  5000,  6000,  6000 */

typedef struct { uint32_t ms; int32_t raw; } pneu_history_sample_t;
typedef struct {
    uint32_t id, trigger_ms, end_ms;
    float baseline, peak, trough;
    float value[PNEU_FEATURE_COUNT];
} pneu_feature_event_t;

typedef struct {
    pneu_history_sample_t history[PNEU_HISTORY_COUNT];
    int32_t quiet[PNEU_BASELINE_COUNT], scratch[PNEU_BASELINE_COUNT];
    uint16_t head, count, quiet_head, quiet_count;
    uint8_t consecutive_high, consecutive_quiet;
    bool ready, active, negative_seen, have_previous;
    uint32_t previous_ms, previous_seq, first_high_ms, next_id;
    float detection_baseline;
    pneu_detect_t detect; /* DINO after init; reset keeps it */
    pneu_feature_event_t event;
} pneu_features_t;

void pneu_features_init(pneu_features_t *state);
/* Clears partial data; preserves the event ID (so old results stay distinguishable)
   and the detection profile. */
void pneu_features_reset(pneu_features_t *state);
/* Takes effect at the next sample; no reset. */
void pneu_features_set_detection(pneu_features_t *state, const pneu_detect_t *detect);
pneu_event_result_t pneu_features_feed(pneu_features_t *state,
                                     uint32_t seq, uint32_t ms, int32_t raw);
pneu_demo_class_t pneu_demo_class_for_zone(uint8_t zone);
#endif
