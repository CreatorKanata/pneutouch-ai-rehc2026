/* Natural logarithm for the feature pipeline without libm: a 257-entry log2
   table with linear interpolation (interpolation error ~2.2e-6; within 4e-6 of logf
   including float32 rounding). Soft-float
   logf costs ~0.30 ms per call on the Cortex-M0+; this is a few multiplies.
   tools/feature_select.py mirrors it bit-exactly. */
#ifndef PNEU_FAST_LOG_H
#define PNEU_FAST_LOG_H

/* x finite and > 0 (callers check); other inputs return an unspecified value. */
float pneu_log(float x);
#endif
