/* Protocol/control test. Model quality is checked separately on the real chip. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ai_validation.h"
#include "solistAi.h"

static char rx[800], tx[4096];
static unsigned rx_pos, tx_pos, now, trained, predicted, beta_rows, p_rows;
static ODL_Parameters parameters;
static uint32_t busy;
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
    puts("AI protocol: CRC, explicit reset, one-hot teacher, prediction isolation, timeout PASS");
    return 0;
}
