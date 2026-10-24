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
#include "object_profile.h"
#include "solistAi.h"
#include "crc16.h"
#include "fram_stub.h"

static unsigned trained, predicted, now, winner, progress_calls;
static int stalled = 0, nonfinite = 0;
static uint16_t expected_predict[12];
static bool check_predict;
static bool loose;                 /* KRAG slots: training inputs are not the fixture's */
static unsigned runner_up = 255;   /* output index with the second best score (255 none) */
static pneu_selection_t reference;
static int last_phase = -1;
static uint16_t last_done, last_total;
static unsigned phase_ends[2];

uint32_t board_micros(void) { return now++; }
void board_watchdog(void) {}
void smpl_enablePeripheral(unsigned mask) { assert(mask == 0x40000000U); }
void ODL_Initialize(uint8_t model, const ODL_Parameters *p)
{
    assert(model == 0 && p->inputSize == 12 && p->hiddenSize == 32 && p->outputSize == 4);
    assert(p->seed == 1 && p->activationFunction == 1 && p->scaleGamma == 0);
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
void ODL_StartTrain(uint8_t model, const bfloat16 *x, const bfloat16 *t)
{
    float pool[PNEU_POOL_COUNT];
    uint16_t words[12];
    unsigned r;
    assert(model == 0 && trained < 32U);
    if (loose) {
        unsigned hot = 0;
        for (unsigned i = 0; i < 4; ++i) hot += t[i] == 0x3f80 ? 1U : 0U;
        assert(hot == 1 && (pneutouch_model.object != PNEU_OBJECT_KRAG || (t[2] == 0 && t[3] == 0)));
        ++trained; return;
    }
    r = fix_train_order[trained++];
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
    if (check_predict) assert(!memcmp(x, expected_predict, sizeof(expected_predict)));
    ++predicted;
}
uint32_t ODL_IsBusy(void) { return (uint32_t)stalled; }
void ODL_GetResult(uint8_t model, bfloat16 *y)
{
    assert(model == 0);
    for (unsigned i = 0; i < 4; ++i) y[i] = (bfloat16)(i == winner ? 0xbf00 : i == runner_up ? 0xbf40 : 0xbf80);
    if (nonfinite) y[3] = 0x7fc0;
}
static void on_progress(uint8_t phase, uint16_t done, uint16_t total)
{
    assert(phase <= 1);
    if (phase == 0 && done == 0) last_phase = -1; /* a new pneu_model_init run */
    if ((int)phase != last_phase) {
        assert((int)phase == last_phase+1 && done == 0); /* phases in order, each starts at 0 */
        if (phase == 1) assert(pneutouch_model.lcd_line2[15] != ' '); /* selection already shown */
    } else assert(done > last_done && total == last_total);
    assert(done <= total && (done % 8U == 0U || done == total));
    if (done == total) ++phase_ends[phase];
    last_phase = phase; last_done = done; last_total = total; ++progress_calls;
}
static void load_named(uint8_t slot, uint8_t k, const char *name, const uint8_t labels[FIX_RECORDS], unsigned count)
{
    float pool[PNEU_POOL_COUNT];
    assert(pneu_store_open(slot, k, name));
    for (unsigned r = 0; r < count; ++r) { fixture_pool(r, pool); assert(pneu_store_put(slot, labels[r], pool)); }
    assert(pneu_store_commit(slot));
}
static void load_slot(uint8_t slot, uint8_t k, const uint8_t labels[FIX_RECORDS], unsigned count)
{
    load_named(slot, k, "fixture", labels, count);
}
static void reference_selection(uint8_t k)
{
    pneu_stats_t s;
    float pool[PNEU_POOL_COUNT];
    pneu_stats_reset(&s);
    for (unsigned r = 0; r < FIX_RECORDS; ++r) { fixture_pool(r, pool); assert(pneu_stats_add(&s, fix_labels[r], pool)); }
    assert(pneu_select(&s, 0xF, k, &reference));
}
static const char *line_of(void)
{
    static char line[17];
    for (unsigned i = 0; i < 17; ++i) line[i] = pneutouch_model.lcd_line2[i];
    return line;
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
    assert(!memcmp(line, "\xff   \xff     \xff  \xffk4", 17));
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

/* SW4 recording stores exactly the pool the prediction would use; no model needed. */
static void test_pool(void)
{
    float values[12], pool[PNEU_POOL_COUNT];
    for (unsigned i = 0; i < 12; ++i) values[i] = (float)i+0.5f;
    pneutouch_model.ready = false;
    assert(pneu_model_pool(values, 2000.0f, 300.0f, pool));
    for (unsigned i = 0; i < 12; ++i) assert(pool[i] == values[i]);
    assert(pool[12] == 2000.0f && pool[13] == 300.0f);
    assert(!pneu_model_pool(values, 0.0f, 300.0f, pool) && !pneu_model_pool(values, 2000.0f, 1e9f, pool));
    values[5] = NAN; assert(!pneu_model_pool(values, 2000.0f, 300.0f, pool));
    values[5] = 1e6f; assert(!pneu_model_pool(values, 2000.0f, 300.0f, pool));
    values[5] = -1.0f; assert(!pneu_model_pool(values, 2000.0f, 300.0f, pool));
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

/* Rewrites slot's header as version 1 (CRC at 24 over 0..23, no mask) with tuned_k. */
static void make_version1(uint8_t slot, uint8_t tuned_k)
{
    uint8_t *h = fram+0x2000U+slot*0xF000U;
    uint16_t crc;
    h[4] = 1; h[9] = tuned_k;
    memset(h+24, 0, 8);
    crc = pneu_crc16(h, 24); h[24] = (uint8_t)crc; h[25] = (uint8_t)(crc >> 8);
}

static void test_mask_boot(void)
{
    static const uint8_t recall[4] = {100, 50, 100, 100};
    memset(fram, 0xff, sizeof(fram));
    assert(pneu_store_init());
    /* Header version 2 with mask {13, 4, 7}: exactly those, in Fisher order 4, 13, 7. */
    load_slot(0, 0, fix_labels, FIX_RECORDS);
    assert(pneu_store_set_tuning(0, (1U << 13) | (1U << 4) | (1U << 7), recall));
    reference_selection(0);
    reference.count = 3; reference.index[0] = 4; reference.index[1] = 13; reference.index[2] = 7;
    assert(pneu_model_init() && pneutouch_model.selected_count == 3 && pneutouch_model.mask == 0x2090U);
    assert(pneutouch_model.selected[0] == 4 && pneutouch_model.selected[1] == 13 && pneutouch_model.selected[2] == 7);
    assert(pneutouch_model.tuned_k == 3 && pneutouch_model.advice == PNEU_CLASS_BACK);
    assert(!memcmp(line_of(), "    \xff  \xff     \xffk3", 17));
    /* A version 1 header with tuned_k 5: the best 5 by Fisher. */
    make_version1(0, 5);
    reference_selection(5);
    assert(pneu_model_init() && pneutouch_model.selected_count == 5 && pneutouch_model.tuned_k == 5);
    for (unsigned i = 0; i < 5; ++i) assert(pneutouch_model.selected[i] == fix_primary_ranking[i]);
    assert(pneutouch_model.mask == 0x3411U);
    /* An explicit header k wins over the mask. */
    load_slot(1, 3, fix_labels, FIX_RECORDS);
    assert(pneu_store_set_tuning(1, 0x2090U, recall) && pneu_store_use(1));
    reference_selection(3);
    assert(pneu_model_init() && pneutouch_model.selected_count == 3 && pneutouch_model.mask == 0x0411U);
    /* A mask whose features are all constant but one cannot be used: SELECT. */
    load_slot(2, 0, fix_labels, FIX_RECORDS);
    assert(pneu_store_set_tuning(2, 0x0003U, recall) && pneu_store_use(2));
    assert(!pneu_model_init() && pneutouch_model.error == PNEU_MODEL_ERR_SELECT);
    assert(pneu_store_use(0));
}

/* A KRAG slot: HAT/FACE only, argmax over the first two outputs, labels 3/4 refused. */
static void test_krag(void)
{
    static const uint8_t krag[FIX_RECORDS] = {1, 1, 2, 2, 1, 1, 2, 2}, legs[FIX_RECORDS] = {1, 1, 2, 2, 1, 3, 2, 2};
    float pool[PNEU_POOL_COUNT];
    pneu_stats_t s;
    pneu_selection_t want;
    memset(fram, 0xff, sizeof(fram));
    assert(pneu_store_init());
    load_named(0, 0, "KRAGx", krag, FIX_RECORDS);
    pneu_stats_reset(&s);
    for (unsigned r = 0; r < FIX_RECORDS; ++r) { fixture_pool(r, pool); assert(pneu_stats_add(&s, krag[r], pool)); }
    assert(pneu_select(&s, 0x03, 0, &want));
    loose = true;
    assert(pneu_model_init() && pneutouch_model.object == PNEU_OBJECT_KRAG && trained == 32);
    assert(pneutouch_model.mask == pneu_selection_mask(&want) && pneutouch_model.class_count[0] == 4);
    assert(pneutouch_model.class_count[1] == 4 && pneutouch_model.class_count[2] == 0);
    fixture_pool(0, pool);
    winner = 1; assert(pneu_model_predict(pool, pool[12], pool[13]) == PNEU_CLASS_BACK); /* FACE */
    winner = 0; assert(pneu_model_predict(pool, pool[12], pool[13]) == PNEU_CLASS_TAIL); /* HAT */
    winner = 2; runner_up = 1; /* LEGS output is the highest: the best of HAT/FACE wins */
    assert(pneu_model_predict(pool, pool[12], pool[13]) == PNEU_CLASS_BACK);
    winner = 3; runner_up = 0; assert(pneu_model_predict(pool, pool[12], pool[13]) == PNEU_CLASS_TAIL);
    runner_up = 255; /* HAT/FACE tie: the lower label */
    assert(pneu_model_predict(pool, pool[12], pool[13]) == PNEU_CLASS_TAIL);
    assert(!strcmp(pneu_class_name(PNEU_CLASS_BACK), "FACE") && !strcmp(pneu_class_name(PNEU_CLASS_TAIL), "HAT"));
    assert(!strcmp(pneu_class_name(PNEU_CLASS_LEGS), "UNKNOWN"));
    assert(!strcmp(pneu_class_name(PNEU_CLASS_HEAD), "UNKNOWN"));
    load_named(1, 0, "KRAGbad", legs, FIX_RECORDS); /* a LEGS record in a KRAG slot */
    assert(pneu_store_use(1) && !pneu_model_init() && pneutouch_model.error == PNEU_MODEL_ERR_RECORD);
    assert(pneutouch_model.object == PNEU_OBJECT_KRAG);
    load_named(2, 0, "DINObad", legs, FIX_RECORDS); /* the same labels are fine for DINO */
    assert(pneu_store_use(2) && pneu_model_init() && pneutouch_model.object == PNEU_OBJECT_DINO);
    winner = 2; assert(pneu_model_predict(pool, pool[12], pool[13]) == PNEU_CLASS_LEGS);
    assert(!strcmp(pneu_class_name(PNEU_CLASS_LEGS), "LEGS"));
    loose = false;
}

int main(void)
{
    test_training();
    test_line2_format();
    test_predict();
    test_failures();
    test_mask_boot();
    test_pool();
    assert(pneutouch_model.object == PNEU_OBJECT_DINO);
    assert(!strcmp(pneu_class_name(PNEU_CLASS_HEAD), "HEAD") && !strcmp(pneu_class_name((pneu_demo_class_t)9), "UNKNOWN"));
    test_krag();
    puts("Pressure model: FeRAM training order/words/labels, selection, LCD line 2, progress, predict, pool, faults, "
         "header mask / v1 tuned_k / explicit k at boot, KRAG slot PASS");
    return 0;
}
