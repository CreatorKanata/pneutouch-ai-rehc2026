/* k tuning: for every candidate k and fold, select features from the training
   records only, train instance 0 and predict the held-out records on the chip.
   The k with the most held-out hits is stored in the slot header; a larger k
   must win by more than PNEU_TUNE_MARGIN_PCT of the records (ties, noise: lower k). */
#include "model_tune.h"
#include "model_core.h"
#include "model_store.h"
#include "board.h"
#include "smpl_common.h"
#include "irq.h"

#define TUNE_TRAININGS ((PNEU_TUNE_K_MAX-PNEU_TUNE_K_MIN+1U)*PNEU_TUNE_FOLDS)

volatile pneu_tune_status_t pneutouch_tune;

typedef struct {
    uint16_t correct, confusion[PNEU_CLASS_COUNT][PNEU_CLASS_COUNT];
} tune_score_t;

uint8_t pneu_model_advice(const uint8_t recall[4], const uint16_t counts[4])
{
    unsigned c, best = PNEU_CLASS_COUNT;
    for (c = 0; c < PNEU_CLASS_COUNT; ++c) {
        if (recall[c] == PNEU_RECALL_ABSENT) continue;
        /* Ascending labels with strict comparisons: a full tie keeps the lower label. */
        if (best == PNEU_CLASS_COUNT || recall[c] < recall[best] ||
            (recall[c] == recall[best] && counts[c] < counts[best])) best = c;
    }
    return best < PNEU_CLASS_COUNT && recall[best] < PNEU_ADVICE_RECALL_PCT ? (uint8_t)(best+1U) : 0U;
}

static uint8_t open_slot(uint16_t *n)
{
    pneu_store_slot_t info;
    if (!pneu_store_init()) return PNEU_MODEL_ERR_STORE;
    pneutouch_tune.slot = pneu_store_active();
    if (!pneu_store_slot_info(pneutouch_tune.slot, &info) || info.state != PNEU_SLOT_COMMITTED ||
        info.count < PNEU_TUNE_MIN_RECORDS || info.count > PNEU_STORE_MAX_RECORDS) return PNEU_MODEL_ERR_NO_SLOT;
    pneutouch_tune.count = info.count;
    *n = info.count;
    return PNEU_MODEL_ERR_NONE;
}

/* Held-out records of fold, ascending, through the live prediction path. */
static uint8_t predict_fold(uint8_t slot, uint16_t n, uint8_t fold, tune_score_t *score)
{
    float pool[PNEU_POOL_COUNT];
    pneu_demo_class_t got;
    uint8_t label;
    uint16_t i;
    for (i = fold; i < n; i = (uint16_t)(i+PNEU_TUNE_FOLDS)) {
        board_watchdog();
        if (!pneu_store_get(slot, i, &label, pool)) return PNEU_MODEL_ERR_RECORD;
        got = pneu_core_classify(pool);
        if (got == PNEU_CLASS_UNKNOWN) return PNEU_MODEL_ERR_AI;
        ++score->confusion[label-1U][got-1U];
        if ((unsigned)got == label) ++score->correct;
    }
    return PNEU_MODEL_ERR_NONE;
}

static uint8_t run_fold(uint8_t slot, uint16_t n, uint8_t k, uint8_t fold, tune_score_t *score)
{
    uint8_t error = pneu_core_load_stats(slot, n, fold);
    if (!error) error = pneu_core_choose(k);
    if (!error) error = pneu_core_train(slot, n, fold);
    if (!error) error = predict_fold(slot, n, fold, score);
    return error;
}

/* Rounded PNEU_TUNE_MARGIN_PCT of the records, at least 1. */
static uint16_t margin(uint16_t count)
{
    uint32_t m = ((uint32_t)count*PNEU_TUNE_MARGIN_PCT+50U)/100U;
    return (uint16_t)(m ? m : 1U);
}

static void keep_best(uint8_t k, const tune_score_t *score, uint16_t *best)
{
    unsigned t, p;
    pneutouch_tune.correct[k] = score->correct;
    /* A one-record lead of a larger k is noise: it must beat the best by more than the margin. */
    if (k != PNEU_TUNE_K_MIN && (uint32_t)score->correct <= (uint32_t)*best+margin(pneutouch_tune.count)) return;
    *best = score->correct;
    pneutouch_tune.best_k = k;
    for (t = 0; t < PNEU_CLASS_COUNT; ++t)
        for (p = 0; p < PNEU_CLASS_COUNT; ++p) pneutouch_tune.confusion[t][p] = score->confusion[t][p];
}

static uint8_t search(uint8_t slot, uint16_t n)
{
    tune_score_t score;
    uint16_t best = 0, done = 0;
    uint8_t k, fold, error;
    pneu_core_progress(PNEU_PHASE_TUNE, 0, TUNE_TRAININGS);
    for (k = PNEU_TUNE_K_MIN; k <= PNEU_TUNE_K_MAX; ++k) {
        score = (tune_score_t){0};
        for (fold = 0; fold < PNEU_TUNE_FOLDS; ++fold) {
            if ((error = run_fold(slot, n, k, fold, &score)) != PNEU_MODEL_ERR_NONE) return error;
            pneu_core_progress(PNEU_PHASE_TUNE, ++done, TUNE_TRAININGS);
        }
        keep_best(k, &score, &best);
    }
    return PNEU_MODEL_ERR_NONE;
}

/* Rounded held-out recall per class of the best k, then the advice. */
static void finish(uint8_t recall[4])
{
    uint16_t counts[PNEU_CLASS_COUNT];
    unsigned t, p;
    for (t = 0; t < PNEU_CLASS_COUNT; ++t) {
        uint32_t row = 0;
        for (p = 0; p < PNEU_CLASS_COUNT; ++p) row += pneutouch_tune.confusion[t][p];
        counts[t] = (uint16_t)row;
        recall[t] = row ? (uint8_t)((100U*pneutouch_tune.confusion[t][t]+row/2U)/row) : (uint8_t)PNEU_RECALL_ABSENT;
        pneutouch_tune.recall[t] = recall[t];
    }
    pneutouch_tune.advice = pneu_model_advice(recall, counts);
}

bool pneu_model_tune(void)
{
    uint32_t start = board_micros();
    uint8_t recall[PNEU_CLASS_COUNT], error, c;
    uint16_t n = 0;
    pneutouch_tune = (pneu_tune_status_t){0};
    for (c = 0; c < PNEU_CLASS_COUNT; ++c) pneutouch_tune.recall[c] = PNEU_RECALL_ABSENT;
    pneutouch_model.ready = false;
    pneutouch_model.error = PNEU_MODEL_ERR_NONE;
    irq_ai_dis(); smpl_enablePeripheral(AI_PERI);
    error = open_slot(&n);
    if (!error) error = search(pneutouch_tune.slot, n);
    if (!error) {
        finish(recall);
        if (!pneu_store_set_tuning(pneutouch_tune.slot, pneutouch_tune.best_k, recall)) error = PNEU_MODEL_ERR_STORE;
    }
    pneutouch_tune.error = error;
    pneutouch_tune.done = error == PNEU_MODEL_ERR_NONE;
    if (pneutouch_tune.done) pneutouch_tune.elapsed_us = board_micros()-start;
    return pneutouch_tune.done;
}
