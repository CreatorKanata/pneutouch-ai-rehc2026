/* Protocol/control test. Model quality is checked separately on the real chip. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ai_validation.h"
#include "pressure_model.h"
#include "model_store.h"
#include "solistAi.h"
#include "fram_stub.h"

static char rx[800], tx[4096];
static unsigned rx_pos, tx_pos, now, trained, predicted, beta_rows, p_rows;
static ODL_Parameters parameters;
static uint32_t busy;
static unsigned live_predictions, model_inits;
volatile pneu_model_status_t pneutouch_model;
bool pneu_model_init(void)
{
    ++model_inits;
    pneutouch_model.ready = true; pneutouch_model.error = 0; pneutouch_model.store_count = 8;
    pneutouch_model.selected_count = 3;
    pneutouch_model.selected[0] = 0; pneutouch_model.selected[1] = 4; pneutouch_model.selected[2] = 10;
    pneutouch_model.training_us = 1234;
    return true;
}
pneu_demo_class_t pneu_model_predict(const float values[12], float peak, float trough)
{
    assert(peak == 1.0f);
    assert(trough == 1.0f); /* peak * values[10] */
    for (unsigned i = 0; i < 12; ++i) assert(values[i] == 1.0f);
    ++live_predictions;
    return PNEU_CLASS_HEAD;
}
uint32_t board_millis(void) { return now; }
uint32_t board_micros(void) { return now*1000U; }
int board_getc(void) { return rx[rx_pos] ? (unsigned char)rx[rx_pos++] : -1; }
void board_putc(char c) { assert(tx_pos+1 < sizeof(tx)); tx[tx_pos++] = c; tx[tx_pos] = 0; }
void smpl_enablePeripheral(unsigned mask) { assert(mask == 0x40000000U); }
void ODL_Initialize(uint8_t model, const ODL_Parameters *p)
{
    assert(model == 0); parameters = *p; beta_rows = p_rows = 0;
    assert(p->outputSize == 4 && p->scaleGamma == 0 && p->leakRate == 0x3f80);
}
void ODL_Reset(uint8_t model) { assert(model == 0); }
void ODL_SetWeightBeta(const void *values, uint8_t model, uint32_t offset, uint16_t size)
{
    const bfloat16 *v = values;
    assert(model == 0 && size == 8 && offset == beta_rows*8);
    for (unsigned i = 0; i < 4; ++i) assert(v[i] == 0);
    ++beta_rows;
}
void ODL_SetWeightP(const void *values, uint8_t model, uint32_t offset, uint16_t size)
{
    const bfloat16 *v = values;
    assert(model == 0 && size == parameters.hiddenSize*2 && offset == p_rows*size);
    for (unsigned i = 0; i < parameters.hiddenSize; ++i)
        assert(v[i] == (i == p_rows ? 0x3f80 : 0));
    ++p_rows;
}
void ODL_StartTrain(uint8_t model, const bfloat16 *x, const bfloat16 *t)
{
    assert(model == 0 && beta_rows == 32 && p_rows == 32);
    assert(x[0] == 0x3f80 && x[11] == 0);
    assert(t[0] == 0x3f80 && t[1] == 0 && t[2] == 0 && t[3] == 0);
    ++trained;
}
void ODL_StartPredict(uint8_t model, const bfloat16 *x, const bfloat16 *t)
{
    assert(model == 0 && x[0] == 0x3f80);
    for (unsigned i = 0; i < 4; ++i) assert(t[i] == 0); /* NO teacher leakage */
    ++predicted;
}
uint32_t ODL_IsBusy(void) { return busy; }
void ODL_GetResult(uint8_t model, bfloat16 *y)
{
    assert(model == 0); y[0] = 0; y[1] = 0x3f80; y[2] = y[3] = 0;
}
static void send(const char *packet)
{
    assert(strlen(packet) < sizeof(rx)); strcpy(rx, packet); rx_pos = tx_pos = 0; tx[0] = 0;
    do { (void)pneu_ai_validation_poll(); } while (rx[rx_pos]);
    (void)pneu_ai_validation_poll();
}
static void packet(const char *body)
{
    uint16_t crc = 0xffff;
    char wire[800];
    for (const char *p = body; *p; ++p) {
        crc ^= (uint16_t)(unsigned char)*p << 8;
        for (unsigned i = 0; i < 8; ++i)
            crc = (uint16_t)(crc & 0x8000 ? (crc << 1)^0x1021 : crc << 1);
    }
    snprintf(wire, sizeof(wire), "%s*%04x\n", body, crc); send(wire);
}
static void pool_hex(char *out, float first)
{
    for (unsigned j = 0; j < PNEU_POOL_COUNT; ++j) {
        float v = first+(float)j*0.25f;
        uint32_t bits;
        memcpy(&bits, &v, sizeof(bits));
        sprintf(out+8*j, "%08x", (unsigned)bits);
    }
}
static void put(const char *id, unsigned slot, unsigned label, float first, const char *expected)
{
    char body[300], hex[PNEU_POOL_COUNT*8+1];
    pool_hex(hex, first);
    snprintf(body, sizeof(body), "PST1,PUT,%s,%u,%u,%s", id, slot, label, hex);
    packet(body); assert(strstr(tx, expected));
}
static void test_store_errors(void)
{
    char body[300], hex[PNEU_POOL_COUNT*8+1];
    pool_hex(hex, 1.0f);
    packet("PST1,OPEN,20,0,0,dino"); assert(!strcmp(tx, "# PST1,OPEN,20,0\r\n"));
    packet("PST1,SLOT,21,0"); assert(!strcmp(tx, "# PST1,SLOT,21,0,1,0,14,0,dino\r\n"));
    put("22", 0, 1, 1.0f, "# PST1,PUT,22,0,1\r\n");
    put("23", 0, 5, 1.0f, "# PST1,ERROR,LABEL"); put("23", 4, 1, 1.0f, "# PST1,ERROR,SLOT");
    put("23", 1, 1, 1.0f, "# PST1,ERROR,STATE"); put("23", 0, 1, -1.0f, "# PST1,ERROR,RANGE");
    put("23", 0, 1, 0.0f, "# PST1,ERROR,RANGE"); /* first value exactly 0 */
    memcpy(hex+8, "7f800000", 8);
    snprintf(body, sizeof(body), "PST1,PUT,24,0,1,%s", hex); packet(body); assert(strstr(tx, "ERROR,NONFINITE"));
    hex[3] = 'z';
    snprintf(body, sizeof(body), "PST1,PUT,24,0,1,%s", hex); packet(body); assert(strstr(tx, "ERROR,INPUT_HEX"));
    packet("PST1,PUT,24,0,1,3f800000"); assert(strstr(tx, "ERROR,INPUT_SIZE"));
    packet("PST1,PUT,24,0,1"); assert(strstr(tx, "ERROR,FIELDS"));
    packet("PST1,COMMIT,25,0"); assert(strstr(tx, "ERROR,RANGE")); /* only 1 record */
    packet("PST1,OPEN,26,0,13,x"); assert(strstr(tx, "ERROR,RANGE"));
    packet("PST1,OPEN,26,0,0,ninechars"); assert(strstr(tx, "ERROR,RANGE"));
    packet("PST1,OPEN,26,9,0,x"); assert(strstr(tx, "ERROR,SLOT"));
    packet("PST1,BOGUS,27"); assert(strstr(tx, "ERROR,COMMAND"));
    packet("PST1,INFO,x"); assert(strstr(tx, "ERROR,FIELDS"));
    packet("PST1,INFO,28,1"); assert(strstr(tx, "ERROR,FIELDS"));
}
static void test_store_protocol(void)
{
    float pool[PNEU_POOL_COUNT];
    memset(fram, 0xff, sizeof(fram));
    packet("PST1,INFO,1"); assert(!strcmp(tx, "# PST1,INFO,1,0,0,0,0,0\r\n")); /* works in MODE 0 */
    packet("PST1,SELFTEST,2"); assert(!strcmp(tx, "# PST1,SELFTEST,2,0\r\n"));
    fram_selftest_result = -1; packet("PST1,SELFTEST,3"); assert(!strcmp(tx, "# PST1,SELFTEST,3,-1\r\n"));
    fram_selftest_result = 7; packet("PST1,SELFTEST,3"); assert(!strcmp(tx, "# PST1,SELFTEST,3,7\r\n"));
    const char *const guarded[] = {"PST1,OPEN,4,0,0,x", "PST1,PUT,4,0,1,00", "PST1,COMMIT,4,0", "PST1,GET,4,0,0",
                                   "PST1,ERASE,4,0", "PST1,USE,4,0", "PST1,TRAIN,4"};
    for (unsigned i = 0; i < 7; ++i) { packet(guarded[i]); assert(!strcmp(tx, "# PST1,ERROR,MODE_REQUIRED\r\n")); }
    assert(model_inits == 1 && fram[0x2000] == 0xff); /* nothing written, TRAIN not run (1 = LIVEINIT) */
    packet("PAI1,MODE,1");
    assert(pneu_store_init());
    test_store_errors();
    put("30", 0, 2, 2.0f, ",PUT,30,0,2\r\n"); put("31", 0, 3, 3.0f, ",PUT,31,0,3\r\n");
    put("32", 0, 4, 4.0f, ",PUT,32,0,4\r\n");
    packet("PST1,COMMIT,33,0"); assert(!strcmp(tx, "# PST1,COMMIT,33,0,4\r\n"));
    put("34", 0, 1, 1.0f, "ERROR,STATE");
    {
        char expected[200], hex[PNEU_POOL_COUNT*8+1];
        pool_hex(hex, 3.0f);
        snprintf(expected, sizeof(expected), "# PST1,GET,35,0,2,3,%s\r\n", hex);
        packet("PST1,GET,35,0,2"); assert(!strcmp(tx, expected));
    }
    packet("PST1,GET,36,0,4"); assert(strstr(tx, "ERROR,INDEX"));
    packet("PST1,GET,36,1,0"); assert(strstr(tx, "ERROR,STATE"));
    fram[0x2000+32+60+5] ^= 1; packet("PST1,GET,37,0,1"); assert(strstr(tx, "ERROR,CRC_RECORD"));
    fram[0x2000+32+60+5] ^= 1;
    packet("PST1,USE,38,1"); assert(!strcmp(tx, "# PST1,USE,38,1\r\n"));
    packet("PST1,INFO,39"); assert(!strcmp(tx, "# PST1,INFO,39,1,4,0,0,0\r\n"));
    packet("PST1,USE,38,7"); assert(strstr(tx, "ERROR,SLOT"));
    /* Full slot: fill through the store API, then one more over the wire. */
    assert(pneu_store_open(2, 0, "full"));
    for (unsigned i = 0; i < PNEU_POOL_COUNT; ++i) pool[i] = 1.0f;
    for (unsigned i = 0; i < PNEU_STORE_MAX_RECORDS; ++i) assert(pneu_store_put(2, 1, pool));
    put("40", 2, 1, 1.0f, "# PST1,ERROR,FULL");
    packet("PST1,ERASE,41,0"); assert(!strcmp(tx, "# PST1,ERASE,41,0\r\n"));
    packet("PST1,INFO,42"); assert(!strcmp(tx, "# PST1,INFO,42,1,0,0,1000,0\r\n"));
    fram_drop_writes = true; packet("PST1,ERASE,43,3"); assert(strstr(tx, "ERROR,STORE")); fram_drop_writes = false;
    packet("PST1,TRAIN,44"); assert(!strcmp(tx, "# PST1,TRAIN,44,1,0,8,3,0:4:10,1234\r\n"));
    /* TRAIN re-initialized the chip: PAI1 INIT is required again, LIVEPREDICT is ready. */
    packet("PAI1,TRAIN,45,1,3f8000000000000000000000000000000000000000000000"); assert(strstr(tx, "INIT_REQUIRED"));
    {
        char body[180] = "PAI1,LIVEPREDICT,49,";
        for (unsigned i = 0; i < 13; ++i) strcat(body, "3f800000");
        packet(body); assert(strstr(tx, "# PAI1,LIVEPREDICT,49,4,4,0,"));
    }
    busy = 1;
    packet("PAI1,INIT,46,12,32,1,1"); packet("PAI1,PREDICT,47,3f8000000000000000000000000000000000000000000000");
    packet("PST1,INFO,48"); assert(!strcmp(tx, "# PST1,ERROR,BUSY\r\n"));
    busy = 0; (void)pneu_ai_validation_poll();
    packet("PAI1,MODE,0");
}
int main(void)
{
    pneu_ai_validation_init();
    send("PAI1,MODE,1*0000\n"); assert(strstr(tx, "ERROR,CRC"));
    send("PAI1,MODE,1*1f24\n"); assert(strstr(tx, "MODE,1"));
    send("PAI1,INIT,1,12,32,1,1*9bc6\n"); assert(strstr(tx, "INIT,1,12,32,4"));
    assert(parameters.inputSize == 12 && p_rows == 32 && beta_rows == 32);
    send("PAI1,TRAIN,2,1,3f8000000000000000000000000000000000000000000000*4976\n");
    assert(trained == 1 && strstr(tx, "TRAIN,2,"));
    packet("PAI1,PREDICT,3,3f8000000000000000000000000000000000000000000000");
    assert(predicted == 1 && strstr(tx, ",0000,3f80,0000,0000"));
    packet("PAI1,TRAIN,4,1,7f8000000000000000000000000000000000000000000000");
    assert(trained == 1 && strstr(tx, "NONFINITE"));
    packet("PAI1,TRAIN,5,4,3f80"); assert(trained == 1 && strstr(tx, "INPUT_SIZE"));
    busy = 1;
    packet("PAI1,PREDICT,6,3f8000000000000000000000000000000000000000000000");
    now = 4001; (void)pneu_ai_validation_poll(); assert(strstr(tx, "AI_TIMEOUT"));
    busy = 0;
    now = 60001; assert(!pneu_ai_validation_poll()); assert(strstr(tx, "IDLE_TIMEOUT"));
    {
        char body[180] = "PAI1,LIVEPREDICT,11,";
        for (unsigned i = 0; i < 13; ++i) strcat(body, "3f800000");
        packet(body); assert(strstr(tx, "MODE_REQUIRED") && live_predictions == 0);
        packet("PAI1,MODE,1");
        packet(body); assert(strstr(tx, "LIVE_INIT_REQUIRED") && live_predictions == 0);
        packet("PAI1,LIVEINIT,10"); assert(strstr(tx, "LIVEINIT,10"));
        packet(body); assert(strstr(tx, "LIVEPREDICT,11,4,4,0,") && live_predictions == 1);
        memcpy(body+strlen("PAI1,LIVEPREDICT,11,"), "7f800000", 8);
        packet(body); assert(strstr(tx, "NONFINITE") && live_predictions == 1);
        packet("PAI1,MODE,0"); assert(!pneu_ai_validation_poll());
    }
    test_store_protocol();
    puts("AI protocol: CRC, explicit reset, one-hot teacher, prediction isolation, timeout, PST1 store commands PASS");
    return 0;
}
