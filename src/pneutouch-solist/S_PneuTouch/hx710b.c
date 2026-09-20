/* HX710B: MSB-first signed 24-bit output; 25/27 clocks select pressure. */
#include "hx710b.h"

bool hx710b_try_read(const hx710b_t *sensor, int32_t *value)
{
    uint32_t bits = 0;
    if ((sensor->pulses != 25 && sensor->pulses != 27) ||
        sensor->dout_high(sensor->context)) {
        return false;
    }
    for (unsigned i = 0; i < 24; ++i) {
        bits = (bits << 1) | (sensor->pulse(sensor->context) ? 1U : 0U);
    }
    for (unsigned i = 24; i < sensor->pulses; ++i) {
        (void)sensor->pulse(sensor->context);
    }
    /* DOUT must return high after pulse 25. A short to ground is not raw=0. */
    if (!sensor->dout_high(sensor->context)) return false;
    /* Arithmetic extension avoids implementation-defined unsigned casts. */
    *value = (bits & 0x800000U) ? (int32_t)bits - 0x1000000 : (int32_t)bits;
    return true;
}
