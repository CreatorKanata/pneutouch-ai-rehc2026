/* Verify startup/recovery settling, timeouts, no invented samples and rollover. */
#include <assert.h>
#include <stdio.h>
#include "acquisition.h"
#include "pneu_config.h"

typedef struct { unsigned pulses; bool ready; } fake_t;
static bool high(void *p) { fake_t *f = p; return !f->ready || f->pulses >= 25; }
static bool pulse(void *p) { fake_t *f = p; ++f->pulses; return true; }
static acquisition_result_t read_at(acquisition_t *state, fake_t *fake, uint32_t now,
                                    pressure_sample_t *value)
{
    fake->pulses = 0;
    return acquisition_poll(state, now, value);
}

int main(void)
{
    fake_t fake = {0, false};
    hx710b_t sensor = {&fake, high, pulse, PNEU_HX_PULSES};
    acquisition_t state;
    pressure_sample_t value = {99, 99, 99};
    acquisition_init(&state, sensor, 0);
    assert(read_at(&state, &fake, PNEU_SENSOR_TIMEOUT_MS - 1, &value) == ACQ_WAITING);
    assert(read_at(&state, &fake, PNEU_SENSOR_TIMEOUT_MS, &value) == ACQ_TIMEOUT);
    assert(value.raw == 99 && fake.pulses == 0);
    fake.ready = true;
    uint32_t start = PNEU_SENSOR_TIMEOUT_MS + 25;
    assert(read_at(&state, &fake, start, &value) == ACQ_WAITING);
    assert(read_at(&state, &fake, start + PNEU_SETTLE_MS - 1, &value) == ACQ_WAITING);
    assert(read_at(&state, &fake, start + PNEU_SETTLE_MS, &value) == ACQ_SAMPLE);
    assert(value.sequence == 0 && value.raw == -1);
    fake.ready = false;
    start += PNEU_SETTLE_MS + PNEU_SENSOR_TIMEOUT_MS;
    assert(read_at(&state, &fake, start, &value) == ACQ_TIMEOUT);
    fake.ready = true;
    assert(read_at(&state, &fake, start + 1, &value) == ACQ_WAITING);
    assert(read_at(&state, &fake, start + 1 + PNEU_SETTLE_MS, &value) == ACQ_SAMPLE);
    assert(value.sequence == 1);
    start = UINT32_MAX - 100;
    acquisition_init(&state, sensor, start);
    assert(read_at(&state, &fake, start, &value) == ACQ_WAITING);
    assert(read_at(&state, &fake, start + PNEU_SETTLE_MS, &value) == ACQ_SAMPLE);
    assert(value.sequence == 0);
    puts("Acquisition: startup, timeout, recovery and clock wrap PASS");
    return 0;
}
