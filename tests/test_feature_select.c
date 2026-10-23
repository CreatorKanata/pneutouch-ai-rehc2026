/* feature_select.c against the hand-made fixture (double-precision expectations). */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "feature_select.h"
#include "feature_select_fixture.h"

static void close_to(float got, double expected)
{
    assert(fabs((double)got-expected) <= 1e-4*fabs(expected)+1e-5);
}

static void fixture_pool(unsigned r, float pool[PNEU_POOL_COUNT])
{
    for (unsigned j = 0; j < PNEU_POOL_COUNT; ++j) pool[j] = ldexpf(1.0f, fix_exponents[r][j]);
}

static void fixture_stats(pneu_stats_t *s)
{
    float pool[PNEU_POOL_COUNT];
    pneu_stats_reset(s);
    for (unsigned r = 0; r < FIX_RECORDS; ++r) { fixture_pool(r, pool); assert(pneu_stats_add(s, fix_labels[r], pool)); }
}

static void check(const pneu_selection_t *sel, const double *fisher, const double *mean, const double *scale,
                  const uint8_t *ranking, const uint8_t *chosen, unsigned count)
{
    uint8_t order[PNEU_POOL_COUNT];
    for (unsigned j = 0; j < PNEU_POOL_COUNT; ++j) {
        close_to(sel->fisher[j], fisher[j]); close_to(sel->mean[j], mean[j]); close_to(sel->scale[j], scale[j]);
    }
    pneu_rank(sel->fisher, order);
    assert(!memcmp(order, ranking, PNEU_POOL_COUNT));
    assert(sel->count == count && !memcmp(sel->index, chosen, count));
}

static void test_fixture(void)
{
    pneu_stats_t s;
    pneu_selection_t sel;
    float pool[PNEU_POOL_COUNT];
    uint16_t words[PNEU_SELECT_MAX];
    fixture_stats(&s);
    assert(s.n[0] == 2 && s.n[3] == 2);
    assert(pneu_select(&s, 0xF, 0, &sel));
    check(&sel, fix_primary_fisher, fix_primary_mean, fix_primary_scale, fix_primary_ranking,
          fix_primary_auto, FIX_PRIMARY_AUTO_COUNT);
    assert(sel.fisher[2] == sel.fisher[5]); /* exact tie: lower index ranks first */
    fixture_pool(FIX_PRIMARY_WORDS_RECORD, pool);
    assert(pneu_select_words(&sel, pool, words) && !memcmp(words, fix_primary_words, sizeof(words)));
    assert(pneu_select(&s, 0xF, 3, &sel) && sel.count == 3 && !memcmp(sel.index, fix_primary_k3, 3));
    assert(pneu_select_words(&sel, pool, words) && !memcmp(words, fix_primary_words, 6) && !words[3] && !words[11]);
    assert(pneu_select(&s, 0xC, 0, &sel));
    check(&sel, fix_specialist_fisher, fix_specialist_mean, fix_specialist_scale, fix_specialist_ranking,
          fix_specialist_auto, FIX_SPECIALIST_AUTO_COUNT);
    fixture_pool(FIX_SPECIALIST_WORDS_RECORD, pool);
    assert(pneu_select_words(&sel, pool, words) && !memcmp(words, fix_specialist_words, sizeof(words)));
    assert(pneu_select(&s, 0xC, 3, &sel) && !memcmp(sel.index, fix_specialist_k3, 3));
    assert(pneu_select(&s, 0xF, 12, &sel) && sel.count == 12);
    for (unsigned i = 0; i < 12; ++i) assert(sel.index[i] != 1); /* constant feature never selected */
    assert(!pneu_select(&s, 0xF, 13, &sel));
}

static void test_auto_bounds(void)
{
    pneu_stats_t s;
    pneu_selection_t sel;
    float pool[PNEU_POOL_COUNT];
    /* Only feature 0 clears max/16: padded to 2 by rank (all others tie). */
    pneu_stats_reset(&s);
    for (unsigned r = 0; r < FIX_RECORDS; ++r) {
        for (unsigned j = 0; j < PNEU_POOL_COUNT; ++j) pool[j] = ldexpf(1.0f, fix_exponents[r][j == 0 ? 0 : 6]);
        pool[1] = 1.0f;
        assert(pneu_stats_add(&s, fix_labels[r], pool));
    }
    assert(pneu_select(&s, 0xF, 0, &sel) && sel.count == 2 && sel.index[0] == 0 && sel.index[1] == 2);
    /* Thirteen equal features: truncated to 12, constant feature 1 skipped. */
    pneu_stats_reset(&s);
    for (unsigned r = 0; r < FIX_RECORDS; ++r) {
        for (unsigned j = 0; j < PNEU_POOL_COUNT; ++j) pool[j] = ldexpf(1.0f, fix_exponents[r][0]);
        pool[1] = 1.0f;
        assert(pneu_stats_add(&s, fix_labels[r], pool));
    }
    assert(pneu_select(&s, 0xF, 0, &sel) && sel.count == 12 && sel.index[0] == 0 && sel.index[1] == 2);
    assert(sel.index[11] == 12);
}

static void test_degenerate(void)
{
    pneu_stats_t s, before;
    pneu_selection_t sel;
    float pool[PNEU_POOL_COUNT];
    uint16_t words[PNEU_SELECT_MAX];
    fixture_stats(&s);
    assert(!pneu_select(&s, 0x1, 0, &sel)); /* one class */
    assert(!pneu_select(&s, 0x0, 0, &sel));
    before = s;
    fixture_pool(0, pool);
    pool[13] = NAN; assert(!pneu_stats_add(&s, 1, pool));
    pool[13] = INFINITY; assert(!pneu_stats_add(&s, 1, pool));
    pool[13] = 0.0f; assert(!pneu_stats_add(&s, 1, pool));
    pool[13] = -2.0f; assert(!pneu_stats_add(&s, 1, pool));
    fixture_pool(0, pool);
    assert(!pneu_stats_add(&s, 0, pool) && !pneu_stats_add(&s, 5, pool));
    assert(!memcmp(&s, &before, sizeof(s))); /* rejected records leave no trace */
    /* Two classes with identical members: every between-class score is 0. */
    pneu_stats_reset(&s);
    for (unsigned r = 0; r < 4; ++r) { fixture_pool(r % 2U, pool); assert(pneu_stats_add(&s, (uint8_t)(r/2U+1U), pool)); }
    assert(!pneu_select(&s, 0xF, 0, &sel) && !pneu_select(&s, 0xF, 3, &sel));
    /* Three records are not enough. */
    pneu_stats_reset(&s);
    for (unsigned r = 0; r < 3; ++r) { fixture_pool(r*2U, pool); assert(pneu_stats_add(&s, fix_labels[r*2U], pool)); }
    assert(!pneu_select(&s, 0xF, 0, &sel));
    fixture_stats(&s);
    assert(pneu_select(&s, 0xF, 0, &sel));
    fixture_pool(0, pool);
    pool[1] = NAN; /* not selected: ignored */
    assert(pneu_select_words(&sel, pool, words));
    pool[4] = 0.0f; assert(!pneu_select_words(&sel, pool, words));
    pool[4] = INFINITY; assert(!pneu_select_words(&sel, pool, words));
}

static void test_train_order(void)
{
    unsigned t = 0;
    for (uint8_t e = 0; e < 4; ++e)
        for (uint16_t i = 0; i < 8; ++i) assert(pneu_train_index(8, e, i) == fix_train_order[t++]);
    assert(pneu_train_index(0, 0, 0) == 0);
    for (uint16_t n = 1; n <= 1000U; n = (uint16_t)(n < 120 ? n+1 : n+37)) {
        for (uint8_t e = 0; e < PNEU_TRAIN_EPOCHS; ++e) {
            static uint8_t seen[1024];
            memset(seen, 0, sizeof(seen));
            for (uint16_t i = 0; i < n; ++i) { uint16_t k = pneu_train_index(n, e, i); assert(k < n && !seen[k]); seen[k] = 1; }
        }
    }
    for (uint8_t e = 0; e < 4; ++e) /* 105 = 3*5*7: strides 11, 13, 17, 19 */
        assert(pneu_train_index(105, e, 1) == (uint16_t)((11U+2U*e+(e > 1 ? 2U : 0U)+e*37U) % 105U));
}

int main(void)
{
    test_fixture();
    test_auto_bounds();
    test_degenerate();
    test_train_order();
    puts("Feature select: fixture Fisher/mean/scale/ranking/selection/words, bounds, degenerate input, order PASS");
    return 0;
}
