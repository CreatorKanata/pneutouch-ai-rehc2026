/* Portable HX710B pressure reader: GPIO callbacks keep it host-testable. */
#ifndef PNEU_HX710B_H
#define PNEU_HX710B_H
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    void *context;
    bool (*dout_high)(void *context);
    /* One timed pulse; sample DOUT and return only after SCK is low again.
       The adapter must prevent interrupts from extending SCK high past 50 us. */
    bool (*pulse)(void *context);
    uint8_t pulses;
} hx710b_t;

/* false means not ready/invalid mode/framing; value is untouched. No busy wait. */
bool hx710b_try_read(const hx710b_t *sensor, int32_t *value);
#endif
