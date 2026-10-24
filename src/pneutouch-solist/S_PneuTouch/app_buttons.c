/* SW2..SW5 debounce, gestures and their store/tune actions; pressure_app.c owns the
   LCD notice timing and the retraining. */
#include "app_buttons.h"
#include "model_store.h"
#include "model_tune.h"
#include "store_protocol.h"
#include "uart_text.h"
#include <string.h>

#define BUTTONS 4U
#define ALL_BUTTONS (APP_SW2 | APP_SW3 | APP_SW4 | APP_SW5)

/* Events of one button: on the press edge, on a short release (<= APP_SHORT_MS),
   on any other release while armed, and once when held APP_TUNE_HOLD_MS. */
typedef struct { uint8_t bit; app_button_event_t down, brief, released, held; } gesture_t;
static const gesture_t gestures[BUTTONS] = {
    {APP_SW2, APP_BUTTON_HOLD, APP_BUTTON_NONE, APP_BUTTON_CANCEL, APP_BUTTON_TUNE},
    {APP_SW3, APP_BUTTON_NONE, APP_BUTTON_NEXT, APP_BUTTON_NONE, APP_BUTTON_NONE},
    {APP_SW4, APP_BUTTON_NONE, APP_BUTTON_REC_TOGGLE, APP_BUTTON_NONE, APP_BUTTON_REC_NEW},
    {APP_SW5, APP_BUTTON_NONE, APP_BUTTON_LABEL, APP_BUTTON_NONE, APP_BUTTON_UNDO},
};

static uint8_t raw_last, accepted, armed; /* armed: a press edge seen and not yet consumed */
static uint32_t raw_since, down_at[BUTTONS];

void app_buttons_reset(uint8_t pressed, uint32_t now_ms)
{
    raw_last = accepted = (uint8_t)(pressed & ALL_BUTTONS);
    raw_since = now_ms;
    armed = 0;
}

/* Release of an armed button at since: short press or plain release. */
static app_button_event_t release(const gesture_t *g, uint32_t pressed_ms)
{
    armed &= (uint8_t)~g->bit;
    if (g->brief != APP_BUTTON_NONE && pressed_ms <= APP_SHORT_MS) return g->brief;
    return g->released;
}

/* Edges of the accepted mask; since = when the raw mask last changed. */
static app_button_event_t edges(uint8_t down, uint8_t up, uint32_t since)
{
    app_button_event_t event = APP_BUTTON_NONE, e;
    unsigned i;
    for (i = 0; i < BUTTONS; ++i) {
        const gesture_t *g = &gestures[i];
        e = APP_BUTTON_NONE;
        if (down & g->bit) { armed |= g->bit; down_at[i] = since; e = g->down; }
        else if ((up & g->bit) && (armed & g->bit)) e = release(g, since-down_at[i]);
        if (e != APP_BUTTON_NONE) event = e;
    }
    return event;
}

/* First armed button held long enough that has a long-press event (fires once). */
static app_button_event_t long_press(uint32_t now_ms)
{
    unsigned i;
    for (i = 0; i < BUTTONS; ++i) {
        const gesture_t *g = &gestures[i];
        if (g->held != APP_BUTTON_NONE && (armed & accepted & g->bit) && now_ms-down_at[i] >= APP_TUNE_HOLD_MS) {
            armed &= (uint8_t)~g->bit;
            return g->held;
        }
    }
    return APP_BUTTON_NONE;
}

app_button_event_t app_buttons_poll(uint8_t pressed, uint32_t now_ms)
{
    pressed &= ALL_BUTTONS;
    if (pressed != raw_last) { raw_last = pressed; raw_since = now_ms; }
    if (raw_last != accepted && now_ms-raw_since >= APP_DEBOUNCE_MS) {
        uint8_t down = (uint8_t)(raw_last & ~accepted), up = (uint8_t)(accepted & ~raw_last);
        accepted = raw_last;
        return edges(down, up, raw_since);
    }
    return long_press(now_ms);
}

void app_digits(char *out, unsigned width, uint32_t value)
{
    unsigned i = width;
    do { out[--i] = (char)('0'+value%10U); value /= 10U; } while (value && i);
    while (i) out[--i] = ' ';
}

void app_slot_notice(char line[17], uint8_t slot, uint16_t count, uint8_t selected)
{
    memcpy(line, "SLOT            ", 17);
    app_digits(line+5, 1, slot);
    app_digits(line+7, 4, count);
    if (selected >= 10U) app_digits(line+13, 2, selected);
    else { line[13] = 'k'; app_digits(line+14, 1, selected); }
}

void app_run_tune(void)
{
    bool ok;
    pneu_text("# PNET1,START\r\n");
    ok = pneu_model_tune();
    pneu_text("# PNET1"); pneu_store_tune_fields(ok); pneu_text("\r\n");
}

/* First slot after active (wrapping) that is COMMITTED with >= 4 records, or active. */
static uint8_t find_next(uint8_t active, uint16_t *count)
{
    pneu_store_slot_t info;
    unsigned step;
    for (step = 1; step < PNEU_STORE_SLOTS; ++step) {
        uint8_t slot = (uint8_t)((active+step)%PNEU_STORE_SLOTS);
        if (pneu_store_slot_info(slot, &info) && info.state == PNEU_SLOT_COMMITTED && info.count >= 4U) {
            *count = info.count;
            return slot;
        }
    }
    return active;
}

bool app_next_slot(char line[17])
{
    uint8_t active = pneu_store_active(), next;
    uint16_t count = 0;
    next = find_next(active, &count);
    if (next == active) {
        memcpy(line, "SLOT   ONLY     ", 17); app_digits(line+5, 1, active);
        pneu_text("# PNES1,ERROR,NO_OTHER_SLOT\r\n");
        return false;
    }
    if (!pneu_store_use(next)) {
        memcpy(line, "STORE ERROR", 12);
        pneu_text("# PNES1,ERROR,STORE\r\n");
        return false;
    }
    pneu_text("# PNES1,slot="); pneu_number(next); pneu_text(",count="); pneu_number(count); pneu_text("\r\n");
    memcpy(line, "SLOT            ", 17); app_digits(line+5, 1, next); app_digits(line+8, 4, count);
    return true;
}
