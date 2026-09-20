/* Host test: verify the on-wire bit order, sign boundaries and mode pulses. */
#include <assert.h>
#include <stdio.h>
#include "hx710b.h"

typedef struct { uint32_t bits; unsigned clocks; bool busy, stuck_low; } fake_t;
static bool busy(void *context) {
    fake_t *f = context;
    return f->busy || (f->clocks >= 25 && !f->stuck_low);
}
static bool pulse(void *context)
{
    fake_t *fake = context;
    unsigned index = fake->clocks++;
    return index < 24 ? (fake->bits & (1U << (23U - index))) != 0 : true;
}

int main(void)
{
    const uint32_t bits[] = {0, 1, 0x7FFFFF, 0x800000, 0xFFFFFF, 0xA53C19};
    const int32_t expected[] = {0, 1, 8388607, -8388608, -1, -5948391};
    for (unsigned mode = 25; mode <= 27; mode += 2) {
        for (unsigned i = 0; i < sizeof(bits) / sizeof(bits[0]); ++i) {
            fake_t fake = {bits[i], 0, false, false};
            hx710b_t sensor = {&fake, busy, pulse, (uint8_t)mode};
            int32_t result = 123;
            assert(hx710b_try_read(&sensor, &result));
            assert(result == expected[i]);
            assert(fake.clocks == mode);
        }
    }
    fake_t fake = {0, 0, true, false};
    hx710b_t sensor = {&fake, busy, pulse, 27};
    int32_t value = 99;
    assert(!hx710b_try_read(&sensor, &value));
    assert(value == 99 && fake.clocks == 0);
    sensor.pulses = 27;
    fake.busy = false;
    fake.stuck_low = true;
    assert(!hx710b_try_read(&sensor, &value));
    assert(value == 99 && fake.clocks == 27);
    fake.clocks = 0;
    fake.stuck_low = false;
    sensor.pulses = 26; /* Voltage-difference mode must not be reported as pressure. */
    assert(!hx710b_try_read(&sensor, &value));
    assert(value == 99 && fake.clocks == 0);
    puts("HX710B: bit order, signed limits, 25/27 clocks and not-ready PASS");
    return 0;
}
