/* No heap, FFT, model, labels or future samples. Float operations run on the CPU.
   Completion means a resolved positive/negative pair, not a classified body part. */
#include "pressure_features.h"
#include <string.h>

#define WARM_POINTS 32U
#define WARM_SPAN 20000
#define TRIGGER_COUNTS 50000.0f
#define QUIET_COUNTS 25000.0f
#define TIMEOUT_MS 4000U
#define MAX_INTERVAL_MS 100U

/* Only extract() uses these arrays. Cache each conversion once per completed
   event: thousands of repeated soft-float conversions otherwise stall the M0+
   for longer than one 25 ms sample period. Single-threaded, no heap or stack. */
static float extraction_time[PNEU_HISTORY_COUNT];
static float extraction_delta[PNEU_HISTORY_COUNT];

static float magnitude(float x) { return x < 0.0f ? -x : x; }
static pneu_history_sample_t sample(const pneu_features_t *s, unsigned i)
{
    unsigned index = s->head + PNEU_HISTORY_COUNT - s->count + i;
    if (index >= PNEU_HISTORY_COUNT) index -= PNEU_HISTORY_COUNT;
    return s->history[index];
}
static float time_at(const pneu_features_t *s, unsigned i)
{
    (void)s;
    return extraction_time[i];
}
static float delta_at(const pneu_features_t *s, unsigned i)
{
    (void)s;
    return extraction_delta[i];
}
static void add_quiet(pneu_features_t *s, int32_t raw)
{
    s->quiet[s->quiet_head] = raw;
    s->quiet_head = (uint16_t)((s->quiet_head + 1U) % PNEU_BASELINE_COUNT);
    if (s->quiet_count < PNEU_BASELINE_COUNT) ++s->quiet_count;
}
static float median(pneu_features_t *s, unsigned n)
{
    unsigned i, j;
    for (i = 1; i < n; ++i) {
        int32_t v = s->scratch[i];
        j = i;
        while (j && s->scratch[j-1] > v) {
            s->scratch[j] = s->scratch[j-1];
            --j;
        }
        s->scratch[j] = v;
    }
    return n & 1U ? (float)s->scratch[n/2] :
        ((float)s->scratch[n/2-1] + (float)s->scratch[n/2]) * 0.5f;
}
static float quiet_baseline(pneu_features_t *s)
{
    unsigned i;
    for (i = 0; i < s->quiet_count; ++i) s->scratch[i] = s->quiet[i];
    return median(s, s->quiet_count);
}
static bool crossing(const pneu_features_t *s, float level, unsigned lo,
                     unsigned hi, bool negative, bool up, bool last, float *out)
{
    unsigned i;
    bool found = false;
    for (i = lo; i < hi; ++i) {
        float a = delta_at(s, i), b = delta_at(s, i+1);
        if (negative) { a = -a; b = -b; }
        if (up ? (a < level && b >= level) : (a > level && b <= level)) {
            float ta = time_at(s, i), tb = time_at(s, i+1);
            *out = ta + (level-a)/(b-a)*(tb-ta);
            found = true;
            if (!last) break;
        }
    }
    return found;
}
static float area(const pneu_features_t *s, float start, float end, bool negative)
{
    float sum = 0.0f;
    unsigned i;
    for (i = 0; i+1 < s->count; ++i) {
        float ta = time_at(s, i), tb = time_at(s, i+1);
        float a, b, ya, yb, za, zb;
        if (tb <= start || ta >= end) continue;
        a = ta > start ? ta : start;
        b = tb < end ? tb : end;
        ya = delta_at(s, i); yb = delta_at(s, i+1);
        za = ya + (yb-ya)*(a-ta)/(tb-ta);
        zb = ya + (yb-ya)*(b-ta)/(tb-ta);
        sum += (za+zb)*0.5f*(b-a);
    }
    return negative ? -sum : sum; /* counts * ms */
}
static bool extract(pneu_features_t *s)
{
    unsigned i, nbase = 0, begin = 0, peak = 0, trough, lo;
    float first = (float)(int32_t)(s->first_high_ms-s->event.trigger_ms);
    float a, b, pa[3], pd[3], na[3], nd[3], positive_area, negative_area, slope = 0.0f;
    const float levels[3] = {0.1f, 0.5f, 0.9f};
    float *f = s->event.value;
    for (i = 0; i < s->count; ++i) {
        pneu_history_sample_t v = sample(s, i);
        /* Differences remain within the bounded window, including uint32 wrap. */
        extraction_time[i] = (float)(int32_t)(v.ms-s->event.trigger_ms);
        extraction_delta[i] = (float)v.raw;
    }
    for (i = 0; i < s->count; ++i) {
        float t = time_at(s, i);
        if (t >= first-800.0f && t <= first-300.0f && nbase < PNEU_BASELINE_COUNT)
            s->scratch[nbase++] = sample(s, i).raw;
    }
    if (nbase < 5) return false;
    s->event.baseline = median(s, nbase);
    for (i = 0; i < s->count; ++i) extraction_delta[i] -= s->event.baseline;
    while (begin+1 < s->count && time_at(s, begin) < first) ++begin;
    peak = begin;
    for (i = begin; i < s->count; ++i)
        if (sample(s, i).raw > sample(s, peak).raw) peak = i;
    trough = peak;
    for (i = peak; i < s->count; ++i)
        if (sample(s, i).raw < sample(s, trough).raw) trough = i;
    a = delta_at(s, peak); b = -delta_at(s, trough);
    if (a < 100000.0f || b < 30000.0f) return false;
    lo = 0;
    while (lo < peak && time_at(s, lo) < time_at(s, peak)-1200.0f) ++lo;
    for (i = 0; i < 3; ++i) {
        if (!crossing(s, levels[i]*a, lo, peak, false, true, true, &pa[i]) ||
            !crossing(s, levels[i]*a, peak, trough, false, false, false, &pd[i]) ||
            !crossing(s, levels[i]*b, peak, trough, true, true, true, &na[i]) ||
            !crossing(s, levels[i]*b, trough, s->count-1U, true, false, false, &nd[i]))
            return false;
    }
    for (i = lo; i+1 < s->count; ++i) {
        float ta = time_at(s, i), tb = time_at(s, i+1);
        if (ta >= pa[0]-25.0f && tb <= time_at(s, peak)+25.0f) {
            float v = (delta_at(s, i+1)-delta_at(s, i))/(tb-ta)*1000.0f;
            if (v > slope) slope = v;
        }
    }
    positive_area = area(s, pa[0], pd[0], false);
    negative_area = area(s, na[0], nd[0], true);
    f[0] = pa[2]-pa[0]; f[1] = pd[0]-pd[2]; f[2] = pd[1]-pa[1];
    f[3] = positive_area/a; f[4] = slope/a; f[5] = f[1]/f[0];
    f[6] = na[2]-na[0]; f[7] = nd[0]-nd[2]; f[8] = nd[1]-na[1];
    f[9] = negative_area/b; f[10] = b/a; f[11] = negative_area/positive_area;
    /* This also rejects NaN/infinity without bringing in libm. */
    for (i = 0; i < PNEU_FEATURE_COUNT; ++i)
        if (!(f[i] > 0.0f && f[i] < 100000.0f)) return false;
    s->event.peak = a; s->event.trough = b;
    return true;
}

void pneu_features_init(pneu_features_t *s) { memset(s, 0, sizeof(*s)); }
void pneu_features_reset(pneu_features_t *s)
{
    uint32_t id = s->next_id;
    pneu_features_init(s);
    s->next_id = id;
}
pneu_demo_class_t pneu_demo_class_for_zone(uint8_t zone)
{
    if (zone == 1) return PNEU_CLASS_TAIL;
    if (zone == 2) return PNEU_CLASS_BACK;
    if (zone >= 3 && zone <= 6) return PNEU_CLASS_LEGS;
    return zone == 7 ? PNEU_CLASS_HEAD : PNEU_CLASS_UNKNOWN;
}
pneu_event_result_t pneu_features_feed(pneu_features_t *s, uint32_t seq, uint32_t ms, int32_t raw)
{
    float base, delta;
    unsigned i;
    bool reset = false;
    if (s->have_previous && (seq-s->previous_seq != 1U ||
        ms-s->previous_ms == 0U || ms-s->previous_ms > MAX_INTERVAL_MS)) {
        pneu_features_reset(s);
        reset = true;
    }
    if (raw <= -8388608 || raw >= 8388607) {
        pneu_features_reset(s);
        return PNEU_EVENT_RESET;
    }
    s->previous_ms = ms; s->previous_seq = seq; s->have_previous = true;
    s->history[s->head] = (pneu_history_sample_t){ms, raw};
    s->head = (uint16_t)((s->head+1U) % PNEU_HISTORY_COUNT);
    if (s->count < PNEU_HISTORY_COUNT) ++s->count;
    if (!s->ready) {
        int32_t min, max;
        if (s->count < WARM_POINTS) return reset ? PNEU_EVENT_RESET : PNEU_EVENT_NONE;
        min = max = sample(s, s->count-WARM_POINTS).raw;
        for (i = s->count-WARM_POINTS; i < s->count; ++i) {
            int32_t v = sample(s, i).raw;
            if (v < min) min = v;
            if (v > max) max = v;
        }
        if (max-min <= WARM_SPAN) {
            s->quiet_head = s->quiet_count = 0;
            for (i = s->count-WARM_POINTS; i < s->count; ++i) add_quiet(s, sample(s, i).raw);
            s->ready = true;
        }
        return PNEU_EVENT_NONE;
    }
    if (!s->active) {
        base = quiet_baseline(s);
        delta = (float)raw-base;
        if (delta > TRIGGER_COUNTS) {
            if (s->consecutive_high == 0) {
                s->detection_baseline = base;
                s->first_high_ms = ms;
            }
            if (++s->consecutive_high == 2) {
                memset(&s->event, 0, sizeof(s->event));
                s->event.id = ++s->next_id; s->event.trigger_ms = ms;
                s->active = true; s->negative_seen = false;
                s->consecutive_high = s->consecutive_quiet = 0;
                return PNEU_EVENT_START;
            }
        } else {
            s->consecutive_high = 0;
            if (magnitude(delta) <= QUIET_COUNTS) add_quiet(s, raw);
        }
        return PNEU_EVENT_NONE;
    }
    delta = (float)raw-s->detection_baseline;
    if (delta < -TRIGGER_COUNTS) s->negative_seen = true;
    if (s->negative_seen && magnitude(delta) <= QUIET_COUNTS) ++s->consecutive_quiet;
    else s->consecutive_quiet = 0;
    if (s->consecutive_quiet >= 8) {
        bool valid;
        s->event.end_ms = ms;
        valid = extract(s);
        s->active = false; s->consecutive_high = s->consecutive_quiet = 0;
        s->quiet_head = s->quiet_count = 0;
        for (i = s->count-8U; i < s->count; ++i) add_quiet(s, sample(s, i).raw);
        return valid ? PNEU_EVENT_COMPLETE : PNEU_EVENT_INVALID;
    }
    if (ms-s->event.trigger_ms >= TIMEOUT_MS) {
        /* Keep the last event for reporting; require a fresh baseline afterward. */
        s->event.end_ms = ms;
        s->active = s->ready = false;
        s->count = s->head = s->quiet_head = s->quiet_count = 0;
        s->consecutive_high = s->consecutive_quiet = 0;
        return PNEU_EVENT_TIMEOUT;
    }
    return PNEU_EVENT_NONE;
}
