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
    assert(le16(SLOT_ADDR(0)+24) == pneu_crc16(fram+SLOT_ADDR(0), 24));
    assert(pneu_store_slot_info(0, &info) && info.state == PNEU_SLOT_OPEN && info.count == 0);
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

int main(void)
{
    test_directory();
    test_round_trip();
    test_full_and_independent();
    puts("Model store: directory, slot layout, CRC, state rules, full slot, erase, use, 4 independent slots PASS");
    return 0;
}
