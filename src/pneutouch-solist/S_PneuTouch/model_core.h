/* Training pieces shared by pressure_model.c (boot training) and model_tune.c /
   model_screen.c (subset search). Internal to the model: the app and the protocols do not use it. */
#ifndef PNEU_MODEL_CORE_H
#define PNEU_MODEL_CORE_H
#include "pressure_model.h"
#include "feature_select.h"

#define PNEU_NO_FOLD 0xFFU /* train on every record */

/* Record index left out of training by fold (index % PNEU_TUNE_FOLDS == fold). */
bool pneu_core_held(uint16_t index, uint8_t fold);
/* Progress callback without the every-8 filter (tune phase). */
void pneu_core_progress(uint8_t phase, uint16_t done, uint16_t total);
/* Statistics pass over the training records of fold (phase 0 progress). */
uint8_t pneu_core_load_stats(uint8_t slot, uint16_t n, uint8_t fold);
/* Selection from those statistics, published to pneutouch_model (line 2 and mask too):
   best k (0 = Fisher auto), or exactly the features of mask in Fisher order. */
uint8_t pneu_core_choose(uint8_t k);
uint8_t pneu_core_choose_mask(uint16_t mask);
/* Fresh instance 0, EPOCHS passes over the training records of fold (phase 1 progress). */
uint8_t pneu_core_train(uint8_t slot, uint16_t n, uint8_t fold);
/* Bit index%8 of chosen[index/8]: record index is in the bitmap. */
bool pneu_core_chosen(const uint8_t *chosen, uint16_t index);
/* Same over the records in the bitmap only; used = how many of [0, n) are set. */
uint8_t pneu_core_train_chosen(uint8_t slot, uint16_t n, const uint8_t *chosen, uint16_t used);
/* Current selection + instance 0, same path as the live prediction; UNKNOWN on any fault. */
pneu_demo_class_t pneu_core_classify(const float pool[PNEU_POOL_COUNT]);
#endif
