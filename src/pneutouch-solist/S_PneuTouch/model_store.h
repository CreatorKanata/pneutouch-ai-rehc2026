/* Training records in the board FeRAM (MB85RS2MTA): a directory and 4 slots.
   Layout: docs/model-store.md. Uses only the board_store_* boundary. */
#ifndef PNEU_MODEL_STORE_H
#define PNEU_MODEL_STORE_H
#include <stdbool.h>
#include <stdint.h>

/* Same value in feature_select.h: 12 event features, positive peak, trough. */
#define PNEU_POOL_COUNT 14U
#define PNEU_STORE_SLOTS 4U
#define PNEU_STORE_MAX_RECORDS 1000U
#define PNEU_STORE_NAME 8U
#define PNEU_RECALL_ABSENT 255U /* recall entry of a class without records */

typedef enum { PNEU_SLOT_EMPTY = 0, PNEU_SLOT_OPEN = 1, PNEU_SLOT_COMMITTED = 2 } pneu_slot_state_t;

typedef struct {
    uint8_t state, k, pool_count;
    /* tuned_k 0 = never tuned; recall percent per class (label-1), only meaningful when tuned. */
    uint8_t tuned_k, recall[4];
    /* Header version 2: bit j = pool feature j chosen by TUNE (0 = none; version 1 slots read 0). */
    uint16_t mask, count;
    char name[PNEU_STORE_NAME+1U];
    bool valid;
} pneu_store_slot_t;

/* Reads the directory; writes a fresh one (active 0) if invalid.
   false on FeRAM fault (write-then-read mismatch). */
bool pneu_store_init(void);
uint8_t pneu_store_active(void);
bool pneu_store_use(uint8_t slot);
/* valid=false (state/count 0) when the header is bad; returns info->valid. */
bool pneu_store_slot_info(uint8_t slot, pneu_store_slot_t *info);
/* Header state=OPEN, count=0; k 0..12; name up to 8 printable characters. */
bool pneu_store_open(uint8_t slot, uint8_t k, const char *name);
/* State must be OPEN; label 1..4; every value finite and > 0; count < 1000. */
bool pneu_store_put(uint8_t slot, uint8_t label, const float pool[PNEU_POOL_COUNT]);
bool pneu_store_commit(uint8_t slot);       /* OPEN -> COMMITTED, count >= 4 */
bool pneu_store_erase(uint8_t slot);        /* EMPTY, count 0 */
/* COMMITTED -> OPEN (SW4 recording appends), keeping count, name, k, mask, tuned_k, recall. */
bool pneu_store_reopen(uint8_t slot);
/* OPEN with count >= 1: count - 1 (header rewrite only; the record bytes stay). */
bool pneu_store_undo(uint8_t slot);
/* OPEN slot, count <= current count: header rewrite only (SW4 discard). */
bool pneu_store_truncate(uint8_t slot, uint16_t count);
/* COMMITTED slot only; mask nonzero, pool bits 0..13 only, at most 12 set (tuned_k = its
   popcount); recall percent 0..100 or 255. Keeps k/state/count/name; writes header version 2. */
bool pneu_store_set_tuning(uint8_t slot, uint16_t mask, const uint8_t recall[4]);
/* CRC checked; works for OPEN or COMMITTED slots. Uses a one-entry header cache
   (any header write or pneu_store_init drops it; pneu_store_slot_info always reads FeRAM). */
bool pneu_store_get(uint8_t slot, uint16_t index, uint8_t *label, float pool[PNEU_POOL_COUNT]);
#endif
