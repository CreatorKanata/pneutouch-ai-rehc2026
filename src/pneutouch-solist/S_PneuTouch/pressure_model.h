#ifndef PNEU_PRESSURE_MODEL_H
#define PNEU_PRESSURE_MODEL_H
#include "pressure_features.h"
typedef struct {
    bool ready;
    uint32_t training_us, inference_us, failures, predictions;
    pneu_demo_class_t label;
    uint16_t input[12], scores[4]; /* Exact bf16 words for debugger/UART audit. */
    uint16_t training_examples, training_steps;
} pneu_model_status_t;
extern volatile pneu_model_status_t pneutouch_model;
/* Train the fixed reference set on the chip at boot (also after PAI1 mode). */
bool pneu_model_init(void);
pneu_demo_class_t pneu_model_predict(const float values[12]);
bool pneu_model_transform(const float values[12], uint16_t words[12]);
const char *pneu_class_name(pneu_demo_class_t label);
#endif
