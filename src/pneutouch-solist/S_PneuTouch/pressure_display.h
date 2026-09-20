#ifndef PNEU_PRESSURE_DISPLAY_H
#define PNEU_PRESSURE_DISPLAY_H
#include "pressure_features.h"
typedef struct {
    bool ready, failed;
    pneu_demo_class_t label;
    uint32_t shown_ms, cleared_ms, updates;
} pneu_display_status_t;
extern volatile pneu_display_status_t pneutouch_display;
void pneu_display_init(void);
void pneu_display_poll(void);
void pneu_display_result(pneu_demo_class_t label);
void pneu_display_idle(const char *message);
#endif
