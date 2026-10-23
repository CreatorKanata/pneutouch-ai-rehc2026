#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pressure_display.h"
static uint32_t now;
static uint8_t *pending;
static unsigned size, writes;
static int completion = 1;
static char displayed[17], displayed2[17];
static uint8_t address;          /* last DDRAM address command */
static char order[8]; static unsigned order_len; /* '1'/'2': which line each text write hit */
uint32_t board_millis(void) { return now; }
void board_lcd_init(void) { pending = NULL; writes = 0; }
void board_lcd_start(uint8_t *b, uint16_t n) { assert(!pending); pending = b; size = n; ++writes; }
int board_lcd_status(void)
{
    assert(pending);
    if (!completion) return 0;
    if (completion > 0 && pending[0] == 0 && size == 2 && (pending[1] == 0x80 || pending[1] == 0xc0)) address = pending[1];
    if (completion > 0 && pending[0] == 0x40) {
        char *target = address == 0xc0 ? displayed2 : displayed;
        assert(size == 17 && (address == 0x80 || address == 0xc0));
        memcpy(target, pending+1, 16); target[16] = 0;
        if (order_len < sizeof(order)) order[order_len++] = address == 0xc0 ? '2' : '1';
        address = 0;
    }
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
    /* Line 2: address 0xC0 then 16 bytes; it does not count as a result update. */
    unsigned updates = pneutouch_display.updates, before2 = writes;
    pneu_display_line2("\xff\xff  8"); advance(5);
    assert(writes == before2+2 && !strcmp(displayed2, "\xff\xff  8           ") && !strcmp(displayed, "READY           "));
    assert(pneutouch_display.updates == updates);
    /* Both pending: line 1 first, then line 2; the 3-second hold is unaffected. */
    order_len = 0;
    pneu_display_line2("LINE TWO"); pneu_display_result(PNEU_CLASS_LEGS); advance(10);
    assert(order_len == 2 && order[0] == '1' && order[1] == '2');
    assert(!strcmp(displayed, "LEGS            ") && !strcmp(displayed2, "LINE TWO        "));
    uint32_t legs_shown = pneutouch_display.shown_ms;
    pneu_display_line2("LEARN1  123/ 704"); advance(10); /* line 2 update during the hold */
    assert(!strcmp(displayed2, "LEARN1  123/ 704") && pneutouch_display.label == PNEU_CLASS_LEGS);
    advance(3000U-(now-legs_shown)+5U);
    assert(pneutouch_display.label == PNEU_CLASS_UNKNOWN && !strcmp(displayed, "READY           "));
    /* A result arriving while line 2 is being written still shows next. */
    order_len = 0;
    pneu_display_line2("X"); advance(1); pneu_display_result(PNEU_CLASS_HEAD); advance(10);
    assert(order_len == 2 && order[0] == '2' && order[1] == '1' && !strcmp(displayed, "HEAD            "));
    advance(3100);
    now = UINT32_MAX-1000U;
    pneu_display_result(PNEU_CLASS_BACK); advance(5); advance(2000);
    pneu_display_result(PNEU_CLASS_HEAD); advance(5); advance(2000);
    assert(pneutouch_display.label == PNEU_CLASS_HEAD); /* replacement restarts timer across wrap */
    advance(1005); assert(pneutouch_display.label == PNEU_CLASS_UNKNOWN);
    pneu_display_result(PNEU_CLASS_TAIL); completion = 0; advance(10);
    assert(!pneutouch_display.failed); /* peripheral remains asynchronous */
    completion = -1; advance(1); assert(pneutouch_display.failed);
    unsigned before = writes; advance(5000); assert(writes == before);
    puts("LCD: initialization ACK, all labels, 3-second hold, line 2, replacement, wrap, I2C fault PASS");
}
