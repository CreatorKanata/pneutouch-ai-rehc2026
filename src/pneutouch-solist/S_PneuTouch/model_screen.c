/* Screening: the hardware learner itself judges each candidate subset, because
   cheap PC proxies (LDA scatter, nearest centroid) do not predict the hard-sigmoid
   ELM. A small training set keeps the ~100 candidates x 2 passes near a minute. */
#include "model_screen.h"
#include "model_tune.h"
#include "model_core.h"
#include "board.h"
#include <string.h>

static uint8_t bitmaps[PNEU_SCREEN_PASSES][PNEU_SCREEN_BITMAP];

uint16_t pneu_screen_ordinal(uint16_t n_c, uint8_t pass, uint8_t j)
{
    uint32_t offset;
    if (n_c <= PNEU_SCREEN_PER_CLASS) return j;
    /* Rounded up so pass 1 differs from pass 0 even for 11..20 records. */
    offset = ((uint32_t)n_c+2U*PNEU_SCREEN_PER_CLASS-1U)/(2U*PNEU_SCREEN_PER_CLASS)*pass;
    return (uint16_t)(offset+(uint32_t)j*n_c/PNEU_SCREEN_PER_CLASS);
}

/* Labels are within the object's classes: the statistics pass checked them first. */
static uint8_t count_labels(uint8_t slot, uint16_t n, uint16_t count[PNEU_CLASS_COUNT])
{
    float pool[PNEU_POOL_COUNT];
    uint8_t label;
    uint16_t i;
    for (i = 0; i < n; ++i) {
        board_watchdog();
        if (!pneu_store_get(slot, i, &label, pool)) return PNEU_MODEL_ERR_RECORD;
        ++count[label-1U];
    }
    return PNEU_MODEL_ERR_NONE;
}

uint8_t pneu_screen_build(uint8_t slot, uint16_t n, uint8_t pass, uint8_t chosen[PNEU_SCREEN_BITMAP],
                          uint16_t *used)
{
    float pool[PNEU_POOL_COUNT];
    uint16_t count[PNEU_CLASS_COUNT] = {0}, seen[PNEU_CLASS_COUNT] = {0}, i;
    uint8_t next[PNEU_CLASS_COUNT] = {0}, label, error;
    memset(chosen, 0, PNEU_SCREEN_BITMAP);
    *used = 0;
    if ((error = count_labels(slot, n, count)) != PNEU_MODEL_ERR_NONE) return error;
    for (i = 0; i < n; ++i) {
        unsigned c;
        board_watchdog();
        if (!pneu_store_get(slot, i, &label, pool)) return PNEU_MODEL_ERR_RECORD;
        c = label-1U;
        if (next[c] < PNEU_SCREEN_PER_CLASS && next[c] < count[c] &&
            seen[c] == pneu_screen_ordinal(count[c], pass, next[c])) {
            chosen[i >> 3] = (uint8_t)(chosen[i >> 3] | (1U << (i & 7U)));
            ++next[c]; ++*used;
        }
        ++seen[c];
    }
    return PNEU_MODEL_ERR_NONE;
}

uint8_t pneu_mask_size(uint16_t mask)
{
    uint8_t n = 0;
    for (; mask; mask = (uint16_t)(mask & (mask-1U))) ++n;
    return n;
}

uint16_t pneu_screen_count(uint8_t size)
{
    uint16_t n = 0;
    uint8_t bits = 0;
    while (pneu_screen_next(size, &bits)) ++n;
    return n;
}

bool pneu_screen_next(uint8_t size, uint8_t *bits)
{
    unsigned limit = 1U << size, b = *bits, want = b ? pneu_mask_size(b) : PNEU_TUNE_MIN_SIZE;
    if (size > PNEU_TUNE_SHORTLIST) return false;
    for (;;) {
        for (++b; b < limit; ++b)
            if (pneu_mask_size((uint16_t)b) == want) { *bits = (uint8_t)b; return true; }
        if (++want > size) return false;
        b = 0;
    }
}

uint16_t pneu_screen_pool_mask(uint8_t bits, const uint8_t shortlist[])
{
    uint16_t mask = 0;
    unsigned i;
    for (i = 0; i < PNEU_TUNE_SHORTLIST; ++i)
        if (bits & (1U << i)) mask = (uint16_t)(mask | (1U << shortlist[i]));
    return mask;
}

bool pneu_tune_better(uint16_t hits_a, uint16_t mask_a, uint16_t hits_b, uint16_t mask_b)
{
    uint8_t size_a = pneu_mask_size(mask_a), size_b = pneu_mask_size(mask_b);
    if (hits_a != hits_b) return hits_a > hits_b;
    if (size_a != size_b) return size_a < size_b;
    return mask_a < mask_b;
}

void pneu_screen_keep(volatile uint16_t masks[], volatile uint16_t scores[], uint16_t mask, uint16_t score)
{
    unsigned at = PNEU_TUNE_FINALISTS, i;
    for (i = 0; i < PNEU_TUNE_FINALISTS && at == PNEU_TUNE_FINALISTS; ++i)
        if (!masks[i] || pneu_tune_better(score, mask, scores[i], masks[i])) at = i;
    if (at == PNEU_TUNE_FINALISTS) return;
    for (i = PNEU_TUNE_FINALISTS-1U; i > at; --i) { masks[i] = masks[i-1U]; scores[i] = scores[i-1U]; }
    masks[at] = mask; scores[at] = score;
}

/* Hits on every record outside the training bitmap, through the live prediction path. */
static uint8_t judge(uint8_t slot, uint16_t n, const uint8_t *chosen, uint16_t *hits)
{
    float pool[PNEU_POOL_COUNT];
    uint8_t label;
    uint16_t i;
    for (i = 0; i < n; ++i) {
        pneu_demo_class_t got;
        if (pneu_core_chosen(chosen, i)) continue;
        board_watchdog();
        if (!pneu_store_get(slot, i, &label, pool)) return PNEU_MODEL_ERR_RECORD;
        got = pneu_core_classify(pool);
        if (got == PNEU_CLASS_UNKNOWN) return PNEU_MODEL_ERR_AI;
        if ((unsigned)got == label) ++*hits;
    }
    return PNEU_MODEL_ERR_NONE;
}

/* Both passes of one candidate; *done counts finished candidate x pass trainings. */
static uint8_t screen_one(uint8_t slot, uint16_t n, uint16_t mask, const uint16_t used[],
                          uint16_t *done, uint16_t total)
{
    uint16_t score = 0;
    uint8_t pass, error = pneu_core_choose_mask(mask);
    for (pass = 0; !error && pass < PNEU_SCREEN_PASSES; ++pass) {
        error = pneu_core_train_chosen(slot, n, bitmaps[pass], used[pass]);
        if (!error) error = judge(slot, n, bitmaps[pass], &score);
        if (!error) pneu_core_progress(PNEU_PHASE_TUNE, ++*done, total);
    }
    if (!error) pneu_screen_keep(pneutouch_tune.finalist_mask, pneutouch_tune.finalist_screen, mask, score);
    return error;
}

uint8_t pneu_screen_run(uint8_t slot, uint16_t n, const uint8_t shortlist[], uint8_t size)
{
    uint16_t used[PNEU_SCREEN_PASSES], done = 0, total;
    uint8_t pass, bits = 0, error = PNEU_MODEL_ERR_NONE;
    for (pass = 0; !error && pass < PNEU_SCREEN_PASSES; ++pass)
        error = pneu_screen_build(slot, n, pass, bitmaps[pass], &used[pass]);
    if (error) return error;
    pneutouch_tune.candidates = pneu_screen_count(size);
    pneutouch_tune.screened = (uint16_t)(n-used[0]);
    total = (uint16_t)(pneutouch_tune.candidates*PNEU_SCREEN_PASSES);
    pneu_core_progress(PNEU_PHASE_TUNE, 0, total);
    while (!error && pneu_screen_next(size, &bits))
        error = screen_one(slot, n, pneu_screen_pool_mask(bits, shortlist), used, &done, total);
    return error;
}
