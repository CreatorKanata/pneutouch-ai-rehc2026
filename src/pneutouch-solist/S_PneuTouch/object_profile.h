/* Object profiles: which zones a FeRAM slot was recorded for (docs/model-store.md,
   "物体プロファイル"). The object lives in the slot name: a name starting with a known
   tag (case-sensitive) belongs to that object, any other name (legacy slots) to DINO.
   Labels stay 1..4 in records and over UART; an object uses labels 1..classes. */
#ifndef PNEU_OBJECT_PROFILE_H
#define PNEU_OBJECT_PROFILE_H
#include <stdint.h>

typedef enum { PNEU_OBJECT_DINO = 0, PNEU_OBJECT_KRAG = 1 } pneu_object_t;
#define PNEU_OBJECT_COUNT 2U
#define PNEU_OBJECT_TAG 4U /* characters of every tag */

typedef struct { const char *tag; uint8_t classes; const char *name[4]; } pneu_object_profile_t;

/* An out-of-range object reads as DINO. */
const pneu_object_profile_t *pneu_object(pneu_object_t object);
pneu_object_t pneu_object_of_name(const char *name);
/* Bit (label-1) per class of the object: 0x0F (DINO) or 0x03 (KRAG). */
uint8_t pneu_object_class_mask(pneu_object_t object);
/* Zone name of label; "UNKNOWN" outside 1..classes. */
const char *pneu_object_class_name(pneu_object_t object, uint8_t label);
/* Tag + suffix, cut at 8 characters: (KRAG, "BTN") -> "KRAGBTN". */
void pneu_object_slot_name(pneu_object_t object, const char *suffix, char out[9]);
#endif
