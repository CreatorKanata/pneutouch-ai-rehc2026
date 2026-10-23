/* Boot training from the FeRAM stub: exact words/labels/order, progress, predict, faults. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "pressure_model.h"
#include "model_tune.h"
#include "model_store.h"
#include "feature_select.h"
#include "feature_select_fixture.h"
#include "solistAi.h"
#include "fram_stub.h"

static unsigned trained, predicted, now, winner, progress_calls;
static int stalled = 0, nonfinite = 0;
static uint16_t expected_predict[12];
static bool check_predict;
static pneu_selection_t reference;
static int last_phase = -1;
static uint16_t last_done, last_total;
static unsigned phase_ends[2];
/* Tune mode: ODL_Initialize starts fold (k, fold) = (4 + n/4, n%4) with n = inits so far. */
static bool tune_mode;
static unsigned tune_inits, tune_predicts, tune_calls;
static uint8_t tune_k, tune_fold;
static uint16_t tune_last_done = 0xFFFFU;
static pneu_selection_t fold_reference;
static void fold_selection(uint8_t k, uint8_t fold);

uint32_t board_micros(void) { return now++; }
void board_watchdog(void) {}
void smpl_enablePeripheral(unsigned mask) { assert(mask == 0x40000000U); }
void ODL_Initialize(uint8_t model, const ODL_Parameters *p)
{
    assert(model == 0 && p->inputSize == 12 && p->hiddenSize == 32 && p->outputSize == 4);
    assert(p->seed == 1 && p->activationFunction == 1 && p->scaleGamma == 0);
    if (tune_mode) {
        if (tune_inits) assert(trained == 24); /* previous fold: 4 epochs x 6 training records */
        tune_k = (uint8_t)(PNEU_TUNE_K_MIN+tune_inits/4U); tune_fold = (uint8_t)(tune_inits%4U);
        ++tune_inits;
        fold_selection(tune_k, tune_fold);
    }
    trained = 0;
}
void ODL_Reset(uint8_t model) { assert(model == 0); }
void ODL_SetWeightBeta(const void *v, uint8_t model, uint32_t off, uint16_t size)
{
    assert(model == 0 && off < 32U*8U && size == 8);
    for (unsigned i = 0; i < 4; ++i) assert(((const bfloat16 *)v)[i] == 0);
}
void ODL_SetWeightP(const void *v, uint8_t model, uint32_t off, uint16_t size)
{
    assert(model == 0 && size == 64U);
    for (unsigned i = 0; i < 32U; ++i) assert(((const bfloat16 *)v)[i] == (i == off/size ? 0x3f80 : 0));
}
static void fixture_pool(unsigned r, float pool[PNEU_POOL_COUNT])
{
    for (unsigned j = 0; j < PNEU_POOL_COUNT; ++j) pool[j] = ldexpf(1.0f, fix_exponents[r][j]);
}
/* x is the fold selection's words of some training record; t is that record's label. */
static void check_fold_train(const bfloat16 *x, const bfloat16 *t)
{
    float pool[PNEU_POOL_COUNT];
    uint16_t words[12];
    unsigned r, found = FIX_RECORDS;
    assert(++trained <= 24U);
    for (r = 0; r < FIX_RECORDS && found == FIX_RECORDS; ++r) {
        if (r%4U == tune_fold) continue;
        fixture_pool(r, pool);
        assert(pneu_select_words(&fold_reference, pool, words));
        if (!memcmp(x, words, sizeof(words))) found = r;
    }
    assert(found < FIX_RECORDS);
    for (unsigned i = 0; i < 4; ++i) assert(t[i] == (i+1U == fix_labels[found] ? 0x3f80 : 0));
}
/* Scripted held-out predictions (0-based class) for record r at candidate k. */
static unsigned scripted(uint8_t k, unsigned r)
{
    unsigned truth = fix_labels[r]-1U;
    if (k >= 7U) return 0; /* only TAIL right */
    if (r == 2U && k <= 5U) return 0;     /* BACK -> TAIL at k 4, 5 */
    if (r == 7U && k != 5U) return 2;     /* HEAD -> LEGS at k 4, 6 */
    return truth;
}
static void check_fold_predict(const bfloat16 *x)
{
    float pool[PNEU_POOL_COUNT];
    uint16_t words[12];
    unsigned j = tune_predicts%8U, k = PNEU_TUNE_K_MIN+tune_predicts/8U, fold = j/2U, r = fold+4U*(j%2U);
    assert(k == tune_k && fold == tune_fold && trained == 24U);
    fixture_pool(r, pool);
    assert(pneu_select_words(&fold_reference, pool, words) && !memcmp(x, words, sizeof(words)));
    winner = scripted((uint8_t)k, r);
    ++tune_predicts;
}
void ODL_StartTrain(uint8_t model, const bfloat16 *x, const bfloat16 *t)
{
    float pool[PNEU_POOL_COUNT];
    uint16_t words[12];
    unsigned r;
    if (tune_mode) { assert(model == 0); check_fold_train(x, t); return; }
    r = fix_train_order[trained++];
    assert(model == 0 && trained <= 32U);
    fixture_pool(r, pool);
    assert(pneu_select_words(&reference, pool, words) && !memcmp(x, words, sizeof(words)));
    if (r == FIX_PRIMARY_WORDS_RECORD && reference.count == FIX_PRIMARY_AUTO_COUNT)
        assert(!memcmp(x, fix_primary_words, sizeof(words)));
    for (unsigned i = 0; i < 4; ++i) assert(t[i] == (i+1U == fix_labels[r] ? 0x3f80 : 0));
}
void ODL_StartPredict(uint8_t model, const bfloat16 *x, const bfloat16 *t)
{
    assert(model == 0 && x != NULL);
    for (unsigned i = 0; i < 4; ++i) assert(t[i] == 0); /* no teacher leakage */
    if (tune_mode) check_fold_predict(x);
    if (check_predict) assert(!memcmp(x, expected_predict, sizeof(expected_predict)));
    ++predicted;
}
uint32_t ODL_IsBusy(void) { return (uint32_t)stalled; }
void ODL_GetResult(uint8_t model, bfloat16 *y)
{
    assert(model == 0);
    for (unsigned i = 0; i < 4; ++i) y[i] = (bfloat16)(i == winner ? 0xbf00 : 0xbf80);
    if (nonfinite) y[3] = 0x7fc0;
}
static void on_progress(uint8_t phase, uint16_t done, uint16_t total)
{
    assert(phase <= 2);
    if (phase == PNEU_PHASE_TUNE) { /* start, then once per finished fold */
        assert(total == 20 && done == (uint16_t)(tune_last_done+1U) && done == tune_calls);
        tune_last_done = done; ++tune_calls;
        return;
    }
    if (tune_mode) assert(total == (phase == 0 ? 6U : 24U)); /* 2 of 8 records held out */
    if (phase == 0 && done == 0) last_phase = -1; /* a new pneu_model_init run */
    if ((int)phase != last_phase) {
        assert((int)phase == last_phase+1 && done == 0); /* phases in order, each starts at 0 */
        if (phase == 1) assert(pneutouch_model.lcd_line2[15] != ' '); /* selection already shown */
    } else assert(done > last_done && total == last_total);
    assert(done <= total && (done % 8U == 0U || done == total));
    if (done == total) ++phase_ends[phase];
    last_phase = phase; last_done = done; last_total = total; ++progress_calls;
}
static void load_slot(uint8_t slot, uint8_t k, const uint8_t labels[FIX_RECORDS], unsigned count)
{
    float pool[PNEU_POOL_COUNT];
    assert(pneu_store_open(slot, k, "fixture"));
    for (unsigned r = 0; r < count; ++r) { fixture_pool(r, pool); assert(pneu_store_put(slot, labels[r], pool)); }
    assert(pneu_store_commit(slot));
}
static void reference_selection(uint8_t k)
{
    pneu_stats_t s;
    float pool[PNEU_POOL_COUNT];
    pneu_stats_reset(&s);
    for (unsigned r = 0; r < FIX_RECORDS; ++r) { fixture_pool(r, pool); assert(pneu_stats_add(&s, fix_labels[r], pool)); }
    assert(pneu_select(&s, 0xF, k, &reference));
}
static void fold_selection(uint8_t k, uint8_t fold)
{
    pneu_stats_t s;
    float pool[PNEU_POOL_COUNT];
    pneu_stats_reset(&s);
    for (unsigned r = 0; r < FIX_RECORDS; ++r) {
        if (r%4U == fold) continue;
        fixture_pool(r, pool); assert(pneu_stats_add(&s, fix_labels[r], pool));
    }
    assert(pneu_select(&s, 0xF, k, &fold_reference) && fold_reference.count == k);
}
static bool init_with_progress(void)
{
    last_phase = -1; phase_ends[0] = phase_ends[1] = 0; progress_calls = 0;
    return pneu_model_init();
}

static void test_training(void)
{
    char line[17];
    memset(fram, 0xff, sizeof(fram));
    assert(pneu_store_init());
    load_slot(0, 0, fix_labels, FIX_RECORDS);
    reference_selection(0);
    pneu_model_set_progress(on_progress);
    assert(init_with_progress() && pneutouch_model.ready && pneutouch_model.error == PNEU_MODEL_ERR_NONE);
    assert(trained == 32 && pneutouch_model.training_steps == 32 && pneutouch_model.training_examples == 8);
    assert(phase_ends[0] == 1 && phase_ends[1] == 1 && last_phase == 1 && last_total == 32);
    assert(progress_calls == 2+5); /* stats 0,8; training 0,8,16,24,32 */
    assert(pneutouch_model.slot == 0 && pneutouch_model.store_count == 8);
    assert(pneutouch_model.selected_count == FIX_PRIMARY_AUTO_COUNT);
    for (unsigned i = 0; i < FIX_PRIMARY_AUTO_COUNT; ++i) assert(pneutouch_model.selected[i] == fix_primary_auto[i]);
    for (unsigned j = 0; j < 14; ++j)
        assert(fabs(pneutouch_model.fisher[j]-fix_primary_fisher[j]) <= 1e-4*fix_primary_fisher[j]+1e-5);
    /* Cells in pool order 0..13: 0, 4, 10, 13 selected; count " 4". */
    for (unsigned i = 0; i < 17; ++i) line[i] = pneutouch_model.lcd_line2[i];
    assert(!memcmp(line, "\xff   \xff     \xff  \xff 4", 17));
}

static void test_line2_format(void)
{
    pneu_model_status_t status = {0};
    char line[17];
    pneu_model_line2(&status, line);
    assert(!strcmp(line, "                "));
    status.selected_count = 12;
    for (unsigned i = 0; i < 12; ++i) status.selected[i] = (uint8_t)(i+2U);
    pneu_model_line2(&status, line);
    assert(!memcmp(line, "  \xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff" "12", 17));
}

static void test_predict(void)
{
    float pool[PNEU_POOL_COUNT];
    fixture_pool(FIX_PRIMARY_WORDS_RECORD, pool);
    memcpy(expected_predict, fix_primary_words, sizeof(expected_predict));
    check_predict = true;
    for (winner = 0; winner < 4; ++winner) { /* LEGS/HEAD come straight from instance 0 */
        assert(pneu_model_predict(pool, pool[12], pool[13]) == (pneu_demo_class_t)(winner+1U));
        assert(pneutouch_model.label == (pneu_demo_class_t)(winner+1U));
    }
    assert(predicted == 4 && pneutouch_model.predictions == 4);
    winner = 3;
    assert(pneu_model_predict(pool, NAN, pool[13]) == PNEU_CLASS_UNKNOWN);
    assert(pneu_model_predict(pool, pool[12], NAN) == PNEU_CLASS_UNKNOWN);
    assert(pneu_model_predict(pool, 0.0f, pool[13]) == PNEU_CLASS_UNKNOWN);
    assert(pneu_model_predict(pool, pool[12], 1e9f) == PNEU_CLASS_UNKNOWN);
    pool[0] = NAN; assert(pneu_model_predict(pool, pool[12], pool[13]) == PNEU_CLASS_UNKNOWN);
    pool[0] = 0.0f; assert(pneu_model_predict(pool, pool[12], pool[13]) == PNEU_CLASS_UNKNOWN);
    assert(predicted == 4 && pneutouch_model.label == PNEU_CLASS_UNKNOWN);
    fixture_pool(FIX_PRIMARY_WORDS_RECORD, pool);
    nonfinite = 1; assert(pneu_model_predict(pool, pool[12], pool[13]) == PNEU_CLASS_UNKNOWN);
    nonfinite = 0; stalled = 1;
    assert(pneu_model_predict(pool, pool[12], pool[13]) == PNEU_CLASS_UNKNOWN && !pneutouch_model.ready);
    assert(!pneu_model_init() && pneutouch_model.error == PNEU_MODEL_ERR_AI);
    assert(pneutouch_model.lcd_line2[0] == ' ' && pneutouch_model.lcd_line2[15] == ' ');
    stalled = 0;
    /* Explicit k = 3: words beyond the third are zero. */
    load_slot(1, 3, fix_labels, FIX_RECORDS);
    assert(pneu_store_use(1));
    reference_selection(3);
    assert(pneu_model_init() && pneutouch_model.selected_count == 3 && pneutouch_model.slot == 1);
    memset(expected_predict, 0, sizeof(expected_predict));
    memcpy(expected_predict, fix_primary_words, 3*sizeof(uint16_t));
    assert(pneu_model_predict(pool, pool[12], pool[13]) == PNEU_CLASS_HEAD);
    check_predict = false;
}

static void test_failures(void)
{
    static const uint8_t one_class[FIX_RECORDS] = {1, 1, 1, 1, 1, 1, 1, 1};
    float pool[PNEU_POOL_COUNT];
    assert(pneu_store_use(2)); /* never written */
    assert(!init_with_progress() && pneutouch_model.error == PNEU_MODEL_ERR_NO_SLOT && progress_calls == 0);
    assert(pneutouch_model.lcd_line2[15] == ' ');
    fixture_pool(0, pool);
    assert(pneu_model_predict(pool, pool[12], pool[13]) == PNEU_CLASS_UNKNOWN);
    assert(pneu_store_open(2, 0, "open"));
    assert(!pneu_model_init() && pneutouch_model.error == PNEU_MODEL_ERR_NO_SLOT); /* not committed */
    load_slot(3, 0, one_class, FIX_RECORDS);
    assert(pneu_store_use(3));
    assert(!pneu_model_init() && pneutouch_model.error == PNEU_MODEL_ERR_SELECT);
    reference_selection(0);
    assert(pneu_store_use(0) && pneu_model_init());
    fram[0x2000U+32U+3U*60U+10U] ^= 0x40U; /* record 3 of slot 0 */
    assert(!pneu_model_init() && pneutouch_model.error == PNEU_MODEL_ERR_RECORD);
    fram[0x2000U+32U+3U*60U+10U] ^= 0x40U;
    memset(fram+0x1000, 0, 16); fram_drop_writes = true;
    assert(!pneu_model_init() && pneutouch_model.error == PNEU_MODEL_ERR_STORE);
    fram_drop_writes = false;
    assert(pneu_model_init()); /* fresh directory: active slot 0 */
}

static void header_copy(uint8_t slot, uint8_t out[32]) { memcpy(out, fram+0x2000U+slot*0xF000U, 32); }
static bool tune_scripted(void)
{
    tune_mode = true; tune_inits = tune_predicts = tune_calls = 0; tune_last_done = 0xFFFFU;
    trained = 0; last_phase = -1;
    bool ok = pneu_model_tune();
    tune_mode = false;
    return ok;
}

static void test_tune(void)
{
    static const uint16_t confusion[4][4] = {{2, 0, 0, 0}, {1, 1, 0, 0}, {0, 0, 2, 0}, {0, 0, 0, 2}};
    static const uint8_t recall[4] = {100, 50, 100, 100};
    pneu_store_slot_t info;
    memset(fram, 0xff, sizeof(fram));
    assert(pneu_store_init());
    load_slot(0, 0, fix_labels, FIX_RECORDS);
    pneu_model_set_progress(on_progress);
    assert(pneu_model_init() && pneutouch_model.ready && pneutouch_model.tuned_k == 0 && pneutouch_model.advice == 0);
    assert(tune_scripted() && pneutouch_tune.done && pneutouch_tune.error == 0);
    assert(tune_inits == 20 && tune_predicts == 40 && trained == 24);
    assert(tune_calls == 21 && tune_last_done == 20);
    assert(pneutouch_tune.slot == 0 && pneutouch_tune.count == 8);
    assert(pneutouch_tune.correct[3] == 0 && pneutouch_tune.correct[4] == 6 && pneutouch_tune.correct[5] == 7);
    assert(pneutouch_tune.correct[6] == 7 && pneutouch_tune.correct[7] == 2 && pneutouch_tune.correct[8] == 2);
    assert(pneutouch_tune.best_k == 5 && pneutouch_tune.advice == PNEU_CLASS_BACK);
    for (unsigned t = 0; t < 4; ++t) {
        assert(pneutouch_tune.recall[t] == recall[t]);
        for (unsigned p = 0; p < 4; ++p) assert(pneutouch_tune.confusion[t][p] == confusion[t][p]);
    }
    assert(!pneutouch_model.ready);
    assert(pneu_store_slot_info(0, &info) && info.tuned_k == 5 && !memcmp(info.recall, recall, 4));
    assert(info.k == 0 && info.count == 8 && info.state == PNEU_SLOT_COMMITTED && !strcmp(info.name, "fixture"));
    /* Next boot: the tuned k replaces the Fisher-auto 4. */
    reference_selection(5);
    assert(pneu_model_init() && pneutouch_model.selected_count == 5);
    for (unsigned i = 0; i < 5; ++i) assert(pneutouch_model.selected[i] == fix_primary_ranking[i]);
    assert(pneutouch_model.tuned_k == 5 && pneutouch_model.advice == PNEU_CLASS_BACK);
    assert(!memcmp((const uint8_t *)pneutouch_model.recall, recall, 4));
    for (unsigned c = 0; c < 4; ++c) assert(pneutouch_model.class_count[c] == 2);
}

static void test_tune_failures(void)
{
    static const uint8_t recall[4] = {100, 50, 100, 100};
    uint8_t before[32], after[32];
    /* An explicit header k still wins over tuned_k. */
    load_slot(1, 3, fix_labels, FIX_RECORDS);
    assert(pneu_store_set_tuning(1, 5, recall) && pneu_store_use(1));
    reference_selection(3);
    assert(pneu_model_init() && pneutouch_model.selected_count == 3 && pneutouch_model.tuned_k == 5);
    /* Fewer than 8 records: NO_SLOT, header untouched. */
    load_slot(2, 0, fix_labels, 4);
    assert(pneu_store_use(2));
    header_copy(2, before);
    assert(!tune_scripted() && !pneutouch_tune.done && pneutouch_tune.error == PNEU_MODEL_ERR_NO_SLOT);
    header_copy(2, after); assert(!memcmp(before, after, 32) && tune_inits == 0);
    /* A stalled or non-finite Solist-AI: error AI, header untouched, model not ready. */
    assert(pneu_store_use(0));
    header_copy(0, before);
    stalled = 1;
    assert(!tune_scripted() && pneutouch_tune.error == PNEU_MODEL_ERR_AI && !pneutouch_model.ready);
    stalled = 0; nonfinite = 1;
    assert(!tune_scripted() && pneutouch_tune.error == PNEU_MODEL_ERR_AI && tune_predicts == 1);
    nonfinite = 0;
    header_copy(0, after); assert(!memcmp(before, after, 32));
    assert(pneutouch_tune.best_k == 0 && pneutouch_tune.recall[0] == 255);
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

int main(void)
{
    test_training();
    test_line2_format();
    test_predict();
    test_failures();
    test_tune();
    test_tune_failures();
    test_advice();
    assert(!strcmp(pneu_class_name(PNEU_CLASS_HEAD), "HEAD") && !strcmp(pneu_class_name((pneu_demo_class_t)9), "UNKNOWN"));
    puts("Pressure model: FeRAM training order/words/labels, selection, LCD line 2, progress, predict, faults, "
         "k tuning (4-fold), advice PASS");
    return 0;
}
