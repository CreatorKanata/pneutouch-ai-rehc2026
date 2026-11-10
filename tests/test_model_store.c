/* FeRAM layout, verification and state rules of model_store.c. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "model_store.h"
#include "crc16.h"
#include "fram_stub.h"

#define SLOT_ADDR(n) (0x2000U+(n)*0xF000U)
#define RECORD_ADDR(n, i) (SLOT_ADDR(n)+32U+(i)*60U)

static void pool_for(unsigned seed, float pool[PNEU_POOL_COUNT])
{
    for (unsigned j = 0; j < PNEU_POOL_COUNT; ++j) pool[j] = (float)(seed*16U+j+1U)*0.5f;
}
static uint16_t le16(uint32_t addr) { return (uint16_t)(fram[addr] | fram[addr+1] << 8); }

static void test_directory(void)
{
    memset(fram, 0xff, sizeof(fram));
    assert(pneu_store_init() && pneu_store_active() == 0);
    assert(!memcmp(fram+0x1000, "PNST", 4) && le16(0x1004) == 1 && fram[0x1006] == 0);
    assert(le16(0x1008) == pneu_crc16(fram+0x1000, 8));
    for (unsigned i = 0; i < 0x1000U; ++i) assert(fram[i] == 0xff); /* legacy region untouched */
    assert(pneu_store_use(2) && pneu_store_active() == 2);
    assert(pneu_store_init() && pneu_store_active() == 2); /* persists */
    assert(!pneu_store_use(4) && pneu_store_active() == 2);
    fram[0x1006] = 3; /* corrupt: CRC no longer matches -> fresh directory */
    assert(pneu_store_init() && pneu_store_active() == 0);
    memset(fram+0x1000, 0, 16); fram_drop_writes = true;
    assert(!pneu_store_init()); /* write-then-read mismatch */
    fram_drop_writes = false;
    assert(pneu_store_init());
}

static void test_round_trip(void)
{
    pneu_store_slot_t info;
    float pool[PNEU_POOL_COUNT], back[PNEU_POOL_COUNT];
    uint8_t label;
    assert(!pneu_store_slot_info(0, &info) && !info.valid && info.state == 0 && info.count == 0);
    assert(!pneu_store_open(0, 13, "x") && !pneu_store_open(0, 0, "toolongnm") && !pneu_store_open(0, 0, "a,b"));
    assert(!pneu_store_open(4, 0, "x"));
    pool_for(0, pool);
    assert(!pneu_store_put(0, 1, pool)); /* never opened */
    assert(pneu_store_open(0, 3, "dino"));
    assert(!memcmp(fram+SLOT_ADDR(0), "PNSL", 4) && fram[SLOT_ADDR(0)+6] == 14 && fram[SLOT_ADDR(0)+7] == 3);
    assert(!memcmp(fram+SLOT_ADDR(0)+12, "dino    ", 8));
    assert(le16(SLOT_ADDR(0)+4) == 2 && le16(SLOT_ADDR(0)+24) == 0); /* header version 2, mask 0 */
    assert(le16(SLOT_ADDR(0)+30) == pneu_crc16(fram+SLOT_ADDR(0), 30));
    assert(pneu_store_slot_info(0, &info) && info.state == PNEU_SLOT_OPEN && info.count == 0 && info.mask == 0);
    assert(info.k == 3 && info.pool_count == 14 && !strcmp(info.name, "dino"));
    assert(!pneu_store_put(0, 0, pool) && !pneu_store_put(0, 5, pool));
    pool[3] = NAN; assert(!pneu_store_put(0, 1, pool));
    pool[3] = INFINITY; assert(!pneu_store_put(0, 1, pool));
    pool[3] = 0.0f; assert(!pneu_store_put(0, 1, pool));
    pool[3] = -1.0f; assert(!pneu_store_put(0, 1, pool));
    for (unsigned i = 0; i < 3; ++i) { pool_for(i, pool); assert(pneu_store_put(0, (uint8_t)(i+1), pool)); }
    assert(!pneu_store_commit(0)); /* fewer than 4 */
    pool_for(3, pool); assert(pneu_store_put(0, 4, pool));
    assert(fram[RECORD_ADDR(0, 3)] == 4 && le16(RECORD_ADDR(0, 3)+58) == pneu_crc16(fram+RECORD_ADDR(0, 3), 58));
    assert(pneu_store_get(0, 2, &label, back) && label == 3); /* OPEN slots are readable */
    assert(pneu_store_commit(0) && !pneu_store_commit(0));
    assert(pneu_store_slot_info(0, &info) && info.state == PNEU_SLOT_COMMITTED && info.count == 4);
    assert(!pneu_store_put(0, 1, pool)); /* committed */
    for (unsigned i = 0; i < 4; ++i) {
        pool_for(i, pool);
        assert(pneu_store_get(0, (uint16_t)i, &label, back) && label == i+1U);
        assert(!memcmp(pool, back, sizeof(pool)));
    }
    assert(!pneu_store_get(0, 4, &label, back));
    fram[RECORD_ADDR(0, 1)+20] ^= 0x01U; /* one flipped bit in a float */
    assert(!pneu_store_get(0, 1, &label, back));
    fram[RECORD_ADDR(0, 1)+20] ^= 0x01U;
    assert(pneu_store_get(0, 1, &label, back));
    fram[SLOT_ADDR(0)+11] ^= 0x01U; /* header count corrupted */
    assert(!pneu_store_slot_info(0, &info) && !info.valid && !pneu_store_get(0, 0, &label, back));
    fram[SLOT_ADDR(0)+11] ^= 0x01U;
    assert(pneu_store_erase(0) && pneu_store_slot_info(0, &info) && info.state == PNEU_SLOT_EMPTY && info.count == 0);
    assert(!pneu_store_get(0, 0, &label, back) && !pneu_store_commit(0));
}

static void test_full_and_independent(void)
{
    pneu_store_slot_t info;
    float pool[PNEU_POOL_COUNT], back[PNEU_POOL_COUNT];
    uint8_t label;
    assert(pneu_store_open(1, 0, ""));
    pool_for(7, pool);
    for (unsigned i = 0; i < PNEU_STORE_MAX_RECORDS; ++i) assert(pneu_store_put(1, (uint8_t)(i%4U+1U), pool));
    assert(!pneu_store_put(1, 1, pool)); /* full at 1000 */
    assert(pneu_store_slot_info(1, &info) && info.count == 1000 && !strcmp(info.name, ""));
    assert(RECORD_ADDR(1, 1000) <= SLOT_ADDR(2));
    for (uint8_t s = 0; s < PNEU_STORE_SLOTS; ++s) {
        if (s == 1) continue;
        assert(pneu_store_open(s, s, "s"));
        for (unsigned i = 0; i < 4; ++i) { pool_for(s*10U+i, pool); assert(pneu_store_put(s, (uint8_t)(s+1U), pool)); }
        assert(pneu_store_commit(s));
    }
    assert(RECORD_ADDR(3, 1023) <= 0x3E000U);
    for (uint8_t s = 0; s < PNEU_STORE_SLOTS; ++s) {
        if (s == 1) continue;
        for (unsigned i = 0; i < 4; ++i) {
            pool_for(s*10U+i, pool);
            assert(pneu_store_get(s, (uint16_t)i, &label, back) && label == s+1U && !memcmp(pool, back, sizeof(pool)));
        }
        assert(pneu_store_slot_info(s, &info) && info.k == s && info.count == 4);
    }
    assert(pneu_store_erase(2));
    assert(pneu_store_slot_info(3, &info) && info.count == 4 && pneu_store_slot_info(1, &info) && info.count == 1000);
    fram_drop_writes = true;
    assert(!pneu_store_erase(3) && !pneu_store_use(1)); /* verification catches a dead bus */
    fram_drop_writes = false;
}

static void fill_slot(uint8_t slot, uint8_t k, const char *name, unsigned count)
{
    float pool[PNEU_POOL_COUNT];
    assert(pneu_store_open(slot, k, name));
    for (unsigned i = 0; i < count; ++i) { pool_for(i, pool); assert(pneu_store_put(slot, (uint8_t)(i%4U+1U), pool)); }
}

/* Rewrites the header CRC at crc_at (24: version 1, 30: version 2) after a hand edit. */
static void fix_crc(uint8_t slot, unsigned crc_at)
{
    uint16_t crc = pneu_crc16(fram+SLOT_ADDR(slot), crc_at);
    fram[SLOT_ADDR(slot)+crc_at] = (uint8_t)crc; fram[SLOT_ADDR(slot)+crc_at+1U] = (uint8_t)(crc >> 8);
}

static void test_tuning(void)
{
    static const uint8_t recall[4] = {100, 50, 255, 0}, bad[4] = {100, 101, 0, 0};
    pneu_store_slot_t info;
    memset(fram, 0xff, sizeof(fram));
    assert(pneu_store_init());
    assert(!pneu_store_set_tuning(0, 0x0031, recall)); /* EMPTY (never written) */
    fill_slot(0, 3, "tune", 4);
    assert(pneu_store_slot_info(0, &info) && info.tuned_k == 0 && info.mask == 0 && !info.recall[0]);
    assert(!pneu_store_set_tuning(0, 0x0031, recall)); /* OPEN */
    assert(pneu_store_commit(0));
    assert(!pneu_store_set_tuning(0, 0, recall) && !pneu_store_set_tuning(0, 0x4001, recall));
    assert(!pneu_store_set_tuning(0, 0x8001, recall) && !pneu_store_set_tuning(0, 0x1FFF, recall)); /* 13 set */
    assert(!pneu_store_set_tuning(0, 0x0031, bad) && !pneu_store_set_tuning(4, 0x0031, recall));
    assert(pneu_store_set_tuning(0, 0x2031, recall)); /* features 0, 4, 5, 13 */
    assert(fram[SLOT_ADDR(0)+9] == 4 && le16(SLOT_ADDR(0)+24) == 0x2031 && !memcmp(fram+SLOT_ADDR(0)+20, recall, 4));
    assert(le16(SLOT_ADDR(0)+30) == pneu_crc16(fram+SLOT_ADDR(0), 30));
    for (unsigned i = 26; i < 30; ++i) assert(fram[SLOT_ADDR(0)+i] == 0); /* reserved */
    assert(pneu_store_slot_info(0, &info) && info.tuned_k == 4 && info.mask == 0x2031 && !memcmp(info.recall, recall, 4));
    assert(info.k == 3 && info.state == PNEU_SLOT_COMMITTED && info.count == 4 && !strcmp(info.name, "tune"));
    assert(pneu_store_set_tuning(0, 0x0FFF, recall) && pneu_store_slot_info(0, &info) && info.tuned_k == 12);
    /* Byte 9 out of range (CRC fixed up) makes the header invalid; so do recall 101 and mask bit 14. */
    fram[SLOT_ADDR(0)+9] = 13; fix_crc(0, 30);
    assert(!pneu_store_slot_info(0, &info) && !info.valid);
    fram[SLOT_ADDR(0)+9] = 12; fram[SLOT_ADDR(0)+21] = 101; fix_crc(0, 30);
    assert(!pneu_store_slot_info(0, &info));
    fram[SLOT_ADDR(0)+21] = 50; fram[SLOT_ADDR(0)+25] = 0x4F; fix_crc(0, 30);
    assert(!pneu_store_slot_info(0, &info));
    fram[SLOT_ADDR(0)+25] = 0x0F; fix_crc(0, 30);
    assert(pneu_store_slot_info(0, &info) && info.mask == 0x0FFF);
    /* A new OPEN resets the tuning. */
    fill_slot(0, 0, "again", 0);
    assert(pneu_store_slot_info(0, &info) && info.tuned_k == 0 && info.mask == 0 && !info.recall[1]);
    fram_drop_writes = true;
    assert(!pneu_store_set_tuning(0, 0x0031, recall));
    fram_drop_writes = false;
}

/* A slot written by version 1 firmware (CRC at 24, no mask) still loads and is upgraded on the next write. */
static void test_version1_header(void)
{
    static const uint8_t recall[4] = {90, 80, 70, 60};
    pneu_store_slot_t info;
    float back[PNEU_POOL_COUNT];
    uint8_t label;
    memset(fram, 0xff, sizeof(fram));
    assert(pneu_store_init());
    fill_slot(1, 0, "old", 5);
    assert(pneu_store_commit(1));
    fram[SLOT_ADDR(1)+4] = 1; fram[SLOT_ADDR(1)+9] = 6; /* version 1 with tuned_k 6 */
    memset(fram+SLOT_ADDR(1)+24, 0, 8); fix_crc(1, 24);
    assert(pneu_store_slot_info(1, &info) && info.tuned_k == 6 && info.mask == 0 && info.count == 5);
    assert(pneu_store_get(1, 4, &label, back) && label == 1);
    fram[SLOT_ADDR(1)+24] = 0x77; /* the CRC field of version 1 */
    assert(!pneu_store_slot_info(1, &info));
    fix_crc(1, 24);
    fram[SLOT_ADDR(1)+4] = 3; fix_crc(1, 30); /* unknown version */
    assert(!pneu_store_slot_info(1, &info));
    fram[SLOT_ADDR(1)+4] = 1; memset(fram+SLOT_ADDR(1)+24, 0, 8); fix_crc(1, 24);
    assert(pneu_store_set_tuning(1, 0x0006, recall));
    assert(le16(SLOT_ADDR(1)+4) == 2 && le16(SLOT_ADDR(1)+30) == pneu_crc16(fram+SLOT_ADDR(1), 30));
    assert(pneu_store_slot_info(1, &info) && info.mask == 0x0006 && info.tuned_k == 2 && info.count == 5);
    assert(!strcmp(info.name, "old") && !memcmp(info.recall, recall, 4) && info.state == PNEU_SLOT_COMMITTED);
    assert(pneu_store_get(1, 4, &label, back) && label == 1);
}

static void test_header_cache(void)
{
    pneu_store_slot_t info;
    float pool[PNEU_POOL_COUNT], back[PNEU_POOL_COUNT];
    uint8_t label;
    memset(fram, 0xff, sizeof(fram));
    assert(pneu_store_init());
    fill_slot(0, 0, "a", 4);
    assert(pneu_store_get(0, 3, &label, back) && !pneu_store_get(0, 4, &label, back));
    pool_for(9, pool); assert(pneu_store_put(0, 2, pool));
    assert(pneu_store_get(0, 4, &label, back) && label == 2); /* put refreshed the count */
    fill_slot(1, 0, "b", 4); /* cache swap: another slot's header written */
    assert(pneu_store_get(0, 4, &label, back) && pneu_store_get(1, 3, &label, back));
    assert(pneu_store_get(0, 0, &label, back) && label == 1);
    assert(pneu_store_erase(0) && !pneu_store_get(0, 0, &label, back));
    assert(pneu_store_get(1, 0, &label, back));
    /* Header corrupted behind the store's back: slot_info re-reads and drops the cache. */
    fram[SLOT_ADDR(1)+11] ^= 0x01U;
    assert(pneu_store_get(1, 0, &label, back)); /* cached header still in use */
    assert(!pneu_store_slot_info(1, &info));
    assert(!pneu_store_get(1, 0, &label, back));
    fram[SLOT_ADDR(1)+11] ^= 0x01U;
    assert(pneu_store_get(1, 0, &label, back));
    assert(pneu_store_init()); /* invalidates too */
    fram[SLOT_ADDR(1)+11] ^= 0x01U;
    assert(!pneu_store_get(1, 0, &label, back));
    fram[SLOT_ADDR(1)+11] ^= 0x01U;
}

/* SW4 recording: reopen a committed slot to append, undo the last record. */
static void test_reopen_undo(void)
{
    static const uint8_t recall[4] = {100, 50, 255, 0};
    pneu_store_slot_t info;
    float pool[PNEU_POOL_COUNT], back[PNEU_POOL_COUNT];
    uint8_t label;
    memset(fram, 0xff, sizeof(fram));
    assert(pneu_store_init());
    assert(!pneu_store_reopen(0) && !pneu_store_undo(0)); /* EMPTY */
    fill_slot(0, 2, "keep", 5);
    assert(!pneu_store_reopen(0)); /* already OPEN */
    assert(pneu_store_undo(0) && pneu_store_slot_info(0, &info) && info.count == 4);
    assert(pneu_store_commit(0) && pneu_store_set_tuning(0, 0x2031, recall));
    assert(!pneu_store_undo(0)); /* COMMITTED */
    assert(pneu_store_reopen(0) && pneu_store_slot_info(0, &info) && info.state == PNEU_SLOT_OPEN);
    assert(info.count == 4 && info.k == 2 && info.mask == 0x2031 && info.tuned_k == 4);
    assert(!memcmp(info.recall, recall, 4) && !strcmp(info.name, "keep"));
    pool_for(20, pool); assert(pneu_store_put(0, 3, pool));
    assert(pneu_store_get(0, 4, &label, back) && label == 3 && !memcmp(pool, back, sizeof(pool)));
    assert(pneu_store_undo(0) && !pneu_store_get(0, 4, &label, back) && fram[RECORD_ADDR(0, 4)] == 3);
    for (unsigned i = 0; i < 4; ++i) assert(pneu_store_undo(0));
    assert(pneu_store_slot_info(0, &info) && info.count == 0 && !pneu_store_undo(0)); /* count 0 */
    assert(!pneu_store_reopen(4) && !pneu_store_undo(4));
    fill_slot(1, 0, "x", 4); assert(pneu_store_commit(1));
    assert(pneu_store_put(0, 1, pool));
    fram_drop_writes = true;
    assert(!pneu_store_reopen(1) && !pneu_store_undo(0));
    fram_drop_writes = false;
    assert(pneu_store_slot_info(1, &info) && info.state == PNEU_SLOT_COMMITTED);
    assert(pneu_store_slot_info(0, &info) && info.count == 1);
}

/* SW4 long while recording discards the session: truncate back to the start count. */
static void test_truncate(void)
{
    pneu_store_slot_t info;
    float back[PNEU_POOL_COUNT];
    uint8_t label;
    memset(fram, 0xff, sizeof(fram));
    assert(pneu_store_init());
    assert(!pneu_store_truncate(0, 0)); /* EMPTY */
    fill_slot(0, 1, "cut", 7);
    assert(!pneu_store_truncate(0, 8) && !pneu_store_truncate(4, 0));
    assert(pneu_store_truncate(0, 7) && pneu_store_truncate(0, 5));
    assert(pneu_store_slot_info(0, &info) && info.count == 5 && info.state == PNEU_SLOT_OPEN && info.k == 1);
    assert(!strcmp(info.name, "cut") && !pneu_store_get(0, 5, &label, back) && pneu_store_get(0, 4, &label, back));
    assert(pneu_store_commit(0) && !pneu_store_truncate(0, 4)); /* COMMITTED */
    assert(pneu_store_reopen(0));
    fram_drop_writes = true; assert(!pneu_store_truncate(0, 4)); fram_drop_writes = false;
    assert(pneu_store_truncate(0, 0) && pneu_store_slot_info(0, &info) && info.count == 0);
}

int main(void)
{
    test_directory();
    test_round_trip();
    test_full_and_independent();
    test_tuning();
    test_version1_header();
    test_header_cache();
    test_reopen_undo();
    test_truncate();
    puts("Model store: directory, slot layout, CRC, state rules, full slot, erase, use, 4 independent slots, tuning mask (header v2), v1 headers, header cache, reopen/undo/truncate PASS");
    return 0;
}
