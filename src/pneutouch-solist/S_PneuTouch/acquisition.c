/* Preserve pressure transients: no smoothing or artificial fixed-rate samples. */
#include "acquisition.h"
#include "pneu_config.h"

void acquisition_init(acquisition_t *state, hx710b_t sensor, uint32_t now)
{
    *state = (acquisition_t){sensor, now, now, 0, false, true};
}

acquisition_result_t acquisition_poll(acquisition_t *state, uint32_t now,
                                      pressure_sample_t *sample)
{
    int32_t raw;
    if (hx710b_try_read(&state->sensor, &raw)) {
        state->last_ready = now;
        /* This transfer selects the NEXT conversion's rate. Discard it. */
        if (!state->mode_selected) {
            state->mode_selected = true;
            state->settling_start = now;
            return ACQ_WAITING;
        }
        if (state->settling && (uint32_t)(now - state->settling_start) < PNEU_SETTLE_MS)
            return ACQ_WAITING;
        state->settling = false;
        *sample = (pressure_sample_t){state->sequence++, now, raw};
        return ACQ_SAMPLE;
    }
    /* Unsigned differences work across the millisecond counter rollover. */
    if ((uint32_t)(now - state->last_ready) >= PNEU_SENSOR_TIMEOUT_MS) {
        state->last_ready = now;
        state->mode_selected = false;
        state->settling = true;
        return ACQ_TIMEOUT;
    }
    return ACQ_WAITING;
}
