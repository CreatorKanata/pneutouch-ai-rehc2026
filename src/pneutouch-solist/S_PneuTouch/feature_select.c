/* Fisher score per feature over the participating classes, computed from one
   streaming pass of per-class sums so 1000 FeRAM records need no RAM copy. */
#include "feature_select.h"
#include <math.h>
#include <string.h>

#define MIN_SCALE 1e-6f
#define AUTO_DIVISOR 10.0f
#define MIN_AUTO 2U

static bool finite_positive(float v)
{
    uint32_t bits;
    memcpy(&bits, &v, sizeof(bits));
    return (bits & 0x7f800000U) != 0x7f800000U && v > 0.0f;
}

void pneu_stats_reset(pneu_stats_t *s)
{
    memset(s, 0, sizeof(*s));
}

bool pneu_stats_add(pneu_stats_t *s, uint8_t label, const float pool[PNEU_POOL_COUNT])
{
    unsigned j, c = (unsigned)label-1U;
    if (label < 1U || label > PNEU_CLASS_COUNT || s->n[c] == UINT16_MAX) return false;
    for (j = 0; j < PNEU_POOL_COUNT; ++j) if (!finite_positive(pool[j])) return false;
    for (j = 0; j < PNEU_POOL_COUNT; ++j) {
        float v = logf(pool[j]);
        s->sum[c][j] += v;
        s->sumsq[c][j] += v*v;
    }
    ++s->n[c];
    return true;
}

/* Fisher, mean and population std of feature j. */
static void score(const pneu_stats_t *s, uint8_t mask, float count, unsigned j, pneu_selection_t *out)
{
    float total = 0.0f, squares = 0.0f, within = 0.0f, between = 0.0f, var;
    unsigned c;
    for (c = 0; c < PNEU_CLASS_COUNT; ++c) {
        if (!(mask & (1U << c)) || !s->n[c]) continue;
        total += s->sum[c][j]; squares += s->sumsq[c][j];
    }
    out->mean[j] = total/count;
    for (c = 0; c < PNEU_CLASS_COUNT; ++c) {
        float nc = (float)s->n[c], sc = s->sum[c][j], d;
        if (!(mask & (1U << c)) || !s->n[c]) continue;
        within += s->sumsq[c][j]-sc*sc/nc;
        d = sc/nc-out->mean[j];
        between += nc*d*d;
    }
    var = squares/count-out->mean[j]*out->mean[j];
    out->scale[j] = sqrtf(var > 0.0f ? var : 0.0f);
    /* A constant feature cannot be normalized, so it is never ranked. */
    out->fisher[j] = out->scale[j] > MIN_SCALE && within > 0.0f ? between/within : 0.0f;
    out->inv_scale[j] = out->scale[j] > MIN_SCALE ? 1.0f/out->scale[j] : 0.0f;
}

void pneu_rank(const float fisher[PNEU_POOL_COUNT], uint8_t order[PNEU_POOL_COUNT])
{
    unsigned i, j;
    for (i = 0; i < PNEU_POOL_COUNT; ++i) order[i] = (uint8_t)i;
    for (i = 0; i < PNEU_POOL_COUNT; ++i) {
        unsigned best = i;
        for (j = i+1U; j < PNEU_POOL_COUNT; ++j) {
            float a = fisher[order[j]], b = fisher[order[best]];
            if (a > b || (a == b && order[j] < order[best])) best = j;
        }
        if (best != i) { uint8_t t = order[i]; order[i] = order[best]; order[best] = t; }
    }
}

static unsigned auto_count(const pneu_selection_t *sel, const uint8_t order[PNEU_POOL_COUNT])
{
    float floor = sel->fisher[order[0]]/AUTO_DIVISOR;
    unsigned i, chosen = 0;
    for (i = 0; i < PNEU_POOL_COUNT; ++i) {
        float f = sel->fisher[order[i]];
        if (sel->scale[order[i]] > MIN_SCALE && f > 0.0f && f >= floor) ++chosen;
    }
    if (chosen < MIN_AUTO) chosen = MIN_AUTO;
    return chosen > PNEU_SELECT_MAX ? PNEU_SELECT_MAX : chosen;
}

static void pick(pneu_selection_t *sel, uint8_t k)
{
    uint8_t order[PNEU_POOL_COUNT];
    unsigned i, wanted;
    pneu_rank(sel->fisher, order);
    wanted = k ? k : auto_count(sel, order);
    sel->count = 0;
    for (i = 0; i < PNEU_POOL_COUNT && sel->count < wanted; ++i)
        if (sel->scale[order[i]] > MIN_SCALE) sel->index[sel->count++] = order[i];
}

bool pneu_select(const pneu_stats_t *s, uint8_t class_mask, uint8_t k, pneu_selection_t *out)
{
    unsigned c, j, n = 0, classes = 0;
    float best = 0.0f;
    memset(out, 0, sizeof(*out));
    if (k > PNEU_SELECT_MAX) return false;
    for (c = 0; c < PNEU_CLASS_COUNT; ++c)
        if ((class_mask & (1U << c)) && s->n[c]) { n += s->n[c]; ++classes; }
    if (n < 4U || classes < 2U) return false;
    for (j = 0; j < PNEU_POOL_COUNT; ++j) {
        score(s, class_mask, (float)n, j, out);
        if (out->fisher[j] > best) best = out->fisher[j];
    }
    if (!(best > 0.0f)) return false;
    pick(out, k);
    return true;
}

bool pneu_select_words(const pneu_selection_t *sel, const float pool[PNEU_POOL_COUNT],
                       uint16_t words[PNEU_SELECT_MAX])
{
    unsigned s;
    memset(words, 0, PNEU_SELECT_MAX*sizeof(words[0]));
    if (sel->count > PNEU_SELECT_MAX) return false;
    for (s = 0; s < sel->count; ++s) {
        unsigned j = sel->index[s];
        uint32_t bits;
        float scaled;
        if (j >= PNEU_POOL_COUNT || !finite_positive(pool[j])) return false;
        scaled = (logf(pool[j])-sel->mean[j])*sel->inv_scale[j];
        memcpy(&bits, &scaled, sizeof(bits));
        if ((bits & 0x7f800000U) == 0x7f800000U) return false;
        words[s] = (uint16_t)(bits >> 16);
    }
    return true;
}

uint16_t pneu_train_index(uint16_t n, uint8_t epoch, uint16_t i)
{
    static const uint8_t primes[] = {3, 5, 7, 11, 13, 17, 19, 23};
    unsigned p, seen = 0;
    uint32_t stride = 1U, offset;
    if (!n) return 0;
    /* A prime that does not divide n is coprime to it: every record once per epoch. */
    for (p = 0; p < sizeof(primes); ++p) {
        if (n % primes[p] == 0U) continue;
        if (seen++ == epoch) { stride = primes[p]; break; }
    }
    offset = (uint32_t)epoch*37U % n;
    return (uint16_t)(((uint32_t)i*stride+offset) % n);
}
