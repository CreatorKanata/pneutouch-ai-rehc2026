#include <assert.h>
#include <stdio.h>
#include "pressure_features.h"

static pneu_features_t state;
static pneu_event_result_t feed(unsigned i, int32_t raw)
{
    return pneu_features_feed(&state, i, i*25U, raw);
}
/* KRAG-sized triangular press: +12000 peak, -15000 trough, back to baseline.
   Returns the number of completed events (with the profile set before the press). */
static unsigned weak_press(const pneu_detect_t *detect)
{
    unsigned i, completed = 0;
    pneu_features_init(&state);
    assert(state.detect.trigger == PNEU_DETECT_DINO.trigger); /* init: DINO */
    if (detect) pneu_features_set_detection(&state, detect);
    for (i = 0; i < 60; ++i) feed(i, 4000000);
    for (i = 60; i < 125; ++i) {
        int u = (int)i-60;
        int delta = u <= 10 ? 1200*u : u <= 20 ? 1200*(20-u) :
                    u <= 30 ? -1500*(u-20) : u <= 40 ? -1500*(40-u) : 0;
        if (feed(i, 4000000+delta) == PNEU_EVENT_COMPLETE) {
            ++completed;
            assert(state.event.peak == 12000.0f && state.event.trough == 15000.0f);
        }
    }
    return completed;
}
static void test_detection_profiles(void)
{
    assert(PNEU_DETECT_DINO.trigger == 30000.0f && PNEU_DETECT_DINO.quiet == 25000.0f);
    assert(PNEU_DETECT_DINO.min_peak == 25000.0f && PNEU_DETECT_DINO.min_trough == 30000.0f);
    assert(PNEU_DETECT_KRAG.trigger == 7000.0f && PNEU_DETECT_KRAG.quiet == 5000.0f);
    assert(PNEU_DETECT_KRAG.min_peak == 6000.0f && PNEU_DETECT_KRAG.min_trough == 6000.0f);
    assert(weak_press(&PNEU_DETECT_KRAG) == 1);
    assert(weak_press(NULL) == 0 && !state.active); /* DINO: never triggers */
    assert(weak_press(&PNEU_DETECT_DINO) == 0);
    /* reset keeps the profile; a later set takes effect without a reset */
    pneu_features_set_detection(&state, &PNEU_DETECT_KRAG);
    pneu_features_reset(&state);
    assert(state.detect.trigger == 7000.0f && state.detect.min_trough == 6000.0f);
}
int main(void)
{
    unsigned i;
    pneu_features_init(&state);
    assert(sizeof(state) < 4096);
    for (i = 0; i < 60; ++i) assert(feed(i, 4000000) == PNEU_EVENT_NONE);
    assert(state.ready);
    assert(feed(60, 4200000) == PNEU_EVENT_NONE);
    assert(feed(61, 4000000) == PNEU_EVENT_NONE); /* isolated spike */
    assert(!state.active);
    assert(feed(62, 4200000) == PNEU_EVENT_NONE);
    assert(feed(63, 4200000) == PNEU_EVENT_START);
    for (i = 64; i < 223; ++i) assert(feed(i, 4000000) == PNEU_EVENT_NONE);
    assert(feed(223, 4000000) == PNEU_EVENT_TIMEOUT); /* no release */
    assert(!state.ready && !state.active);
    for (i = 224; i < 270; ++i) feed(i, 4000000);
    assert(state.ready);
    assert(feed(270, 4200000) == PNEU_EVENT_NONE);
    assert(feed(271, 4200000) == PNEU_EVENT_START);
    assert(feed(273, 4200000) == PNEU_EVENT_RESET); /* lost sample */
    assert(!state.active && !state.ready);
    assert(feed(274, 8388607) == PNEU_EVENT_RESET);
    pneu_features_init(&state);
    for (i = 0; i < 100; ++i)
        assert(pneu_features_feed(&state, 0xFFFFFFC0U+i, 0xFFFFFF00U+i*25U, 4000000) == PNEU_EVENT_NONE);
    assert(state.ready); /* sequence/time wrap is continuous */
    /* Analytic positive/negative triangular pulses: independently known widths,
       slopes and areas, without supplying the onset to the detector. */
    pneu_features_init(&state);
    for (i = 0; i < 60; ++i) feed(i, 4000000);
    {
        unsigned completed = 0;
        const float expected[12] = {200, 200, 250, 247.5f, 4, 1,
                                   200, 200, 250, 247.5f, 1, 1};
        for (i = 60; i < 125; ++i) {
            int u = (int)i-60;
            int delta = u <= 10 ? 20000*u : u <= 20 ? 20000*(20-u) :
                        u <= 30 ? -20000*(u-20) : u <= 40 ? -20000*(40-u) : 0;
            if (feed(i, 4000000+delta) == PNEU_EVENT_COMPLETE) {
                unsigned j;
                ++completed;
                for (j = 0; j < 12; ++j) {
                    float d = state.event.value[j]-expected[j];
                    assert(d > -.001f && d < .001f);
                }
            }
        }
        assert(completed == 1);
    }
    /* 30000-count trigger (TPU 85A legs): +20000 stays quiet, +35000 starts. */
    pneu_features_init(&state);
    for (i = 0; i < 60; ++i) feed(i, 4000000);
    for (i = 60; i < 63; ++i) assert(feed(i, 4020000) == PNEU_EVENT_NONE);
    assert(!state.active);
    pneu_features_init(&state);
    for (i = 0; i < 60; ++i) feed(i, 4000000);
    assert(feed(60, 4035000) == PNEU_EVENT_NONE);
    assert(feed(61, 4035000) == PNEU_EVENT_START);
    assert(state.active);
    assert(pneu_demo_class_for_zone(0) == PNEU_CLASS_UNKNOWN);
    assert(pneu_demo_class_for_zone(1) == PNEU_CLASS_TAIL);
    assert(pneu_demo_class_for_zone(2) == PNEU_CLASS_BACK);
    for (i = 3; i <= 6; ++i) assert(pneu_demo_class_for_zone((uint8_t)i) == PNEU_CLASS_LEGS);
    assert(pneu_demo_class_for_zone(7) == PNEU_CLASS_HEAD);
    assert(pneu_demo_class_for_zone(8) == PNEU_CLASS_UNKNOWN);
    test_detection_profiles();
    printf("Feature state %lu bytes; debounce, timeout, gap, saturation, wrap, class mapping, DINO/KRAG detection profiles PASS\n", (unsigned long)sizeof(state));
    return 0;
}
