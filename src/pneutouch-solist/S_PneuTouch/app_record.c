/* SW4/SW5 recording state and its FeRAM actions. A put happens once per completed
   press event (one 60-byte record and a 32-byte header, each written and read back),
   never inside the sampling loop: the one FeRAM write during live acquisition. */
#include "app_record.h"
#include "model_store.h"
#include "pressure_model.h"
#include "uart_text.h"
#include "board.h"
#include <string.h>

#define MIN_RECORDS 4U
#define FIRST_LABEL 1U
#define LABELS 4U
#define FRESH_NAME "BTN"

volatile app_record_t app_record = {false, false, false, 0, FIRST_LABEL, 0, 0, 0, {0, 0, 0, 0}};

/* text: exactly 16 characters. */
static void set_notice(char notice[17], const char *text) { memcpy(notice, text, 17); }

static void error_line(const char *reason)
{
    pneu_text("# PNER1,ERROR,"); pneu_text(reason); pneu_text("\r\n");
}

static void store_error(char notice[17])
{
    set_notice(notice, "STORE ERROR     ");
    error_line("STORE");
}

static const char *label_name(uint8_t label) { return pneu_class_name((pneu_demo_class_t)label); }

/* "<head>,<slot>,<a>,<b>" after "# PNER1,"; b < 0 = absent. */
static void record_line(const char *head, uint32_t a, int32_t b)
{
    pneu_text("# PNER1,"); pneu_text(head); board_putc(','); pneu_number(app_record.slot);
    board_putc(','); pneu_number(a);
    if (b >= 0) { board_putc(','); pneu_number((uint32_t)b); }
    pneu_text("\r\n");
}

/* "<prefix><ZONE> <n>", left-aligned and space padded to 16 columns. */
static void zone_notice(char notice[17], const char *prefix, uint8_t label, uint16_t n)
{
    char digits[4];
    unsigned at = (unsigned)strlen(prefix), i = 0;
    memset(notice, ' ', 16); notice[16] = '\0';
    memcpy(notice, prefix, at);
    memcpy(notice+at, label_name(label), 4); at += 5U;
    app_digits(digits, 4, n);
    while (i < 3U && digits[i] == ' ') ++i;
    memcpy(notice+at, digits+i, 4U-i);
}

void app_record_boot(void)
{
    pneu_store_slot_t info;
    uint8_t slot;
    app_record.active = false; app_record.label = FIRST_LABEL; app_record.added = 0;
    if (!pneu_store_init()) return;
    slot = pneu_store_active();
    if (!pneu_store_slot_info(slot, &info) || info.state != PNEU_SLOT_OPEN || info.count < MIN_RECORDS) return;
    if (!pneu_store_commit(slot)) { error_line("STORE"); return; }
    pneu_text("# PNER1,AUTOCOMMIT,"); pneu_number(slot); board_putc(','); pneu_number(info.count);
    pneu_text("\r\n");
}

/* One label pass over the records of slot into class_count. */
static bool count_classes(uint8_t slot, uint16_t count)
{
    float pool[PNEU_POOL_COUNT];
    uint16_t counts[LABELS] = {0}, i;
    uint8_t label;
    unsigned c;
    for (i = 0; i < count; ++i) {
        board_watchdog();
        if (!pneu_store_get(slot, i, &label, pool)) return false;
        ++counts[label-1U];
    }
    for (c = 0; c < LABELS; ++c) app_record.class_count[c] = counts[c];
    return true;
}

/* Leaves slot OPEN for appending; *count = its records. An unreadable header reads
   as EMPTY (pneu_store_slot_info zeroes it) and is opened fresh. */
static bool open_for_append(uint8_t slot, bool *fresh, uint16_t *count)
{
    pneu_store_slot_t info;
    unsigned c;
    (void)pneu_store_slot_info(slot, &info);
    if (info.state == PNEU_SLOT_EMPTY) *fresh = true;
    app_record.reopened = !*fresh && info.state == PNEU_SLOT_COMMITTED;
    app_record.prev_active = pneu_store_active();
    if (*fresh) {
        *count = 0;
        for (c = 0; c < LABELS; ++c) app_record.class_count[c] = 0;
        return pneu_store_open(slot, 0, FRESH_NAME) && pneu_store_use(slot);
    }
    *count = info.count;
    if (!count_classes(slot, info.count)) return false;
    return info.state == PNEU_SLOT_OPEN || pneu_store_reopen(slot);
}

bool app_record_start(uint8_t slot, bool fresh, char notice[17])
{
    uint16_t count = 0;
    if (slot >= PNEU_STORE_SLOTS || !open_for_append(slot, &fresh, &count)) {
        store_error(notice);
        return false;
    }
    app_record.active = true; app_record.slot = slot; app_record.fresh = fresh;
    app_record.added = 0; app_record.count = count;
    if (app_record.label < 1U || app_record.label > LABELS) app_record.label = FIRST_LABEL;
    record_line("START", count, fresh ? 1 : 0);
    return true;
}

bool app_record_empty_slot(uint8_t *slot)
{
    pneu_store_slot_t info;
    uint8_t s;
    for (s = 0; s < PNEU_STORE_SLOTS; ++s) {
        (void)pneu_store_slot_info(s, &info); /* an unreadable header reads as EMPTY */
        if (info.state == PNEU_SLOT_EMPTY) { *slot = s; return true; }
    }
    return false;
}

bool app_record_stop(char notice[17])
{
    if (app_record.count < MIN_RECORDS) { set_notice(notice, "NEED 4 RECORDS  "); return false; }
    if (!pneu_store_commit(app_record.slot)) { store_error(notice); return false; }
    app_record.active = false;
    record_line("STOP", app_record.count, (int32_t)app_record.added);
    return true;
}

bool app_record_event(const float values[12], float peak, float trough, char notice[17])
{
    float pool[PNEU_POOL_COUNT];
    uint8_t label = app_record.label;
    if (!pneu_model_pool(values, peak, trough, pool)) {
        set_notice(notice, "BAD PRESS       "); error_line("POOL");
        return false;
    }
    if (!pneu_store_put(app_record.slot, label, pool)) { store_error(notice); return false; }
    ++app_record.added; ++app_record.count; ++app_record.class_count[label-1U];
    pneu_text("# PNER1,REC,"); pneu_number(app_record.slot); board_putc(',');
    pneu_text(label_name(label)); board_putc(','); pneu_number(app_record.count); pneu_text("\r\n");
    zone_notice(notice, "+", label, app_record.class_count[label-1U]);
    return true;
}

void app_record_next_label(void)
{
    app_record.label = (uint8_t)(app_record.label%LABELS+1U);
}

bool app_record_undo(char notice[17])
{
    float pool[PNEU_POOL_COUNT];
    uint8_t label;
    if (!app_record.added || !app_record.count) { set_notice(notice, "NOTHING TO UNDO "); return false; }
    if (!pneu_store_get(app_record.slot, (uint16_t)(app_record.count-1U), &label, pool) ||
        !pneu_store_undo(app_record.slot)) {
        store_error(notice);
        return false;
    }
    --app_record.added; --app_record.count; --app_record.class_count[label-1U];
    pneu_text("# PNER1,UNDO,"); pneu_number(app_record.slot); board_putc(',');
    pneu_text(label_name(label)); board_putc(','); pneu_number(app_record.count); pneu_text("\r\n");
    zone_notice(notice, "UNDO ", label, app_record.class_count[label-1U]);
    return true;
}

void app_record_line(char line[17])
{
    uint8_t label = app_record.label;
    memcpy(line, "REC             ", 17);
    memcpy(line+4, label_name(label), 4);
    app_digits(line+9, 3, app_record.class_count[label-1U]);
    line[12] = '/';
    app_digits(line+13, 3, app_record.count);
}

/* Back to the state before the session; false on a FeRAM fault. */
static bool restore(void)
{
    uint8_t slot = app_record.slot;
    if (!pneu_store_truncate(slot, (uint16_t)(app_record.count-app_record.added))) return false;
    if (app_record.fresh) return pneu_store_erase(slot) && pneu_store_use(app_record.prev_active);
    return !app_record.reopened || pneu_store_commit(slot);
}

app_record_action_t app_record_discard(char notice[17])
{
    bool ok = restore();
    app_record.active = false; /* also after a fault: the slot state is no longer known */
    if (!ok) { store_error(notice); return APP_REC_DISCARD; }
    record_line("DISCARD", app_record.added, -1);
    set_notice(notice, "DISCARDED       ");
    return app_record.fresh || app_record.reopened ? APP_REC_DISCARD : APP_REC_NOTICE;
}

void app_record_abort(void)
{
    if (!app_record.active) return;
    app_record.active = false;
    if (app_record.count >= MIN_RECORDS && pneu_store_commit(app_record.slot)) {
        record_line("STOP", app_record.count, (int32_t)app_record.added);
        return;
    }
    record_line("ABORT", app_record.count, -1);
}

/* SW5 outside recording does nothing. */
app_record_action_t app_record_button(app_button_event_t event, char notice[17])
{
    uint8_t slot;
    switch (event) {
    case APP_BUTTON_REC_TOGGLE:
        if (app_record.active) return app_record_stop(notice) ? APP_REC_RETRAIN : APP_REC_NOTICE;
        return app_record_start(pneu_store_active(), false, notice) ? APP_REC_IDLE : APP_REC_NOTICE;
    case APP_BUTTON_REC_NEW:
        if (app_record.active) return app_record_discard(notice);
        if (!app_record_empty_slot(&slot)) {
            set_notice(notice, "NO EMPTY SLOT   "); error_line("NO_EMPTY_SLOT");
            return APP_REC_NOTICE;
        }
        return app_record_start(slot, true, notice) ? APP_REC_IDLE : APP_REC_NOTICE;
    case APP_BUTTON_LABEL:
        if (!app_record.active) return APP_REC_NONE;
        app_record_next_label();
        return APP_REC_IDLE;
    case APP_BUTTON_UNDO:
        if (!app_record.active) return APP_REC_NONE;
        return app_record_undo(notice) ? APP_REC_BRIEF : APP_REC_NOTICE;
    default:
        return APP_REC_NONE;
    }
}
