/* Stage 1 of PST1 TUNE (model_screen.c): every subset of the shortlist is trained on
   PNEU_SCREEN_PER_CLASS records per class and judged on all the other records.
   Internal to the tuning; the pure helpers are exposed for the host tests. */
#ifndef PNEU_MODEL_SCREEN_H
#define PNEU_MODEL_SCREEN_H
#include <stdbool.h>
#include <stdint.h>
#include "model_store.h"

#define PNEU_SCREEN_BITMAP ((PNEU_STORE_MAX_RECORDS+7U)/8U)

/* Class ordinal (0-based, slot order) of the j-th screening record of a class with n_c
   records: j when n_c <= PER_CLASS (all taken), else offset + j*n_c/PER_CLASS with
   offset = pass*ceil(n_c/(2*PER_CLASS)). */
uint16_t pneu_screen_ordinal(uint16_t n_c, uint8_t pass, uint8_t j);
/* Bitmap (bit i%8 of chosen[i/8]) of the screening training records of pass; *used = how many. */
uint8_t pneu_screen_build(uint8_t slot, uint16_t n, uint8_t pass, uint8_t chosen[PNEU_SCREEN_BITMAP],
                          uint16_t *used);
uint8_t pneu_mask_size(uint16_t mask);
/* Subsets of a shortlist of size features with PNEU_TUNE_MIN_SIZE..size members. */
uint16_t pneu_screen_count(uint8_t size);
/* Next subset (shortlist bit i = shortlist[i]) by size ascending, then value ascending;
   start with *bits = 0. false after the last one. */
bool pneu_screen_next(uint8_t size, uint8_t *bits);
uint16_t pneu_screen_pool_mask(uint8_t bits, const uint8_t shortlist[]);
/* Ranking rule of screening and finals: more hits, then fewer features, then the lower mask. */
bool pneu_tune_better(uint16_t hits_a, uint16_t mask_a, uint16_t hits_b, uint16_t mask_b);
/* Inserts (mask, score) into the sorted PNEU_TUNE_FINALISTS list (mask 0 = empty entry). */
void pneu_screen_keep(volatile uint16_t masks[], volatile uint16_t scores[], uint16_t mask, uint16_t score);
/* Stage 1 on the all-record statistics already loaded: fills candidates, screened and the
   finalists of pneutouch_tune (progress phase 2). */
uint8_t pneu_screen_run(uint8_t slot, uint16_t n, const uint8_t shortlist[], uint8_t size);
#endif
