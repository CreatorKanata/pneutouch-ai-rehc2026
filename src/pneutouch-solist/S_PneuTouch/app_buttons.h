/* Board push buttons without a PC (docs/model-store.md, "ボタン操作"):
   SW2 held 2 s = PST1 TUNE, SW3 short press = next committed slot, SW4 short = start/stop
   recording into the active slot, SW4 held 2 s = record into a new slot (object chooser,
   app_chooser.h), SW5 short = next
   label, SW5 held 2 s = undo the last record (app_record.c). */
#ifndef PNEU_APP_BUTTONS_H
#define PNEU_APP_BUTTONS_H
#include <stdbool.h>
#include <stdint.h>

#define APP_DEBOUNCE_MS 30U    /* raw mask stable this long before it is accepted */
#define APP_TUNE_HOLD_MS 2000U /* long press of SW2/SW4/SW5 (fires once while held) */
#define APP_SHORT_MS 1000U     /* released within this = short press */
#define APP_NOTICE_MS 2000U    /* LCD notices ("SLOT ...") before the idle text returns */
#define APP_BRIEF_MS 1000U     /* recording notices ("+HEAD 39", "UNDO HEAD 38") */
#define APP_SW2 0x01U
#define APP_SW3 0x02U
#define APP_SW4 0x04U
#define APP_SW5 0x08U

typedef enum {
    APP_BUTTON_NONE,
    APP_BUTTON_HOLD,   /* SW2 went down: show the hold hint */
    APP_BUTTON_CANCEL, /* SW2 released before the hold time */
    APP_BUTTON_TUNE,   /* SW2 held APP_TUNE_HOLD_MS (once per press) */
    APP_BUTTON_NEXT,   /* SW3 short press */
    APP_BUTTON_REC_TOGGLE, /* SW4 short press */
    APP_BUTTON_REC_NEW,    /* SW4 held APP_TUNE_HOLD_MS */
    APP_BUTTON_LABEL,      /* SW5 short press */
    APP_BUTTON_UNDO        /* SW5 held APP_TUNE_HOLD_MS */
} app_button_event_t;

/* Accepts pressed (board_buttons()) as the current state without edges: buttons
   held across a reset or validation do nothing until released and pressed again. */
void app_buttons_reset(uint8_t pressed, uint32_t now_ms);
/* Debounce and gesture detection; at most one event per call (edges seen in the same
   call: the later button in SW2..SW5 order wins, the others are only tracked). */
app_button_event_t app_buttons_poll(uint8_t pressed, uint32_t now_ms);

/* Right-aligned decimal in width columns (leading spaces; truncated to the low digits). */
void app_digits(char *out, unsigned width, uint32_t value);
/* "S2 DINO  176  k4": slot, object tag, records (right-aligned in columns 7..11), selected
   count in columns 14..15 ("k4"; 10..12 two digits). */
void app_slot_notice(char line[17], uint8_t slot, uint8_t object, uint16_t count, uint8_t selected);

/* SW2: "# PNET1,START", pneu_model_tune() (blocks), "# PNET1,<PST1 TUNE fields>".
   Instance 0 must be retrained afterwards. */
void app_run_tune(void);
/* SW3: switch to the next COMMITTED slot with >= 4 records after the active one.
   true = switched, line = "S1 KRAG  141    " (retrain now); false = line is a 2 s notice
   ("SLOT 2 ONLY     " or "STORE ERROR"). UART "# PNES1,..." either way. */
bool app_next_slot(char line[17]);
#endif
