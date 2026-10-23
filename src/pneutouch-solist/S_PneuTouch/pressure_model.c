/* Boot-time training from the active FeRAM slot. The chip ranks the 14 pool
   features itself, so no training data or normalization is compiled in.
   Every live prediction uses the current HX710B event, never a stored record.
   The pneu_core_* pieces are shared with the k tuning in model_tune.c. */
#include "pressure_model.h"
#include "model_core.h"
#include "model_store.h"
#include "model_tune.h"
#include "board.h"
#include "solistAi.h"
#include "smpl_common.h"
#include "irq.h"
#include <string.h>

#define PNEU_MODEL_HIDDEN 32U
#define ALL_CLASSES 0x0FU
#define SHAPE_LIMIT 100000.0f
#define AMPLITUDE_LIMIT 100000000.0f

volatile pneu_model_status_t pneutouch_model;
static bfloat16 input[12], teacher[4], output[4], reset_row[64];
static pneu_stats_t stats;
static pneu_selection_t selection;
static pneu_model_progress_t progress;

void pneu_model_set_progress(pneu_model_progress_t callback) { progress = callback; }

void pneu_core_progress(uint8_t phase, uint16_t done, uint16_t total)
{
    if (progress) progress(phase, done, total);
}

/* Phase start (done 0), every 8 records/steps and the phase end. */
static void report(uint8_t phase, uint16_t done, uint16_t total)
{
    if (done % 8U == 0U || done == total) pneu_core_progress(phase, done, total);
}

bool pneu_core_held(uint16_t index, uint8_t fold)
{
    return fold != PNEU_NO_FOLD && index % PNEU_TUNE_FOLDS == fold;
}

/* Records of [0, n) that fold holds out. */
static uint16_t held_count(uint16_t n, uint8_t fold)
{
    return fold == PNEU_NO_FOLD ? 0U : (uint16_t)((n+PNEU_TUNE_FOLDS-1U-fold)/PNEU_TUNE_FOLDS);
}

static bool wait_ai(void)
{
    uint32_t started = board_micros();
    while (ODL_IsBusy()) {
        board_watchdog();
        if (board_micros()-started >= 20000U) {
            pneutouch_model.ready = false; ++pneutouch_model.failures;
            return false;
        }
    }
    return true;
}

/* Explicit ridge start (Beta=0, P=I) as in the PAI1 validation path. */
static void reset_instance(void)
{
    ODL_Parameters p = {0};
    unsigned i;
    p.inputSize = 12; p.hiddenSize = PNEU_MODEL_HIDDEN; p.outputSize = 4; p.seed = 1;
    p.forgettingFactor = p.scaleAlpha = p.leakRate = (bfloat16)0x3f80;
    p.activationFunction = ODL_ACTV_SIGMOID; p.lossFunction = ODL_LOSS_MSE;
    ODL_Initialize(0, &p); ODL_Reset(0);
    memset(reset_row, 0, sizeof(reset_row));
    for (i = 0; i < PNEU_MODEL_HIDDEN; ++i) ODL_SetWeightBeta(reset_row, 0, i*8U, 8);
    for (i = 0; i < PNEU_MODEL_HIDDEN; ++i) {
        if (i) reset_row[i-1] = 0;
        reset_row[i] = (bfloat16)0x3f80;
        ODL_SetWeightP(reset_row, 0, i*PNEU_MODEL_HIDDEN*2U, (uint16_t)(PNEU_MODEL_HIDDEN*2U));
    }
    irq_ai_clearIRQ();
}

static uint8_t train_step(uint8_t label, const float pool[PNEU_POOL_COUNT])
{
    uint16_t words[12];
    unsigned j;
    if (!pneu_select_words(&selection, pool, words)) return PNEU_MODEL_ERR_RECORD;
    for (j = 0; j < 12; ++j) input[j] = (bfloat16)words[j];
    for (j = 0; j < 4; ++j) teacher[j] = j+1U == label ? (bfloat16)0x3f80 : 0;
    board_watchdog();
    ODL_StartTrain(0, input, teacher);
    return wait_ai() ? PNEU_MODEL_ERR_NONE : PNEU_MODEL_ERR_AI;
}

bool pneu_core_chosen(const uint8_t *chosen, uint16_t index)
{
    return (chosen[index >> 3] & (1U << (index & 7U))) != 0U;
}

/* Training record: not held out by fold and, with a bitmap, inside it. */
static bool trains(uint16_t index, uint8_t fold, const uint8_t *chosen)
{
    return !pneu_core_held(index, fold) && (!chosen || pneu_core_chosen(chosen, index));
}

/* Records are re-read from FeRAM in a fixed stride order: no RAM copy.
   used = training records per epoch (for the progress total). */
static uint8_t train_records(uint8_t slot, uint16_t n, uint8_t fold, const uint8_t *chosen, uint16_t used)
{
    float pool[PNEU_POOL_COUNT];
    uint16_t total = (uint16_t)(PNEU_TRAIN_EPOCHS*used), steps = 0, index;
    uint8_t label, error;
    unsigned e, i;
    reset_instance();
    report(PNEU_PHASE_TRAIN, 0, total);
    for (e = 0; e < PNEU_TRAIN_EPOCHS; ++e)
        for (i = 0; i < n; ++i) {
            index = pneu_train_index(n, (uint8_t)e, (uint16_t)i);
            if (!trains(index, fold, chosen)) continue;
            if (!pneu_store_get(slot, index, &label, pool)) return PNEU_MODEL_ERR_RECORD;
            if ((error = train_step(label, pool)) != PNEU_MODEL_ERR_NONE) return error;
            report(PNEU_PHASE_TRAIN, ++steps, total);
            pneutouch_model.training_steps = steps;
        }
    return PNEU_MODEL_ERR_NONE;
}

uint8_t pneu_core_train(uint8_t slot, uint16_t n, uint8_t fold)
{
    return train_records(slot, n, fold, 0, (uint16_t)(n-held_count(n, fold)));
}

uint8_t pneu_core_train_chosen(uint8_t slot, uint16_t n, const uint8_t *chosen, uint16_t used)
{
    return train_records(slot, n, PNEU_NO_FOLD, chosen, used);
}

uint8_t pneu_core_load_stats(uint8_t slot, uint16_t n, uint8_t fold)
{
    float pool[PNEU_POOL_COUNT];
    uint8_t label;
    uint16_t i, done = 0, total = (uint16_t)(n-held_count(n, fold));
    pneu_stats_reset(&stats);
    report(PNEU_PHASE_STATS, 0, total);
    for (i = 0; i < n; ++i) {
        if (pneu_core_held(i, fold)) continue;
        board_watchdog();
        if (!pneu_store_get(slot, i, &label, pool) || !pneu_stats_add(&stats, label, pool))
            return PNEU_MODEL_ERR_RECORD;
        report(PNEU_PHASE_STATS, ++done, total);
    }
    return PNEU_MODEL_ERR_NONE;
}

/* lcd_line2 is published as soon as the selection is known (shown while training). */
static void publish_line2(bool blank)
{
    char line[17];
    unsigned i;
    if (blank) { memset(line, ' ', 16); line[16] = '\0'; }
    else pneu_model_line2(&pneutouch_model, line); /* no 156-byte stack copy */
    for (i = 0; i < sizeof(line); ++i) pneutouch_model.lcd_line2[i] = line[i];
}

static uint8_t publish_selection(bool ok)
{
    unsigned i;
    if (!ok) return PNEU_MODEL_ERR_SELECT;
    pneutouch_model.selected_count = selection.count;
    pneutouch_model.mask = pneu_selection_mask(&selection);
    for (i = 0; i < selection.count; ++i) pneutouch_model.selected[i] = selection.index[i];
    for (i = 0; i < PNEU_POOL_COUNT; ++i) pneutouch_model.fisher[i] = selection.fisher[i];
    publish_line2(false);
    return PNEU_MODEL_ERR_NONE;
}

/* class_mask stays a parameter of pneu_select; the firmware uses all four. */
uint8_t pneu_core_choose(uint8_t k) { return publish_selection(pneu_select(&stats, ALL_CLASSES, k, &selection)); }

uint8_t pneu_core_choose_mask(uint16_t mask)
{
    return publish_selection(pneu_select_mask(&stats, ALL_CLASSES, mask, &selection));
}

/* Explicit header k, then the TUNE mask, then a version 1 tuned_k, then Fisher auto. */
static uint8_t choose_from_header(const pneu_store_slot_t *info)
{
    if (info->k) return pneu_core_choose(info->k);
    if (info->mask) return pneu_core_choose_mask(info->mask);
    return pneu_core_choose(info->tuned_k);
}

/* Tuning result of the header and the class counts of the statistics pass. */
static void publish_tuning(const pneu_store_slot_t *info)
{
    unsigned c;
    pneutouch_model.tuned_k = info->tuned_k;
    for (c = 0; c < PNEU_CLASS_COUNT; ++c) {
        pneutouch_model.recall[c] = info->recall[c];
        pneutouch_model.class_count[c] = stats.n[c];
    }
    pneutouch_model.advice = info->tuned_k ? pneu_model_advice(info->recall, stats.n) : 0U;
}

static uint8_t prepare(pneu_store_slot_t *info)
{
    uint8_t slot, error;
    if (!pneu_store_init()) return PNEU_MODEL_ERR_STORE;
    slot = pneu_store_active();
    pneutouch_model.slot = slot;
    if (!pneu_store_slot_info(slot, info) || info->state != PNEU_SLOT_COMMITTED ||
        info->count < 4U || info->count > PNEU_STORE_MAX_RECORDS) return PNEU_MODEL_ERR_NO_SLOT;
    pneutouch_model.store_count = info->count;
    pneutouch_model.training_examples = info->count;
    error = pneu_core_load_stats(slot, info->count, PNEU_NO_FOLD);
    if (!error) publish_tuning(info);
    return error;
}

bool pneu_model_init(void)
{
    uint32_t start = board_micros();
    pneu_store_slot_t info;
    uint8_t error;
    pneutouch_model = (pneu_model_status_t){0};
    irq_ai_dis(); smpl_enablePeripheral(AI_PERI);
    error = prepare(&info);
    if (!error) error = choose_from_header(&info);
    if (!error) error = pneu_core_train(pneutouch_model.slot, info.count, PNEU_NO_FOLD);
    pneutouch_model.error = error;
    pneutouch_model.ready = error == PNEU_MODEL_ERR_NONE;
    if (pneutouch_model.ready) pneutouch_model.training_us = board_micros()-start;
    publish_line2(!pneutouch_model.ready);
    return pneutouch_model.ready;
}

void pneu_model_line2(const volatile pneu_model_status_t *status, char out[17])
{
    unsigned i, count = status->selected_count;
    memset(out, ' ', 16); out[16] = '\0';
    if (!count) return;
    for (i = 0; i < count && i < 12U; ++i)
        if (status->selected[i] < PNEU_POOL_COUNT) out[status->selected[i]] = (char)0xff;
    out[14] = count >= 10U ? (char)('0'+count/10U) : 'k'; /* "k4"; 10..12 have no room for k */
    out[15] = (char)('0'+count%10U);
}

static pneu_demo_class_t predict_instance(const uint16_t words[12])
{
    uint32_t bits;
    float best = 0, value;
    unsigned i, winner = 0;
    for (i = 0; i < 12; ++i) {
        input[i] = (bfloat16)words[i]; pneutouch_model.input[i] = words[i];
    }
    memset(teacher, 0, sizeof(teacher));
    ODL_StartPredict(0, input, teacher);
    if (!wait_ai()) return PNEU_CLASS_UNKNOWN;
    ODL_GetResult(0, output);
    for (i = 0; i < 4; ++i) {
        pneutouch_model.scores[i] = (uint16_t)output[i];
        bits = (uint32_t)(uint16_t)output[i] << 16;
        if ((bits & 0x7f800000U) == 0x7f800000U) {
            ++pneutouch_model.failures; return PNEU_CLASS_UNKNOWN;
        }
        memcpy(&value, &bits, sizeof(value));
        if (!i || value > best) { best = value; winner = i; }
    }
    return (pneu_demo_class_t)(winner+1U);
}

pneu_demo_class_t pneu_core_classify(const float pool[PNEU_POOL_COUNT])
{
    uint16_t words[12];
    if (!pneu_select_words(&selection, pool, words)) return PNEU_CLASS_UNKNOWN;
    return predict_instance(words);
}

static bool amplitude_ok(float counts)
{
    return counts > 0.0f && counts < AMPLITUDE_LIMIT;
}

/* Pool order: 12 event features, positive peak, trough magnitude (counts). */
static bool build_pool(const float values[12], float peak, float trough, float pool[PNEU_POOL_COUNT])
{
    unsigned i;
    for (i = 0; i < 12; ++i) {
        if (!(values[i] > 0.0f && values[i] < SHAPE_LIMIT)) return false;
        pool[i] = values[i];
    }
    pool[12] = peak; pool[13] = trough;
    return amplitude_ok(peak) && amplitude_ok(trough);
}

pneu_demo_class_t pneu_model_predict(const float values[12], float positive_peak_counts,
                                     float trough_counts)
{
    float pool[PNEU_POOL_COUNT];
    uint32_t start = board_micros();
    pneu_demo_class_t label;
    pneutouch_model.label = PNEU_CLASS_UNKNOWN;
    pneutouch_model.inference_us = 0;
    if (!pneutouch_model.ready || !build_pool(values, positive_peak_counts, trough_counts, pool))
        return PNEU_CLASS_UNKNOWN;
    label = pneu_core_classify(pool);
    if (label == PNEU_CLASS_UNKNOWN) return label;
    pneutouch_model.inference_us = board_micros()-start;
    ++pneutouch_model.predictions;
    pneutouch_model.label = label;
    return label;
}

const char *pneu_class_name(pneu_demo_class_t label)
{
    static const char *const names[] = {"UNKNOWN", "TAIL", "BACK", "LEGS", "HEAD"};
    return label >= PNEU_CLASS_TAIL && label <= PNEU_CLASS_HEAD ? names[label] : names[0];
}
