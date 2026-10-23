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

typedef enum { PNEU_SLOT_EMPTY = 0, PNEU_SLOT_OPEN = 1, PNEU_SLOT_COMMITTED = 2 } pneu_slot_state_t;

typedef struct {
    uint8_t state, k, pool_count;
    uint16_t count;
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
/* CRC checked; works for OPEN or COMMITTED slots. */
bool pneu_store_get(uint8_t slot, uint16_t index, uint8_t *label, float pool[PNEU_POOL_COUNT]);
#endif
