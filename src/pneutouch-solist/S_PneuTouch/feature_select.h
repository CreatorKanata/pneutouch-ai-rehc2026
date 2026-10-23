/* On-chip feature selection: Fisher ranking of the 14 pool features (log),
   normalization and the deterministic training order. Pure float32 math. */
#ifndef PNEU_FEATURE_SELECT_H
#define PNEU_FEATURE_SELECT_H
#include <stdbool.h>
#include <stdint.h>

/* Same value in model_store.h: 12 event features, positive peak, trough. */
#define PNEU_POOL_COUNT 14U
#define PNEU_SELECT_MAX 12U
#define PNEU_TRAIN_EPOCHS 4U
#define PNEU_CLASS_COUNT 4U

typedef struct {
    uint16_t n[PNEU_CLASS_COUNT];                  /* records per class (label-1) */
    float sum[PNEU_CLASS_COUNT][PNEU_POOL_COUNT], sumsq[PNEU_CLASS_COUNT][PNEU_POOL_COUNT];
} pneu_stats_t;

typedef struct {
    uint8_t count, index[PNEU_SELECT_MAX];         /* selected pool indices, best first */
    float fisher[PNEU_POOL_COUNT], mean[PNEU_POOL_COUNT], scale[PNEU_POOL_COUNT];
    float inv_scale[PNEU_POOL_COUNT];              /* 1/scale (0 when constant): no division per word */
} pneu_selection_t;

void pneu_stats_reset(pneu_stats_t *s);
/* Applies logf; false (and no change) if the label or any value is invalid. */
bool pneu_stats_add(pneu_stats_t *s, uint8_t label, const float pool[PNEU_POOL_COUNT]);
/* class_mask: bit (label-1) per class taking part (primary 0xF, HEAD/LEGS 0xC).
   k = 0 picks automatically, 1..12 keeps the best k. */
bool pneu_select(const pneu_stats_t *s, uint8_t class_mask, uint8_t k, pneu_selection_t *out);
/* Pool indices by Fisher score, best first; ties keep the lower index first. */
void pneu_rank(const float fisher[PNEU_POOL_COUNT], uint8_t order[PNEU_POOL_COUNT]);
/* log, normalize, bf16 truncate; words[count..11] = 0. false on non-finite. */
bool pneu_select_words(const pneu_selection_t *sel, const float pool[PNEU_POOL_COUNT],
                       uint16_t words[PNEU_SELECT_MAX]);
/* Record visited at step i of an epoch (stride permutation, no RAM table). */
uint16_t pneu_train_index(uint16_t n, uint8_t epoch, uint16_t i);
#endif
