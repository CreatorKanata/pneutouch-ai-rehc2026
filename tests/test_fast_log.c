/* fast_log.c: accuracy against libm, exact powers of two, monotonicity. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "fast_log.h"

#define SWEEP_POINTS 50000U
#define MAX_ERROR 4e-6
#define LN2_F 0.69314718f

static void test_sweep(void)
{
    double lo = log(1e-4), hi = log(1e8), worst = 0.0;
    float previous = -INFINITY;
    for (unsigned n = 0; n < SWEEP_POINTS; ++n) {
        float x = (float)exp(lo+(hi-lo)*n/(SWEEP_POINTS-1U)), got = pneu_log(x);
        double error = fabs((double)got-(double)(float)log((double)x));
        if (error > worst) worst = error;
        assert(error <= MAX_ERROR);
        assert(got >= previous); /* monotone non-decreasing */
        previous = got;
    }
    printf("fast_log: max error %.3g over [1e-4, 1e8]\n", worst);
}

static void test_exact_points(void)
{
    assert(pneu_log(1.0f) == 0.0f);
    for (int e = -40; e <= 60; ++e) assert(pneu_log(ldexpf(1.0f, e)) == (float)e*LN2_F);
    assert(fabsf(pneu_log(2.0f)-0.69314718f) <= 1e-7f);
    assert(fabs((double)pneu_log(ldexpf(1.5f, -140))-log(ldexp(1.5, -140))) <= 1e-4); /* denormal */
    assert(fabs((double)pneu_log(16777215.0f)-log(16777215.0)) <= MAX_ERROR*4.0);
}

int main(void)
{
    test_sweep();
    test_exact_points();
    puts("Fast log: libm agreement, exact powers of two, monotone, denormal PASS");
    return 0;
}
