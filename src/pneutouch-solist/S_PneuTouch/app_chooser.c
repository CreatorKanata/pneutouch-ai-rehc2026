/* Object chooser of SW4 long; see app_chooser.h. */
#include "app_chooser.h"
#include "model_store.h"
#include "object_profile.h"
#include "board.h"
#include <string.h>

volatile app_chooser_t app_chooser;

static void touch(void) { app_chooser.since_ms = board_millis(); }

app_record_action_t app_chooser_open(char notice[17])
{
    pneu_store_slot_t info;
    uint8_t slot;
    if (!app_record_need_empty(&slot, notice)) return APP_REC_NOTICE;
    (void)pneu_store_slot_info(pneu_store_active(), &info); /* unreadable: empty name, DINO */
    app_chooser.object = (uint8_t)pneu_object_of_name(info.name);
    app_chooser.active = true;
    touch();
    return APP_REC_IDLE;
}

void app_chooser_close(void) { app_chooser.active = false; }

void app_chooser_line(char line[17])
{
    memcpy(line, "NEW:       (SW5)", 17);
    memcpy(line+5, pneu_object((pneu_object_t)app_chooser.object)->tag, PNEU_OBJECT_TAG);
}

/* SW4 short: the chooser closes either way. */
static app_record_action_t confirm(char notice[17])
{
    uint8_t slot;
    app_chooser_close();
    if (!app_record_need_empty(&slot, notice)) return APP_REC_NOTICE;
    return app_record_start(slot, true, app_chooser.object, notice) ? APP_REC_IDLE : APP_REC_NOTICE;
}

app_record_action_t app_chooser_button(app_button_event_t event, char notice[17])
{
    if (!app_chooser.active) return APP_REC_NONE;
    switch (event) {
    case APP_BUTTON_LABEL:
        app_chooser.object = (uint8_t)((app_chooser.object+1U)%PNEU_OBJECT_COUNT);
        touch();
        return APP_REC_IDLE;
    case APP_BUTTON_REC_TOGGLE: return confirm(notice);
    case APP_BUTTON_REC_NEW: app_chooser_close(); return APP_REC_IDLE;
    case APP_BUTTON_NONE:
        if (board_millis()-app_chooser.since_ms < APP_CHOOSER_MS) return APP_REC_NONE;
        app_chooser_close();
        return APP_REC_IDLE;
    default:
        return APP_REC_NONE;
    }
}
