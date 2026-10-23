/* Subset tuning: the Fisher-auto selection is the champion; stage 1
   (model_screen.c) screens every subset of the Fisher shortlist, stage 2 runs the
   4-fold cross-validation (statistics from the training folds only) on the
   champion and the best screened subsets. A challenger replaces the champion
   only when it wins by more than PNEU_TUNE_MARGIN_PCT of the records. */
#include "model_tune.h"
#include "model_screen.h"
#include "model_core.h"
#include "model_store.h"
#include "board.h"
#include "smpl_common.h"
#include "irq.h"
#include <string.h>

volatile pneu_tune_status_t pneutouch_tune;

typedef struct {
    uint16_t mask, correct, confusion[PNEU_CLASS_COUNT][PNEU_CLASS_COUNT];
} tune_score_t;

/* The champion and the best challenger so far (only these two confusions are kept). */
static tune_score_t champion, challenger;

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

/* All-record statistics, the champion (Fisher auto) and the shortlist of the best
   ranked features with a nonzero score (a nonzero score implies a usable scale). */
static uint8_t shortlist_of(uint8_t slot, uint16_t n, uint8_t shortlist[PNEU_TUNE_SHORTLIST], uint8_t *size)
{
    float fisher[PNEU_POOL_COUNT];
    uint8_t order[PNEU_POOL_COUNT], error;
    unsigned i;
    *size = 0;
    error = pneu_core_load_stats(slot, n, PNEU_NO_FOLD);
    if (!error) error = pneu_core_choose(0);
    if (error) return error;
    pneutouch_tune.champion_mask = pneutouch_model.mask;
    for (i = 0; i < PNEU_POOL_COUNT; ++i) fisher[i] = pneutouch_model.fisher[i];
    pneu_rank(fisher, order);
    for (i = 0; i < PNEU_POOL_COUNT && *size < PNEU_TUNE_SHORTLIST; ++i)
        if (fisher[order[i]] > 0.0f) shortlist[(*size)++] = order[i];
    return *size < PNEU_TUNE_MIN_SIZE ? PNEU_MODEL_ERR_SELECT : PNEU_MODEL_ERR_NONE;
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

static uint8_t cross_validate(uint8_t slot, uint16_t n, uint16_t mask, tune_score_t *score)
{
    uint8_t fold, error = PNEU_MODEL_ERR_NONE;
    *score = (tune_score_t){0};
    score->mask = mask;
    for (fold = 0; !error && fold < PNEU_TUNE_FOLDS; ++fold) {
        error = pneu_core_load_stats(slot, n, fold);
        if (!error) error = pneu_core_choose_mask(mask);
        if (!error) error = pneu_core_train(slot, n, fold);
        if (!error) error = predict_fold(slot, n, fold, score);
    }
    return error;
}

/* Finals: champion + finalists not equal to it, best challenger kept with its confusion. */
static uint8_t finals(uint8_t slot, uint16_t n)
{
    tune_score_t score;
    uint16_t done = 0, total = 1;
    uint8_t f, error;
    for (f = 0; f < PNEU_TUNE_FINALISTS; ++f)
        if (pneutouch_tune.finalist_mask[f] && pneutouch_tune.finalist_mask[f] != pneutouch_tune.champion_mask) ++total;
    challenger = (tune_score_t){0};
    pneu_core_progress(PNEU_PHASE_FINAL, 0, total);
    if ((error = cross_validate(slot, n, pneutouch_tune.champion_mask, &champion)) != PNEU_MODEL_ERR_NONE) return error;
    pneutouch_tune.champion_correct = champion.correct;
    pneu_core_progress(PNEU_PHASE_FINAL, ++done, total);
    for (f = 0; f < PNEU_TUNE_FINALISTS && pneutouch_tune.finalist_mask[f]; ++f) {
        uint16_t mask = pneutouch_tune.finalist_mask[f];
        if (mask == pneutouch_tune.champion_mask) { pneutouch_tune.finalist_correct[f] = champion.correct; continue; }
        if ((error = cross_validate(slot, n, mask, &score)) != PNEU_MODEL_ERR_NONE) return error;
        pneutouch_tune.finalist_correct[f] = score.correct;
        if (!challenger.mask || pneu_tune_better(score.correct, mask, challenger.correct, challenger.mask))
            challenger = score;
        pneu_core_progress(PNEU_PHASE_FINAL, ++done, total);
    }
    return PNEU_MODEL_ERR_NONE;
}

/* Rounded PNEU_TUNE_MARGIN_PCT of the records, at least 1. */
static uint16_t margin(uint16_t count)
{
    uint32_t m = ((uint32_t)count*PNEU_TUNE_MARGIN_PCT+50U)/100U;
    return (uint16_t)(m ? m : 1U);
}

/* Chosen result, its rounded recall per class and the advice. */
static void decide(uint8_t recall[4])
{
    const tune_score_t *chosen = challenger.mask &&
        (uint32_t)challenger.correct > (uint32_t)champion.correct+margin(pneutouch_tune.count) ? &challenger : &champion;
    uint16_t counts[PNEU_CLASS_COUNT];
    unsigned t, p;
    pneutouch_tune.mask = chosen->mask;
    pneutouch_tune.best_k = pneu_mask_size(chosen->mask);
    pneutouch_tune.correct = chosen->correct;
    for (t = 0; t < PNEU_CLASS_COUNT; ++t) {
        uint32_t row = 0;
        for (p = 0; p < PNEU_CLASS_COUNT; ++p) {
            pneutouch_tune.confusion[t][p] = chosen->confusion[t][p];
            row += chosen->confusion[t][p];
        }
        counts[t] = (uint16_t)row;
        recall[t] = row ? (uint8_t)((100U*chosen->confusion[t][t]+row/2U)/row) : (uint8_t)PNEU_RECALL_ABSENT;
        pneutouch_tune.recall[t] = recall[t];
    }
    pneutouch_tune.advice = pneu_model_advice(recall, counts);
}

static uint8_t search(uint8_t slot, uint16_t n)
{
    uint8_t shortlist[PNEU_TUNE_SHORTLIST] = {0}, size, error;
    error = shortlist_of(slot, n, shortlist, &size);
    if (!error) error = pneu_screen_run(slot, n, shortlist, size);
    if (!error) error = finals(slot, n);
    return error;
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
        decide(recall);
        if (!pneu_store_set_tuning(pneutouch_tune.slot, pneutouch_tune.mask, recall)) error = PNEU_MODEL_ERR_STORE;
    }
    pneutouch_tune.error = error;
    pneutouch_tune.done = error == PNEU_MODEL_ERR_NONE;
    if (pneutouch_tune.done) pneutouch_tune.elapsed_us = board_micros()-start;
    return pneutouch_tune.done;
}
