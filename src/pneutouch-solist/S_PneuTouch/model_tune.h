/* On-chip choice of the feature subset by the Solist-AI learner itself (PST1 TUNE):
   screening of every subset of the Fisher shortlist on a small training set, 4-fold
   cross-validation of the best few against the Fisher-auto champion, and the
   "collect more of zone X" advice. Algorithm: docs/model-store.md. */
#ifndef PNEU_MODEL_TUNE_H
#define PNEU_MODEL_TUNE_H
#include <stdbool.h>
#include <stdint.h>

#define PNEU_TUNE_SHORTLIST 7U     /* best Fisher features whose subsets are screened */
#define PNEU_TUNE_MIN_SIZE 3U      /* smallest subset screened */
#define PNEU_SCREEN_PER_CLASS 10U  /* screening training records per class and pass */
#define PNEU_SCREEN_PASSES 2U
#define PNEU_TUNE_FINALISTS 5U     /* best screening scores that get the 4-fold check */
#define PNEU_TUNE_FOLDS 4U
#define PNEU_TUNE_MIN_RECORDS 8U
#define PNEU_ADVICE_RECALL_PCT 90U
/* A challenger must beat the champion by more than this share of the records (minimum 1). */
#define PNEU_TUNE_MARGIN_PCT 2U

typedef struct {
    bool done;                                /* last pneu_model_tune() completed */
    uint8_t error, slot, advice;              /* error: pneu_model_error_t; advice 1..4 or 0 */
    uint16_t count, mask, best_k;             /* records; chosen mask and its popcount */
    uint16_t champion_mask, champion_correct; /* Fisher-auto set and its 4-fold hits */
    uint16_t correct;                         /* 4-fold hits of the chosen mask */
    /* Best screening results (mask 0 = unused entry), best first. */
    uint16_t finalist_mask[PNEU_TUNE_FINALISTS], finalist_screen[PNEU_TUNE_FINALISTS];
    uint16_t finalist_correct[PNEU_TUNE_FINALISTS];
    uint16_t candidates, screened;            /* subsets enumerated; records judged per pass */
    uint16_t confusion[4][4];                 /* chosen mask: [true label-1][predicted label-1], all folds */
    uint8_t recall[4];                        /* percent of the chosen mask; 255 when the class has no records */
    uint32_t elapsed_us;
} pneu_tune_status_t;
extern volatile pneu_tune_status_t pneutouch_tune;

/* Blocks for minutes. Trains instance 0 repeatedly: afterwards pneutouch_model.ready is false
   until pneu_model_init() runs again (TRAIN or leaving MODE 1). Writes the mask and recall to
   the slot header only on success. */
bool pneu_model_tune(void);
/* Pure: class 1..4 with the lowest recall if it is < PNEU_ADVICE_RECALL_PCT (ties: fewer records,
   then the lower label), 0 otherwise. 255 entries are ignored. */
uint8_t pneu_model_advice(const uint8_t recall[4], const uint16_t counts[4]);
#endif
