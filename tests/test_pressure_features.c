#include <assert.h>
#include <stdio.h>
#include "pressure_features.h"

static pneu_features_t state;
static pneu_event_result_t feed(unsigned i, int32_t raw)
{
    return pneu_features_feed(&state, i, i*25U, raw);
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
    assert(pneu_demo_class_for_zone(0) == PNEU_CLASS_UNKNOWN);
    assert(pneu_demo_class_for_zone(1) == PNEU_CLASS_TAIL);
    assert(pneu_demo_class_for_zone(2) == PNEU_CLASS_BACK);
    for (i = 3; i <= 6; ++i) assert(pneu_demo_class_for_zone((uint8_t)i) == PNEU_CLASS_LEGS);
    assert(pneu_demo_class_for_zone(7) == PNEU_CLASS_HEAD);
    assert(pneu_demo_class_for_zone(8) == PNEU_CLASS_UNKNOWN);
    printf("Feature state %lu bytes; debounce, timeout, gap, saturation, wrap, class mapping PASS\n", (unsigned long)sizeof(state));
    return 0;
}
