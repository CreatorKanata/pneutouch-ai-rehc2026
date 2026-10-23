#ifndef PNEU_PRESSURE_MODEL_H
#define PNEU_PRESSURE_MODEL_H
#include "pressure_features.h"

typedef enum {
    PNEU_MODEL_ERR_NONE, PNEU_MODEL_ERR_STORE, PNEU_MODEL_ERR_NO_SLOT,
    PNEU_MODEL_ERR_RECORD, PNEU_MODEL_ERR_SELECT, PNEU_MODEL_ERR_AI
} pneu_model_error_t;

typedef struct {
    bool ready;
    uint32_t training_us, inference_us, failures, predictions;
    pneu_demo_class_t label;
    uint16_t input[12], scores[4]; /* Exact bf16 words for debugger/UART audit. */
    uint16_t training_examples, training_steps;
    /* FeRAM slot and the pool features the chip selected from it. */
    uint8_t slot, error, selected_count, selected[12];
    uint16_t store_count;
    /* From the slot header (written by PST1 TUNE): k chosen by cross-validation (0 =
       never tuned) and held-out recall percent per class (255 = class absent). */
    uint8_t tuned_k, recall[4], advice; /* advice: class 1..4 to collect more of, 0 none */
    uint16_t class_count[4];            /* records per class (label-1) of the last init */
    float fisher[14];
    char lcd_line2[17];
} pneu_model_status_t;
extern volatile pneu_model_status_t pneutouch_model;

/* Train instance 0 from the active FeRAM slot (boot, PAI1 exit, PST1 TRAIN). */
bool pneu_model_init(void);
#define PNEU_PHASE_STATS 0U
#define PNEU_PHASE_TRAIN 1U
#define PNEU_PHASE_TUNE 2U
/* Progress while pneu_model_init / pneu_model_tune block: phase 0 statistics
   (records read), phase 1 training (steps), both at phase start, every 8
   records/steps and at the end; phase 2 tuning (trainings finished of all,
   at the start and after every fold). NULL = none. */
typedef void (*pneu_model_progress_t)(uint8_t phase, uint16_t done, uint16_t total);
void pneu_model_set_progress(pneu_model_progress_t callback);
/* values: 12 event features; peak and trough magnitude in counts. */
pneu_demo_class_t pneu_model_predict(const float values[12], float positive_peak_counts,
                                     float trough_counts);
const char *pneu_class_name(pneu_demo_class_t label);
/* LCD line 2: one cell per pool feature (0xFF selected, ' ' not), then the
   selected count in the last two columns ("k4"; 10..12 as two digits); 16 spaces when nothing is selected
   (an init failure blanks it). */
void pneu_model_line2(const volatile pneu_model_status_t *status, char out[17]);
#endif
