#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pressure_display.h"
static uint32_t now;
static uint8_t *pending;
static unsigned size, writes;
static int completion = 1;
static char displayed[17];
uint32_t board_millis(void) { return now; }
void board_lcd_init(void) { pending = NULL; writes = 0; }
void board_lcd_start(uint8_t *b, uint16_t n) { assert(!pending); pending = b; size = n; ++writes; }
int board_lcd_status(void)
{
    assert(pending);
    if (!completion) return 0;
    if (completion > 0 && pending[0] == 0x40) { assert(size == 17); memcpy(displayed, pending+1, 16); displayed[16] = 0; }
    pending = NULL; return completion;
}
const char *pneu_class_name(pneu_demo_class_t label)
{
    const char *names[] = {"UNKNOWN","TAIL","BACK","LEGS","HEAD"}; return names[label];
}
static void advance(unsigned ms) { while (ms--) { ++now; pneu_display_poll(); } }
int main(void)
{
    pneu_display_init(); advance(350);
    assert(pneutouch_display.ready && !pneutouch_display.failed && writes == 12);
    assert(!strcmp(displayed, "READY           "));
    for (unsigned label = 1; label <= 4; ++label) {
        pneu_display_result((pneu_demo_class_t)label); advance(5);
        assert(pneutouch_display.label == (pneu_demo_class_t)label);
        uint32_t shown = pneutouch_display.shown_ms;
        advance(2999U-(now-shown));
        assert(pneutouch_display.label == (pneu_demo_class_t)label);
        advance(5);
        assert(pneutouch_display.label == PNEU_CLASS_UNKNOWN);
        assert(pneutouch_display.cleared_ms-shown >= 3000U && pneutouch_display.cleared_ms-shown <= 3005U);
    }
    now = UINT32_MAX-1000U;
    pneu_display_result(PNEU_CLASS_BACK); advance(5); advance(2000);
    pneu_display_result(PNEU_CLASS_HEAD); advance(5); advance(2000);
    assert(pneutouch_display.label == PNEU_CLASS_HEAD); /* replacement restarts timer across wrap */
    advance(1005); assert(pneutouch_display.label == PNEU_CLASS_UNKNOWN);
    pneu_display_result(PNEU_CLASS_TAIL); completion = 0; advance(10);
    assert(!pneutouch_display.failed); /* peripheral remains asynchronous */
    completion = -1; advance(1); assert(pneutouch_display.failed);
    unsigned before = writes; advance(5000); assert(writes == before);
    puts("LCD: initialization ACK, all labels, 3-second hold, replacement, wrap, I2C fault PASS");
}
