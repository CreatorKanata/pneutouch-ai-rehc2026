/* Nonblocking LCD initialization and writes, using the supplied I2CF0 driver.
   A result's 3000 ms starts when its complete text has been acknowledged. */
#include "pressure_display.h"
#include "pressure_model.h"
#include "board.h"
#include <string.h>

volatile pneu_display_status_t pneutouch_display;
static const uint8_t init_commands[] = {0x38,0x39,0x14,0x73,0x5e,0x6c,0x0c,0x38,0x01,0x06};
static uint8_t buffer[17], init_index, phase;
static uint32_t since, delay_ms;
static bool busy, dirty, holding;
static char desired[17], idle[17];
static pneu_demo_class_t desired_label, sent_label;

static void line(char out[17], const char *s)
{
    unsigned n = 0;
    while (n < 16 && s[n]) { out[n] = s[n]; ++n; }
    while (n < 16) out[n++] = ' ';
    out[16] = 0;
}
void pneu_display_init(void)
{
    pneutouch_display = (pneu_display_status_t){0};
    board_lcd_init();
    init_index = phase = 0; busy = holding = false; dirty = true;
    desired_label = sent_label = PNEU_CLASS_UNKNOWN;
    line(idle, "READY"); line(desired, "READY");
    since = board_millis(); delay_ms = 100;
}
void pneu_display_idle(const char *message)
{
    line(idle, message); memcpy(desired, idle, sizeof(desired));
    desired_label = PNEU_CLASS_UNKNOWN; holding = false; dirty = true;
}
void pneu_display_result(pneu_demo_class_t label)
{
    if (label < PNEU_CLASS_TAIL || label > PNEU_CLASS_HEAD) return;
    line(desired, pneu_class_name(label)); desired_label = label;
    dirty = true; holding = false;
}
void pneu_display_poll(void)
{
    uint32_t now = board_millis();
    if (pneutouch_display.failed) return;
    if (busy) {
        int result = board_lcd_status();
        if (!result) return;
        busy = false;
        if (result < 0) {
            pneutouch_display.failed = true; pneutouch_display.ready = false;
            return;
        }
        since = now;
        if (init_index < sizeof(init_commands)) {
            delay_ms = init_index == 5 ? 200U : (init_index == 8 ? 5U : 1U);
            ++init_index;
        } else {
            delay_ms = 1;
            if (phase == 1) phase = 2; /* Address set; send text next. */
            else if (phase == 3) {
                phase = 0; ++pneutouch_display.updates;
                pneutouch_display.label = sent_label;
                if (sent_label != PNEU_CLASS_UNKNOWN) {
                    pneutouch_display.shown_ms = now;
                    holding = !dirty;
                } else {
                    pneutouch_display.cleared_ms = now;
                    holding = false;
                }
            }
        }
    }
    if (holding && now-pneutouch_display.shown_ms >= 3000U) {
        holding = false; memcpy(desired, idle, sizeof(desired));
        desired_label = PNEU_CLASS_UNKNOWN; dirty = true;
    }
    if (now-since < delay_ms) return;
    if (init_index < sizeof(init_commands)) {
        buffer[0] = 0; buffer[1] = init_commands[init_index];
        board_lcd_start(buffer, 2); busy = true;
    } else {
        pneutouch_display.ready = true;
        if (phase == 2) {
            buffer[0] = 0x40; memcpy(buffer+1, desired, 16);
            sent_label = desired_label; dirty = false; phase = 3;
            board_lcd_start(buffer, 17); busy = true;
        } else if (!phase && dirty) {
            buffer[0] = 0; buffer[1] = 0x80; phase = 1;
            board_lcd_start(buffer, 2); busy = true;
        }
    }
}
