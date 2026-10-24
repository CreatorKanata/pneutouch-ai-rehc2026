/* SW4 long outside recording (docs/model-store.md, "物体プロファイル"): a one-step chooser of
   the object of the new slot. LCD "NEW: DINO  (SW5)"; SW5 short toggles DINO/KRAG, SW4 short
   opens the first EMPTY slot as <tag>BTN and starts recording, SW4 long or APP_CHOOSER_MS
   without a press cancels. Every other button is ignored while it is up. */
#ifndef PNEU_APP_CHOOSER_H
#define PNEU_APP_CHOOSER_H
#include <stdbool.h>
#include <stdint.h>
#include "app_record.h"

#define APP_CHOOSER_MS 10000U

typedef struct {
    bool active;
    uint8_t object;    /* pneu_object_t shown */
    uint32_t since_ms; /* last press (board_millis) */
} app_chooser_t;
extern volatile app_chooser_t app_chooser; /* for the debugger and tests */

/* Opens with the object of the active slot (APP_REC_IDLE), or "NO EMPTY SLOT" (APP_REC_NOTICE). */
app_record_action_t app_chooser_open(char notice[17]);
void app_chooser_close(void);
/* "NEW: DINO  (SW5)". */
void app_chooser_line(char line[17]);
/* Button events while active (APP_BUTTON_NONE checks the timeout): APP_REC_IDLE = show the
   idle text again (the chooser line or the REC line), APP_REC_NOTICE = notice set, else NONE. */
app_record_action_t app_chooser_button(app_button_event_t event, char notice[17]);
#endif
