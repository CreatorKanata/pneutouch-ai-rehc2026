/* FeRAM has no erase cycle: every update is a plain overwrite followed by a
   read-back compare, which catches a missing chip or wiring fault early. */
#include "model_store.h"
#include "board.h"
#include "crc16.h"
#include <string.h>

#define DIR_ADDR 0x1000U
#define DIR_SIZE 16U
#define DIR_MAGIC 0x54534E50UL  /* 'PNST' */
#define SLOT_ADDR 0x2000U
#define SLOT_SPAN 0xF000U
#define SLOT_MAGIC 0x4C534E50UL /* 'PNSL' */
#define HEADER_SIZE 32U
#define HEADER_CRC_AT 24U
#define RECORD_SIZE 60U
#define RECORD_CRC_AT 58U
#define STORE_VERSION 1U
#define SLOT_CAPACITY ((SLOT_SPAN-HEADER_SIZE)/RECORD_SIZE)
#define MIN_COMMIT 4U
#define MAX_K 12U

static uint8_t active;

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static void put32(uint8_t *p, uint32_t v) { put16(p, (uint16_t)v); put16(p+2, (uint16_t)(v >> 16)); }
static uint32_t get32(const uint8_t *p) { return get16(p) | ((uint32_t)get16(p+2) << 16); }

static uint32_t header_addr(uint8_t slot) { return SLOT_ADDR+(uint32_t)slot*SLOT_SPAN; }
static uint32_t record_addr(uint8_t slot, uint16_t index)
{
    return header_addr(slot)+HEADER_SIZE+(uint32_t)index*RECORD_SIZE;
}

static bool write_verified(uint32_t addr, const uint8_t *src, unsigned n)
{
    uint8_t back[RECORD_SIZE];
    board_store_write(addr, src, n);
    board_store_read(addr, back, n);
    return !memcmp(back, src, n);
}

static bool write_directory(uint8_t slot)
{
    uint8_t d[DIR_SIZE] = {0};
    put32(d, DIR_MAGIC); put16(d+4, STORE_VERSION); d[6] = slot;
    put16(d+8, pneu_crc16(d, 8U));
    if (!write_verified(DIR_ADDR, d, DIR_SIZE)) return false;
    active = slot;
    return true;
}

bool pneu_store_init(void)
{
    uint8_t d[DIR_SIZE];
    board_store_read(DIR_ADDR, d, DIR_SIZE);
    if (get32(d) == DIR_MAGIC && get16(d+4) == STORE_VERSION && d[6] < PNEU_STORE_SLOTS &&
        get16(d+8) == pneu_crc16(d, 8U)) {
        active = d[6];
        return true;
    }
    active = 0;
    return write_directory(0);
}

uint8_t pneu_store_active(void) { return active; }

bool pneu_store_use(uint8_t slot)
{
    return slot < PNEU_STORE_SLOTS && write_directory(slot);
}

static bool read_header(uint8_t slot, pneu_store_slot_t *info)
{
    uint8_t h[HEADER_SIZE];
    unsigned i, end = 0;
    memset(info, 0, sizeof(*info));
    if (slot >= PNEU_STORE_SLOTS) return false;
    board_store_read(header_addr(slot), h, HEADER_SIZE);
    if (get32(h) != SLOT_MAGIC || get16(h+4) != STORE_VERSION || h[6] != PNEU_POOL_COUNT ||
        h[7] > MAX_K || h[8] > PNEU_SLOT_COMMITTED || get16(h+10) > SLOT_CAPACITY ||
        get16(h+HEADER_CRC_AT) != pneu_crc16(h, HEADER_CRC_AT)) return false;
    info->pool_count = h[6]; info->k = h[7]; info->state = h[8];
    info->count = get16(h+10);
    for (i = 0; i < PNEU_STORE_NAME; ++i) {
        info->name[i] = (char)h[12+i];
        if (h[12+i] != ' ') end = i+1U;
    }
    info->name[end] = '\0'; /* space padding is not part of the name */
    info->valid = true;
    return true;
}

bool pneu_store_slot_info(uint8_t slot, pneu_store_slot_t *info)
{
    return read_header(slot, info);
}

static bool write_header(uint8_t slot, const pneu_store_slot_t *info)
{
    uint8_t h[HEADER_SIZE] = {0};
    unsigned i;
    put32(h, SLOT_MAGIC); put16(h+4, STORE_VERSION);
    h[6] = PNEU_POOL_COUNT; h[7] = info->k; h[8] = info->state; /* h[9] reserved */
    put16(h+10, info->count);
    for (i = 0; i < PNEU_STORE_NAME; ++i) h[12+i] = ' ';
    for (i = 0; i < PNEU_STORE_NAME && info->name[i]; ++i) h[12+i] = (uint8_t)info->name[i];
    put16(h+HEADER_CRC_AT, pneu_crc16(h, HEADER_CRC_AT));
    return write_verified(header_addr(slot), h, HEADER_SIZE);
}

static bool name_ok(const char *name)
{
    unsigned n = 0;
    for (; name[n]; ++n)
        if (n >= PNEU_STORE_NAME || name[n] < ' ' || name[n] > '~' || name[n] == ',') return false;
    return true;
}

bool pneu_store_open(uint8_t slot, uint8_t k, const char *name)
{
    pneu_store_slot_t info = {0};
    if (slot >= PNEU_STORE_SLOTS || k > MAX_K || !name_ok(name)) return false;
    info.k = k; info.state = PNEU_SLOT_OPEN;
    strcpy(info.name, name);
    return write_header(slot, &info);
}

static bool pool_ok(const float pool[PNEU_POOL_COUNT])
{
    unsigned j;
    for (j = 0; j < PNEU_POOL_COUNT; ++j) {
        uint32_t bits;
        memcpy(&bits, &pool[j], sizeof(bits));
        if ((bits & 0x7f800000U) == 0x7f800000U || !(pool[j] > 0.0f)) return false;
    }
    return true;
}

bool pneu_store_put(uint8_t slot, uint8_t label, const float pool[PNEU_POOL_COUNT])
{
    pneu_store_slot_t info;
    uint8_t r[RECORD_SIZE] = {0};
    unsigned j;
    if (!read_header(slot, &info) || info.state != PNEU_SLOT_OPEN || label < 1U || label > 4U ||
        info.count >= PNEU_STORE_MAX_RECORDS || !pool_ok(pool)) return false;
    r[0] = label;
    for (j = 0; j < PNEU_POOL_COUNT; ++j) {
        uint32_t bits;
        memcpy(&bits, &pool[j], sizeof(bits));
        put32(r+2U+4U*j, bits);
    }
    put16(r+RECORD_CRC_AT, pneu_crc16(r, RECORD_CRC_AT));
    if (!write_verified(record_addr(slot, info.count), r, RECORD_SIZE)) return false;
    ++info.count; /* only after the record itself is verified */
    return write_header(slot, &info);
}

bool pneu_store_commit(uint8_t slot)
{
    pneu_store_slot_t info;
    if (!read_header(slot, &info) || info.state != PNEU_SLOT_OPEN || info.count < MIN_COMMIT) return false;
    info.state = PNEU_SLOT_COMMITTED;
    return write_header(slot, &info);
}

bool pneu_store_erase(uint8_t slot)
{
    const pneu_store_slot_t empty = {0};
    return slot < PNEU_STORE_SLOTS && write_header(slot, &empty);
}

bool pneu_store_get(uint8_t slot, uint16_t index, uint8_t *label, float pool[PNEU_POOL_COUNT])
{
    pneu_store_slot_t info;
    uint8_t r[RECORD_SIZE];
    unsigned j;
    if (!read_header(slot, &info) || info.state == PNEU_SLOT_EMPTY || index >= info.count) return false;
    board_store_read(record_addr(slot, index), r, RECORD_SIZE);
    if (get16(r+RECORD_CRC_AT) != pneu_crc16(r, RECORD_CRC_AT) || r[0] < 1U || r[0] > 4U) return false;
    for (j = 0; j < PNEU_POOL_COUNT; ++j) {
        uint32_t bits = get32(r+2U+4U*j);
        memcpy(&pool[j], &bits, sizeof(bits));
    }
    *label = r[0];
    return true;
}
