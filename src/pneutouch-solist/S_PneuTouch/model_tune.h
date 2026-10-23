/* On-chip choice of the feature count k by 4-fold cross-validation on the
   active FeRAM slot, and the "collect more of zone X" advice (PST1 TUNE). */
#ifndef PNEU_MODEL_TUNE_H
#define PNEU_MODEL_TUNE_H
#include <stdbool.h>
#include <stdint.h>

#define PNEU_TUNE_K_MIN 4U
#define PNEU_TUNE_K_MAX 8U
#define PNEU_TUNE_FOLDS 4U
#define PNEU_TUNE_MIN_RECORDS 8U
#define PNEU_ADVICE_RECALL_PCT 90U

typedef struct {
    bool done;                            /* last pneu_model_tune() completed */
    uint8_t error;                        /* pneu_model_error_t, 0 when done */
    uint8_t slot, best_k, advice;         /* advice: 1..4 class to collect more, 0 none */
    uint16_t count;                       /* records in the slot (= held-out predictions per k) */
    uint16_t correct[PNEU_TUNE_K_MAX+1U]; /* held-out correct per candidate k; 0 outside K_MIN..K_MAX */
    uint16_t confusion[4][4];             /* best k: [true label-1][predicted label-1] over all folds */
    uint8_t recall[4];                    /* percent for best k; 255 when the class has no records */
    uint32_t elapsed_us;
} pneu_tune_status_t;
extern volatile pneu_tune_status_t pneutouch_tune;

/* Blocks for tens of seconds. Trains instance 0 repeatedly: afterwards pneutouch_model.ready is false
   until pneu_model_init() runs again (TRAIN or leaving MODE 1). Writes tuned_k and recall to the
   slot header only on success. */
bool pneu_model_tune(void);
/* Pure: class 1..4 with the lowest recall if it is < PNEU_ADVICE_RECALL_PCT (ties: fewer records,
   then the lower label), 0 otherwise. 255 entries are ignored. */
uint8_t pneu_model_advice(const uint8_t recall[4], const uint16_t counts[4]);
#endif
