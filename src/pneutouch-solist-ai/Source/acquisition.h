/* Phase 0 acquisition state: settle after startup/recovery and report timeouts. */
#ifndef PNEU_ACQUISITION_H
#define PNEU_ACQUISITION_H
#include "hx710b.h"

typedef enum { ACQ_WAITING, ACQ_SAMPLE, ACQ_TIMEOUT } acquisition_result_t;
typedef struct {
    hx710b_t sensor;
    uint32_t last_ready, settling_start, sequence;
    bool mode_selected, settling;
} acquisition_t;
typedef struct { uint32_t sequence, milliseconds; int32_t raw; } pressure_sample_t;

void acquisition_init(acquisition_t *state, hx710b_t sensor, uint32_t now);
acquisition_result_t acquisition_poll(acquisition_t *state, uint32_t now,
                                      pressure_sample_t *sample);
#endif
