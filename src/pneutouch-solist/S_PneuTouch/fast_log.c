/* log(x) = (e + log2(m)) * ln2 with m in [1, 2): the top 8 mantissa bits pick a
   table segment, the low 15 bits interpolate inside it. Every step is its own
   statement so a host compiler cannot fuse a multiply-add the chip does not have. */
#include "fast_log.h"
#include "fast_log_table.h"
#include <stdint.h>
#include <string.h>

#define LN2_F 0.69314718f
#define EXPONENT_BIAS 127
#define DENORMAL_SCALE 16777216.0f /* 2^24 */
#define DENORMAL_SHIFT 24
#define FRACTION_SCALE (1.0f/32768.0f)

float pneu_log(float x)
{
    uint32_t bits;
    int32_t e;
    unsigned i;
    float f, step, offset, log2m, sum;
    memcpy(&bits, &x, sizeof(bits));
    e = (int32_t)((bits >> 23) & 0xFFU)-EXPONENT_BIAS;
    if (e == -EXPONENT_BIAS) { /* denormal: normalize first */
        float scaled = x*DENORMAL_SCALE;
        memcpy(&bits, &scaled, sizeof(bits));
        e = (int32_t)((bits >> 23) & 0xFFU)-EXPONENT_BIAS-DENORMAL_SHIFT;
    }
    i = (unsigned)((bits >> 15) & 0xFFU);
    f = (float)(bits & 0x7FFFU)*FRACTION_SCALE;
    step = pneu_log2_table[i+1U]-pneu_log2_table[i];
    offset = step*f;
    log2m = pneu_log2_table[i]+offset;
    sum = (float)e+log2m;
    return sum*LN2_F;
}
