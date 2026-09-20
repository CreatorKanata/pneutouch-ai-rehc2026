#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "pressure_model.h"
#include "pressure_model_data.h"
#include "solistAi.h"
static unsigned trained, predicted, now, winner;
static bool stalled, nonfinite;
uint32_t board_micros(void) { return now++; }
void board_watchdog(void) {}
void smpl_enablePeripheral(unsigned mask) { assert(mask == 0x40000000U); }
void ODL_Initialize(uint8_t model, const ODL_Parameters *p)
{
    assert(model == 0 && p->inputSize == 12 && p->hiddenSize == PNEU_MODEL_HIDDEN && p->outputSize == 4);
    assert(p->seed == 1 && p->activationFunction == 1 && p->scaleGamma == 0);
    trained = 0;
}
void ODL_Reset(uint8_t model) { assert(model == 0); }
void ODL_SetWeightBeta(const void *v, uint8_t model, uint32_t off, uint16_t size)
{
    assert(model == 0 && off < PNEU_MODEL_HIDDEN*8U && size == 8);
    for (unsigned i = 0; i < 4; ++i) assert(((const bfloat16 *)v)[i] == 0);
}
void ODL_SetWeightP(const void *v, uint8_t model, uint32_t off, uint16_t size)
{
    assert(model == 0 && size == PNEU_MODEL_HIDDEN*2U);
    for (unsigned i = 0; i < PNEU_MODEL_HIDDEN; ++i)
        assert(((const bfloat16 *)v)[i] == (i == off/size ? 0x3f80 : 0));
}
void ODL_StartTrain(uint8_t model, const bfloat16 *x, const bfloat16 *t)
{
    unsigned index = model_order[trained++];
    assert(model == 0 && !memcmp(x, model_training[index], 24));
    for (unsigned i = 0; i < 4; ++i) assert(t[i] == (i+1 == model_labels[index] ? 0x3f80 : 0));
}
void ODL_StartPredict(uint8_t model, const bfloat16 *x, const bfloat16 *t)
{
    assert(model == 0 && x != NULL);
    for (unsigned i = 0; i < 4; ++i) assert(t[i] == 0);
    ++predicted;
}
uint32_t ODL_IsBusy(void) { return stalled; }
void ODL_GetResult(uint8_t model, bfloat16 *y)
{
    assert(model == 0);
    for (unsigned i = 0; i < 4; ++i) y[i] = (bfloat16)(i == winner ? 0xbf00 : 0xbf80);
    if (nonfinite) y[0] = 0x7fc0;
}
int main(void)
{
    float x[12];
    uint16_t words[12];
    memcpy(x, model_reference_raw, sizeof(x));
    assert(pneu_model_transform(x, words));
    assert(!memcmp(words, model_training[0], 24));
    assert(pneu_model_init() && trained == PNEU_MODEL_STEPS && pneutouch_model.ready);
    for (winner = 0; winner < 4; ++winner)
        assert(pneu_model_predict(x) == (pneu_demo_class_t)(winner+1));
    assert(predicted == 4);
    x[0] = NAN; assert(pneu_model_predict(x) == PNEU_CLASS_UNKNOWN && predicted == 4);
    x[0] = 0; assert(!pneu_model_transform(x, words));
    x[0] = INFINITY; assert(!pneu_model_transform(x, words));
    x[0] = 80; nonfinite = true;
    assert(pneu_model_predict(x) == PNEU_CLASS_UNKNOWN);
    nonfinite = false; stalled = true;
    assert(pneu_model_predict(x) == PNEU_CLASS_UNKNOWN && !pneutouch_model.ready);
    assert(!pneu_model_init());
    assert(!strcmp(pneu_class_name(PNEU_CLASS_HEAD), "HEAD"));
    puts("Pressure model: boot training, preprocessing, all four outputs, no teacher leakage, timeout PASS");
}
