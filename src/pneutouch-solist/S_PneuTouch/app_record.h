/* Recording training data with the board buttons, no PC (docs/model-store.md,
   "ボタン操作"): every completed press event while recording is appended to the active
   FeRAM slot with the chosen label. pressure_app.c owns the LCD timing and retraining. */
#ifndef PNEU_APP_RECORD_H
#define PNEU_APP_RECORD_H
#include <stdbool.h>
#include <stdint.h>
#include "app_buttons.h"

/* label 1..classes of object (pneu_object_t of the slot, from its name; DINO TAIL..HEAD,
   KRAG HAT/FACE); added = records put since the start (undo stops at 0);
   count = records in the slot; class_count per label-1. fresh = the slot was opened
   empty by this session (prev_active: the active slot before it); reopened = it was
   COMMITTED before the session. */
typedef struct {
    bool active, fresh, reopened;
    uint8_t slot, label, prev_active, object;
    uint16_t added, count;
    uint16_t class_count[4];
} app_record_t;
extern volatile app_record_t app_record; /* for the debugger and tests */

/* What pressure_app.c does after a recording button. */
typedef enum {
    APP_REC_NONE,    /* nothing (gesture ignored in this state) */
    APP_REC_IDLE,    /* show the idle line (app_record_line while recording) */
    APP_REC_NOTICE,  /* notice for APP_NOTICE_MS */
    APP_REC_BRIEF,   /* notice for APP_BRIEF_MS */
    APP_REC_RETRAIN, /* recording committed: retrain and restart acquisition */
    APP_REC_DISCARD  /* discarded and restored: retrain, then the notice (APP_NOTICE_MS) */
} app_record_action_t;

/* Boot, before training: stops any recording, label TAIL; an OPEN active slot with
   >= 4 records (power lost while recording) is committed: "# PNER1,AUTOCOMMIT,<slot>,<count>". */
void app_record_boot(void);
/* fresh: pneu_store_open(slot, 0, <tag of object>"BTN") + pneu_store_use(slot); else a
   COMMITTED slot is reopened (an OPEN one continues, an EMPTY one is opened like fresh) and
   keeps the object of its name. class_count is filled by one label pass.
   "# PNER1,START,<slot>,<count>,<fresh 0/1>". false: notice set. */
bool app_record_start(uint8_t slot, bool fresh, uint8_t object, char notice[17]);
/* First EMPTY slot; false when none. */
bool app_record_empty_slot(uint8_t *slot);
/* Same, but false also sets "NO EMPTY SLOT   " and logs "# PNER1,ERROR,NO_EMPTY_SLOT". */
bool app_record_need_empty(uint8_t *slot, char notice[17]);
/* Commit (count >= 4, else "NEED 4 RECORDS  " and still recording);
   "# PNER1,STOP,<slot>,<count>,<added>". true = retrain now. */
bool app_record_stop(char notice[17]);
/* One completed press event: put with the current label. true: notice "+HEAD 39" (brief;
   names padded to 4: "+HAT  3"). */
bool app_record_event(const float values[12], float peak, float trough, char notice[17]);
void app_record_next_label(void); /* TAIL -> BACK -> LEGS -> HEAD -> TAIL; HAT -> FACE -> HAT */
/* Removes the last record added since the start: true + "UNDO HEAD 38    ". */
bool app_record_undo(char notice[17]);
/* LCD idle text while recording: "REC HEAD  38/176", "REC HAT   12/ 40" (label count / total). */
void app_record_line(char line[17]);
/* SW4 long while recording: truncates to the count before the session; a fresh slot is
   erased and prev_active used again, a reopened one committed, an OPEN one left OPEN.
   "# PNER1,DISCARD,<slot>,<removed>", notice "DISCARDED       ". */
app_record_action_t app_record_discard(char notice[17]);
/* Entering PAI1/PST1 MODE 1: closes the SW4-long chooser; commit with >= 4 records
   ("# PNER1,STOP,...") or leave the slot OPEN ("# PNER1,ABORT,<slot>,<count>"); recording
   ends either way. No-op when idle. */
void app_record_abort(void);
/* SW4/SW5 events (others: APP_REC_NONE); notice filled for NOTICE/BRIEF. SW4 long outside
   recording opens the object chooser (app_chooser.h) when a slot is EMPTY. */
app_record_action_t app_record_button(app_button_event_t event, char notice[17]);
#endif
