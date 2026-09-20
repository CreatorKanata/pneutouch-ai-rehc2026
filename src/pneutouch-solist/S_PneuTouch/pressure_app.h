/* PneuTouch Phase 0: single-threaded acquisition and UART output. */
#ifndef PNEU_PRESSURE_APP_H
#define PNEU_PRESSURE_APP_H
#include <stdint.h>

typedef enum {
    PNEU_STARTING, PNEU_SETTLING, PNEU_STREAMING, PNEU_SENSOR_TIMEOUT
} pneutouch_state_t;

/* Watch this symbol in PneutouchAi Debug. raw is valid only after samples > 0.
   After a timeout, the previous raw remains visible; state indicates staleness. */
typedef struct {
    pneutouch_state_t state;
    int32_t raw;
    uint32_t samples, milliseconds, timeouts;
} pneutouch_status_t;
extern volatile pneutouch_status_t pneutouch_status;

void pressure_app_init(void);
/* Call repeatedly from main; no fabricated samples when the sensor is busy. */
void pressure_app_poll(void);
#endif
