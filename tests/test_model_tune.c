/* Two-stage subset TUNE (model_tune.c + model_screen.c) against the FeRAM stub and a
   scripted Solist-AI: screening sets, candidate order, finalists, finals, decision. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "pressure_model.h"
#include "model_tune.h"
#include "model_screen.h"
#include "model_store.h"
#include "feature_select.h"
#include "feature_select_fixture.h"
#include "solistAi.h"
#include "fram_stub.h"
#include "object_profile.h"

#define RECORDS 48U                /* 12 per class */
#define JUDGED 8U                  /* records outside a screening set */
#define CHAMPION 0x2411U           /* Fisher auto: features 0, 4, 10, 13 */

/* Screening ordinals of a 12-record class: offset 0 / ceil(12/20) = 1, then j*12/10. */
static const uint8_t ordinals[2][10] = {{0, 1, 2, 3, 4, 6, 7, 8, 9, 10}, {1, 2, 3, 4, 5, 7, 8, 9, 10, 11}};
static bool member[2][RECORDS];    /* expected screening training sets */
static pneu_stats_t all_stats, fold_stats[4];
static pneu_selection_t current;   /* selection the firmware should be using */
static unsigned now, trained, inits, final_inits, predicts_in_run, predicts_in_training, judge_cursor, winner;
static int stalled, phase_seen = -1;
static bool in_tune, finals_started, challenger_wins; /* in_tune false: a plain boot training */
static bool krag;                  /* KRAG run: unscripted, the AI answers by a fixed cycle */
static unsigned krag_predicts;
static uint16_t screen_done, final_done, screen_total, final_total;

static void record_pool(unsigned r, float pool[PNEU_POOL_COUNT])
{
    for (unsigned j = 0; j < PNEU_POOL_COUNT; ++j) pool[j] = ldexpf(1.0f, fix_exponents[r%8U][j]);
    pool[1] = ldexpf(1.0f, (int)(r/8U)); /* constant in the fixture: makes every record differ */
}
static uint8_t record_label(unsigned r) { return fix_labels[r%8U]; }

static void build_references(void)
{
    float pool[PNEU_POOL_COUNT];
    pneu_stats_reset(&all_stats);
    for (unsigned f = 0; f < 4; ++f) pneu_stats_reset(&fold_stats[f]);
    for (unsigned r = 0; r < RECORDS; ++r) {
        record_pool(r, pool);
        assert(pneu_stats_add(&all_stats, record_label(r), pool));
        for (unsigned f = 0; f < 4; ++f) if (r%4U != f) assert(pneu_stats_add(&fold_stats[f], record_label(r), pool));
    }
    memset(member, 0, sizeof(member));
    for (unsigned p = 0; p < 2; ++p)
        for (unsigned c = 0; c < 4; ++c)
            for (unsigned j = 0; j < 10; ++j) {
                unsigned o = ordinals[p][j];
                member[p][2U*c+o%2U+8U*(o/2U)] = true; /* class c holds r%8 = 2c, 2c+1 */
            }
}

static void load_slot(uint8_t slot, unsigned count)
{
    float pool[PNEU_POOL_COUNT];
    assert(pneu_store_open(slot, 0, "tune48"));
    for (unsigned r = 0; r < count; ++r) { record_pool(r, pool); assert(pneu_store_put(slot, record_label(r), pool)); }
    assert(pneu_store_commit(slot));
}

static bool words_of(unsigned r, uint16_t words[12])
{
    float pool[PNEU_POOL_COUNT];
    record_pool(r, pool);
    return pneu_select_words(&current, pool, words);
}

/* ---- scripted Solist-AI ---- */
uint32_t board_micros(void) { return now++; }
void board_watchdog(void) {}
void smpl_enablePeripheral(unsigned mask) { assert(mask == 0x40000000U); }
void ODL_Reset(uint8_t model) { assert(model == 0); }
void ODL_SetWeightBeta(const void *v, uint8_t model, uint32_t off, uint16_t size) { (void)v; (void)off; assert(model == 0 && size == 8); }
void ODL_SetWeightP(const void *v, uint8_t model, uint32_t off, uint16_t size) { (void)v; (void)off; assert(model == 0 && size == 64); }
uint32_t ODL_IsBusy(void) { return (uint32_t)stalled; }

static unsigned screen_pass(void) { return (inits-1U)%2U; }
static unsigned final_fold(void) { return (final_inits-1U)%4U; }

void ODL_Initialize(uint8_t model, const ODL_Parameters *p)
{
    assert(model == 0 && p->inputSize == 12 && p->outputSize == 4);
    if (!in_tune || krag) return;
    if (inits && !finals_started) assert(trained == 160U); /* 4 epochs x 40 screening records */
    if (final_inits) assert(trained == 144U);                /* 4 epochs x 36 training-fold records */
    ++inits; trained = 0; judge_cursor = 0; predicts_in_training = 0;
    if (finals_started) {
        ++final_inits;
        if (final_fold() == 0U) predicts_in_run = 0;
        assert(pneu_select_mask(&fold_stats[final_fold()], 0xF, pneutouch_model.mask, &current));
    } else {
        assert(pneu_select_mask(&all_stats, 0xF, pneutouch_model.mask, &current));
    }
}

/* Teacher one-hot of some training record (screening set or training folds) with these words. */
void ODL_StartTrain(uint8_t model, const bfloat16 *x, const bfloat16 *t)
{
    uint16_t words[12];
    bool found = false;
    assert(model == 0);
    if (krag) assert(t[2] == 0 && t[3] == 0 && (t[0] == 0x3f80) != (t[1] == 0x3f80));
    if (!in_tune || krag) return;
    assert(++trained <= 160U);
    for (unsigned r = 0; r < RECORDS && !found; ++r) {
        bool trains = finals_started ? r%4U != final_fold() : member[screen_pass()][r];
        if (!trains || !words_of(r, words) || memcmp(x, words, sizeof(words))) continue;
        for (unsigned i = 0; i < 4; ++i) assert(t[i] == (i+1U == record_label(r) ? 0x3f80 : 0));
        found = true;
    }
    assert(found);
}

/* Screening wrong predictions per pass by subset; everything else is judged right. */
static unsigned screen_wrong(uint16_t mask)
{
    if (mask == 0x0015U || mask == 0x3400U || mask == CHAMPION) return 0; /* 16 of 16 */
    if (mask == 0x0411U || mask == 0x2011U || mask == 0x3C15U) return 1; /* 14 of 16 */
    return 3;
}
/* Cross-validation wrong predictions (of 48) by subset. */
static unsigned final_wrong(uint16_t mask)
{
    switch (mask) {
    case CHAMPION: return 6;                        /* 42 */
    case 0x0015U: return challenger_wins ? 4U : 5U; /* 44: beats 42 + margin 1; 43: does not */
    case 0x0411U: return 5;                         /* 43: ties 0x0015 in run 1, higher mask */
    case 0x3400U: return 8;
    default: return 18;
    }
}

static unsigned next_predicted(void)
{
    if (finals_started) return final_fold()+4U*judge_cursor++;
    while (member[screen_pass()][judge_cursor]) ++judge_cursor;
    return judge_cursor++;
}

void ODL_StartPredict(uint8_t model, const bfloat16 *x, const bfloat16 *t)
{
    uint16_t words[12];
    unsigned r, wrong, truth;
    if (krag) { /* HAT, FACE, LEGS (never answered: the best of HAT/FACE is HAT) */
        winner = krag_predicts++%3U; if (winner == 2U) winner = 3U;
        return;
    }
    r = next_predicted();
    assert(model == 0 && r < RECORDS && (finals_started ? trained == 144U : trained == 160U));
    for (unsigned i = 0; i < 4; ++i) assert(t[i] == 0);
    assert(words_of(r, words) && !memcmp(x, words, sizeof(words)));
    wrong = finals_started ? final_wrong(pneutouch_model.mask) : screen_wrong(pneutouch_model.mask);
    truth = record_label(r)-1U;
    /* Screening: the first `wrong` of each pass; finals: the first `wrong` of the whole CV. */
    winner = (finals_started ? predicts_in_run : predicts_in_training) < wrong ? (truth+1U)%4U : truth;
    ++predicts_in_run; ++predicts_in_training;
}

void ODL_GetResult(uint8_t model, bfloat16 *y)
{
    assert(model == 0);
    for (unsigned i = 0; i < 4; ++i) y[i] = (bfloat16)(i == winner ? 0xbf00 : 0xbf80);
}

/* Phases 0/1 inside the stages; phase 2 counts 0..198 once, then phase 3 counts 0..finals. */
static void on_progress(uint8_t phase, uint16_t done, uint16_t total)
{
    assert(phase <= PNEU_PHASE_FINAL && done <= total);
    if (phase == PNEU_PHASE_TUNE) {
        assert(!finals_started && phase_seen <= (int)PNEU_PHASE_TUNE);
        assert(done == (phase_seen == (int)PNEU_PHASE_TUNE ? screen_done+1U : 0U));
        screen_done = done; screen_total = total; phase_seen = PNEU_PHASE_TUNE;
    } else if (phase == PNEU_PHASE_FINAL) {
        assert(phase_seen == (int)PNEU_PHASE_TUNE || finals_started);
        assert(screen_done == screen_total && done == (finals_started ? final_done+1U : 0U));
        final_done = done; final_total = total; finals_started = true;
    }
}

static bool run_tune(void)
{
    bool ok;
    trained = inits = final_inits = predicts_in_run = 0;
    phase_seen = -1; finals_started = false; screen_done = final_done = screen_total = final_total = 0;
    in_tune = true;
    ok = pneu_model_tune();
    in_tune = finals_started = false;
    return ok;
}

static void header_copy(uint8_t slot, uint8_t out[32]) { memcpy(out, fram+0x2000U+slot*0xF000U, 32); }

/* ---- pure helpers ---- */
static void test_ordinals(void)
{
    for (uint8_t p = 0; p < 2; ++p)
        for (uint8_t j = 0; j < 10; ++j) assert(pneu_screen_ordinal(12, p, j) == ordinals[p][j]);
    for (uint8_t j = 0; j < 7; ++j) assert(pneu_screen_ordinal(7, 1, j) == j); /* <= 10: all records */
    for (uint8_t j = 0; j < 10; ++j) {
        assert(pneu_screen_ordinal(1000, 0, j) == j*100U);
        assert(pneu_screen_ordinal(1000, 1, j) == 50U+j*100U);
        assert(pneu_screen_ordinal(11, 1, j) == 1U+j*11U/10U);
    }
    for (uint16_t n = 11; n <= 1000U; ++n)
        for (uint8_t p = 0; p < 2; ++p) {
            assert(pneu_screen_ordinal(n, p, 9) < n);
            for (uint8_t j = 1; j < 10; ++j) assert(pneu_screen_ordinal(n, p, j) > pneu_screen_ordinal(n, p, j-1U));
        }
}

static void test_enumeration(void)
{
    uint8_t bits = 0, previous = 0;
    unsigned n = 0;
    assert(pneu_screen_count(7) == 99U && pneu_screen_count(3) == 1U && pneu_screen_count(4) == 5U);
    assert(pneu_screen_count(2) == 0U && pneu_screen_count(0) == 0U);
    while (pneu_screen_next(7, &bits)) {
        unsigned size = pneu_mask_size(bits), before = pneu_mask_size(previous);
        assert(size >= 3U && bits < 128U);
        if (n) assert(size > before || (size == before && bits > previous)); /* size, then value */
        if (n == 0) assert(bits == 0x07U);
        if (n == 35) assert(bits == 0x0FU); /* C(7,3) = 35 subsets of 3 come first */
        previous = bits; ++n;
    }
    assert(n == 99U && previous == 0x7FU);
    static const uint8_t shortlist[7] = {0, 4, 10, 13, 12, 11, 2};
    assert(pneu_screen_pool_mask(0x43U, shortlist) == 0x0015U && pneu_screen_pool_mask(0x7FU, shortlist) == 0x3C15U);
}

static void test_keep(void)
{
    uint16_t masks[PNEU_TUNE_FINALISTS] = {0}, scores[PNEU_TUNE_FINALISTS] = {0};
    static const uint16_t want_masks[5] = {0x0007, 0x0070, 0x000F, 0x0003, 0x0300};
    static const uint16_t want_scores[5] = {20, 20, 20, 18, 18};
    pneu_screen_keep(masks, scores, 0x000F, 20); /* 4 features */
    pneu_screen_keep(masks, scores, 0x0070, 20);
    pneu_screen_keep(masks, scores, 0x0300, 18);
    pneu_screen_keep(masks, scores, 0x1C00, 10);
    pneu_screen_keep(masks, scores, 0x0007, 20); /* same score, same size, lower mask: before 0x0070 */
    pneu_screen_keep(masks, scores, 0x0003, 18);
    pneu_screen_keep(masks, scores, 0x0C00, 18); /* full: worse than the last entry, dropped */
    pneu_screen_keep(masks, scores, 0x0001, 5);
    assert(!memcmp(masks, want_masks, sizeof(masks)) && !memcmp(scores, want_scores, sizeof(scores)));
    assert(pneu_tune_better(5, 0x7, 4, 0x1) && pneu_tune_better(5, 0x3, 5, 0x7) && !pneu_tune_better(5, 0x6, 5, 0x5));
}

static void test_screening_sets(void)
{
    uint8_t chosen[PNEU_SCREEN_BITMAP];
    uint16_t used;
    for (uint8_t p = 0; p < 2; ++p) {
        assert(pneu_screen_build(0, RECORDS, p, chosen, &used) == PNEU_MODEL_ERR_NONE && used == 40U);
        for (unsigned r = 0; r < RECORDS; ++r) assert(((chosen[r/8U] >> (r%8U)) & 1U) == member[p][r]);
        for (unsigned r = RECORDS; r < PNEU_STORE_MAX_RECORDS; ++r) assert(!((chosen[r/8U] >> (r%8U)) & 1U));
    }
    for (unsigned c = 0, n = 0; c < 4; ++c, n = 0) { /* exactly 10 per class */
        for (unsigned r = 0; r < RECORDS; ++r) n += member[1][r] && record_label(r) == c+1U;
        assert(n == 10U);
    }
}

/* ---- full runs ---- */
static void check_common(void)
{
    static const uint16_t finalists[5] = {0x0015, 0x3400, CHAMPION, 0x0411, 0x2011};
    static const uint16_t screens[5] = {16, 16, 16, 14, 14};
    assert(pneutouch_tune.done && pneutouch_tune.error == 0 && pneutouch_tune.count == RECORDS);
    assert(pneutouch_tune.candidates == 99U && pneutouch_tune.screened == JUDGED);
    assert(screen_total == 198U && screen_done == 198U && final_total == 5U && final_done == 5U);
    assert(inits == 198U+5U*4U && final_inits == 20U); /* champion + 4 finalists (champion not twice) */
    assert(pneutouch_tune.champion_mask == CHAMPION && pneutouch_tune.champion_correct == 42U);
    for (unsigned f = 0; f < 5; ++f)
        assert(pneutouch_tune.finalist_mask[f] == finalists[f] && pneutouch_tune.finalist_screen[f] == screens[f]);
    assert(pneutouch_tune.finalist_correct[1] == 40U && pneutouch_tune.finalist_correct[2] == 42U);
    assert(pneutouch_tune.finalist_correct[3] == 43U && pneutouch_tune.finalist_correct[4] == 30U);
    assert(!pneutouch_model.ready);
}

static void check_header(uint16_t mask, uint8_t k, const uint8_t recall[4])
{
    pneu_store_slot_t info;
    assert(pneu_store_slot_info(0, &info) && info.mask == mask && info.tuned_k == k && !memcmp(info.recall, recall, 4));
    assert(info.k == 0 && info.count == RECORDS && info.state == PNEU_SLOT_COMMITTED && !strcmp(info.name, "tune48"));
}

static void test_champion_kept(void)
{
    static const uint8_t recall[4] = {75, 100, 75, 100};
    challenger_wins = false;
    assert(run_tune());
    check_common();
    /* 0x0015 and 0x0411 both 43 = 42 + margin 1: not enough. */
    assert(pneutouch_tune.finalist_correct[0] == 43U);
    assert(pneutouch_tune.mask == CHAMPION && pneutouch_tune.best_k == 4U && pneutouch_tune.correct == 42U);
    assert(pneutouch_tune.confusion[0][1] == 3U && pneutouch_tune.confusion[0][0] == 9U && pneutouch_tune.confusion[2][3] == 3U);
    assert(!memcmp((const uint8_t *)pneutouch_tune.recall, recall, 4) && pneutouch_tune.advice == PNEU_CLASS_TAIL);
    check_header(CHAMPION, 4, recall);
}

static void test_challenger_chosen(void)
{
    static const uint8_t recall[4] = {83, 100, 83, 100};
    challenger_wins = true;
    assert(run_tune());
    check_common();
    assert(pneutouch_tune.finalist_correct[0] == 44U);
    assert(pneutouch_tune.mask == 0x0015U && pneutouch_tune.best_k == 3U && pneutouch_tune.correct == 44U);
    assert(pneutouch_tune.confusion[0][1] == 2U && pneutouch_tune.confusion[1][1] == 12U);
    assert(!memcmp((const uint8_t *)pneutouch_tune.recall, recall, 4) && pneutouch_tune.advice == PNEU_CLASS_TAIL);
    check_header(0x0015U, 3, recall);
    /* Next boot uses exactly features 0, 4, 2 in Fisher order. */
    assert(pneu_model_init() && pneutouch_model.selected_count == 3 && pneutouch_model.mask == 0x0015U);
    assert(pneutouch_model.selected[0] == 0 && pneutouch_model.selected[1] == 4 && pneutouch_model.selected[2] == 2);
    challenger_wins = false;
}

static void test_errors(void)
{
    uint8_t before[32], after[32];
    load_slot(1, 7); /* fewer than 8 records */
    assert(pneu_store_use(1));
    header_copy(1, before);
    assert(!run_tune() && !pneutouch_tune.done && pneutouch_tune.error == PNEU_MODEL_ERR_NO_SLOT && inits == 0);
    header_copy(1, after); assert(!memcmp(before, after, 32));
    assert(pneu_store_use(0));
    header_copy(0, before);
    stalled = 1;
    assert(!run_tune() && pneutouch_tune.error == PNEU_MODEL_ERR_AI && !pneutouch_model.ready);
    stalled = 0;
    header_copy(0, after); assert(!memcmp(before, after, 32));
    assert(pneutouch_tune.mask == 0 && pneutouch_tune.recall[0] == PNEU_RECALL_ABSENT);
}

static void test_advice(void)
{
    static const uint16_t counts[4] = {10, 10, 10, 10}, fewer_head[4] = {10, 10, 10, 5};
    assert(pneu_model_advice((const uint8_t[4]){90, 90, 90, 90}, counts) == 0);
    assert(pneu_model_advice((const uint8_t[4]){100, 89, 95, 90}, counts) == PNEU_CLASS_BACK);
    assert(pneu_model_advice((const uint8_t[4]){80, 95, 95, 80}, counts) == PNEU_CLASS_TAIL);
    assert(pneu_model_advice((const uint8_t[4]){80, 95, 95, 80}, fewer_head) == PNEU_CLASS_HEAD);
    assert(pneu_model_advice((const uint8_t[4]){255, 95, 70, 255}, counts) == PNEU_CLASS_LEGS);
    assert(pneu_model_advice((const uint8_t[4]){255, 255, 255, 255}, counts) == 0);
    assert(pneu_model_advice((const uint8_t[4]){0, 100, 100, 100}, counts) == PNEU_CLASS_TAIL);
}

/* A KRAG slot (24 records, 12 HAT and 12 FACE): only two classes take part. */
static void test_krag(void)
{
    float pool[PNEU_POOL_COUNT];
    pneu_store_slot_t info;
    assert(pneu_store_open(2, 0, "KRAG24"));
    for (unsigned r = 0; r < 24U; ++r) { record_pool(r, pool); assert(pneu_store_put(2, (uint8_t)(r%2U+1U), pool)); }
    assert(pneu_store_commit(2) && pneu_store_use(2)); krag = true; krag_predicts = 0;
    assert(run_tune() && pneutouch_tune.done && pneutouch_model.object == PNEU_OBJECT_KRAG);
    assert(pneutouch_tune.recall[2] == PNEU_RECALL_ABSENT && pneutouch_tune.recall[3] == PNEU_RECALL_ABSENT);
    assert(pneutouch_tune.recall[0] <= 100U && pneutouch_tune.recall[1] <= 100U);
    assert(pneutouch_tune.advice >= 1U && pneutouch_tune.advice <= 2U && pneutouch_tune.screened == 4U);
    for (unsigned c = 0; c < 4; ++c) {
        assert(pneutouch_tune.confusion[c][2] == 0 && pneutouch_tune.confusion[c][3] == 0);
        assert(c < 2U || (pneutouch_tune.confusion[c][0] == 0 && pneutouch_tune.confusion[c][1] == 0));
    }
    assert(pneutouch_tune.confusion[0][0]+pneutouch_tune.confusion[0][1] == 12U);
    assert(pneu_store_slot_info(2, &info) && info.recall[2] == 255U && info.recall[3] == 255U && info.tuned_k);
    assert(pneu_store_open(3, 0, "KRAGbad")); /* a LEGS record: the slot cannot be tuned */
    for (unsigned r = 0; r < 12U; ++r) {
        record_pool(r, pool); assert(pneu_store_put(3, (uint8_t)(r == 5 ? 3 : r%2U+1U), pool));
    }
    assert(pneu_store_commit(3) && pneu_store_use(3));
    assert(!run_tune() && pneutouch_tune.error == PNEU_MODEL_ERR_RECORD);
    krag = false;
    assert(pneu_store_use(0));
}

int main(void)
{
    memset(fram, 0xff, sizeof(fram));
    assert(pneu_store_init());
    build_references();
    load_slot(0, RECORDS);
    pneu_model_set_progress(on_progress);
    test_ordinals();
    test_enumeration();
    test_keep();
    test_screening_sets();
    test_champion_kept();
    test_challenger_chosen();
    test_errors();
    test_advice();
    test_krag();
    puts("Model tune: screening ordinals/sets, 99 subsets in order, finalists and ties, finals, "
         "champion margin, header mask, progress, errors, advice, KRAG slot PASS");
    return 0;
}
