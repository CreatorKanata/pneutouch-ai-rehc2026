/* Every reply starts "# PST1,"; the decimal id is echoed like PAI1 tokens.
   Writes need MODE 1 so FeRAM traffic never delays live sampling. */
#include "store_protocol.h"
#include "model_store.h"
#include "pressure_model.h"
#include "uart_text.h"
#include "board.h"
#include <string.h>

#define POOL_HEX (PNEU_POOL_COUNT*8U)
#define MAX_K 12U

typedef const char *(*handler_t)(uint32_t id, char *const f[], unsigned n);

static void head(const char *op, uint32_t id) { pneu_text("# PST1,"); pneu_text(op); board_putc(','); pneu_number(id); }
static void field(uint32_t v) { board_putc(','); pneu_number(v); }
static void end(void) { pneu_text("\r\n"); }

static bool slot_arg(const char *s, uint8_t *slot)
{
    uint32_t v;
    if (!pneu_integer(s, &v) || v >= PNEU_STORE_SLOTS) return false;
    *slot = (uint8_t)v;
    return true;
}

static void list(const volatile uint8_t *items, unsigned count)
{
    unsigned i;
    board_putc(',');
    for (i = 0; i < count; ++i) { if (i) board_putc(':'); pneu_number(items[i]); }
}

static const char *info_cmd(uint32_t id, char *const f[], unsigned n)
{
    pneu_store_slot_t info;
    uint8_t s;
    (void)f;
    if (n != 3) return "FIELDS";
    head("INFO", id); field(pneu_store_active());
    for (s = 0; s < PNEU_STORE_SLOTS; ++s) field(pneu_store_slot_info(s, &info) ? info.count : 0U);
    end();
    return 0;
}

static const char *slot_cmd(uint32_t id, char *const f[], unsigned n)
{
    pneu_store_slot_t info;
    uint8_t s;
    if (n != 4) return "FIELDS";
    if (!slot_arg(f[3], &s)) return "SLOT";
    (void)pneu_store_slot_info(s, &info);
    head("SLOT", id); field(s); field(info.state); field(info.count); field(info.pool_count);
    field(info.k); board_putc(','); pneu_text(info.name); end();
    return 0;
}

static const char *open_cmd(uint32_t id, char *const f[], unsigned n)
{
    uint32_t k;
    uint8_t s;
    if (n != 6) return "FIELDS";
    if (!slot_arg(f[3], &s)) return "SLOT";
    if (!pneu_integer(f[4], &k) || k > MAX_K || strlen(f[5]) > PNEU_STORE_NAME) return "RANGE";
    if (!pneu_store_open(s, (uint8_t)k, f[5])) return "STORE";
    head("OPEN", id); field(s); end();
    return 0;
}

static const char *put_cmd(uint32_t id, char *const f[], unsigned n)
{
    float pool[PNEU_POOL_COUNT];
    pneu_store_slot_t info;
    uint32_t label;
    unsigned j;
    uint8_t s;
    int parsed;
    if (n != 6) return "FIELDS";
    if (!slot_arg(f[3], &s)) return "SLOT";
    if (!pneu_integer(f[4], &label) || label < 1U || label > 4U) return "LABEL";
    if (strlen(f[5]) != POOL_HEX) return "INPUT_SIZE";
    parsed = pneu_parse_f32_hex(f[5], pool, PNEU_POOL_COUNT);
    if (parsed) return parsed == 1 ? "INPUT_HEX" : "NONFINITE";
    for (j = 0; j < PNEU_POOL_COUNT; ++j) if (!(pool[j] > 0.0f)) return "RANGE";
    if (!pneu_store_slot_info(s, &info) || info.state != PNEU_SLOT_OPEN) return "STATE";
    if (info.count >= PNEU_STORE_MAX_RECORDS) return "FULL";
    if (!pneu_store_put(s, (uint8_t)label, pool)) return "STORE";
    head("PUT", id); field(s); field(info.count+1U); end();
    return 0;
}

static const char *commit_cmd(uint32_t id, char *const f[], unsigned n)
{
    pneu_store_slot_t info;
    uint8_t s;
    if (n != 4) return "FIELDS";
    if (!slot_arg(f[3], &s)) return "SLOT";
    if (!pneu_store_slot_info(s, &info) || info.state != PNEU_SLOT_OPEN) return "STATE";
    if (info.count < 4U) return "RANGE";
    if (!pneu_store_commit(s)) return "STORE";
    head("COMMIT", id); field(s); field(info.count); end();
    return 0;
}

static const char *get_cmd(uint32_t id, char *const f[], unsigned n)
{
    float pool[PNEU_POOL_COUNT];
    pneu_store_slot_t info;
    uint32_t index, bits;
    unsigned j;
    uint8_t s, label;
    if (n != 5) return "FIELDS";
    if (!slot_arg(f[3], &s)) return "SLOT";
    if (!pneu_integer(f[4], &index)) return "INDEX";
    if (!pneu_store_slot_info(s, &info) || info.state == PNEU_SLOT_EMPTY) return "STATE";
    if (index >= info.count) return "INDEX";
    if (!pneu_store_get(s, (uint16_t)index, &label, pool)) return "CRC_RECORD";
    head("GET", id); field(s); field(index); field(label); board_putc(',');
    for (j = 0; j < PNEU_POOL_COUNT; ++j) { memcpy(&bits, &pool[j], sizeof(bits)); pneu_hex32(bits); }
    end();
    return 0;
}

static const char *slot_only(const char *op, bool (*action)(uint8_t), uint32_t id,
                             char *const f[], unsigned n)
{
    uint8_t s;
    if (n != 4) return "FIELDS";
    if (!slot_arg(f[3], &s)) return "SLOT";
    if (!action(s)) return "STORE";
    head(op, id); field(s); end();
    return 0;
}
static const char *erase_cmd(uint32_t id, char *const f[], unsigned n) { return slot_only("ERASE", pneu_store_erase, id, f, n); }
static const char *use_cmd(uint32_t id, char *const f[], unsigned n) { return slot_only("USE", pneu_store_use, id, f, n); }

static const char *train_cmd(uint32_t id, char *const f[], unsigned n)
{
    bool ready;
    (void)f;
    if (n != 3) return "FIELDS";
    ready = pneu_model_init();
    head("TRAIN", id); field(ready ? 1U : 0U); field(pneutouch_model.error);
    field(pneutouch_model.store_count); field(pneutouch_model.selected_count);
    list(pneutouch_model.selected, pneutouch_model.selected_count);
    field(pneutouch_model.training_us); end();
    return 0;
}

static const char *selftest_cmd(uint32_t id, char *const f[], unsigned n)
{
    int result;
    (void)f;
    if (n != 3) return "FIELDS";
    result = board_store_selftest();
    head("SELFTEST", id); board_putc(',');
    if (result < 0) { board_putc('-'); pneu_number((uint32_t)-(result+1)+1U); }
    else pneu_number((uint32_t)result);
    end();
    return 0;
}

static const struct { const char *name; bool needs_mode; handler_t run; } commands[] = {
    {"INFO", false, info_cmd}, {"SLOT", false, slot_cmd}, {"SELFTEST", false, selftest_cmd},
    {"OPEN", true, open_cmd}, {"PUT", true, put_cmd}, {"COMMIT", true, commit_cmd},
    {"GET", true, get_cmd}, {"ERASE", true, erase_cmd}, {"USE", true, use_cmd},
    {"TRAIN", true, train_cmd},
};

bool pneu_store_command(char *const fields[], unsigned n, bool validation)
{
    const char *why = "COMMAND";
    uint32_t id;
    unsigned i;
    bool retrained = false;
    if (n < 3 || !pneu_integer(fields[2], &id)) why = "FIELDS";
    else for (i = 0; i < sizeof(commands)/sizeof(commands[0]); ++i) {
        if (strcmp(fields[1], commands[i].name)) continue;
        if (commands[i].needs_mode && !validation) { why = "MODE_REQUIRED"; break; }
        why = commands[i].run(id, fields, n);
        retrained = !why && commands[i].run == train_cmd;
        break;
    }
    if (why) { pneu_text("# PST1,ERROR,"); pneu_text(why); end(); }
    return retrained;
}
